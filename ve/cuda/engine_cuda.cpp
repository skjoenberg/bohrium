/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/

#include <vector>
#include <iostream>

#include <boost/algorithm/string/replace.hpp>
#include <iomanip>

#include <bh_instruction.hpp>
#include <bh_component.hpp>

#include "engine_cuda.hpp"

using namespace std;
namespace fs = boost::filesystem;

namespace bohrium {

EngineCUDA::EngineCUDA(const ConfigParser &config, jitk::Statistics &stat) :
    EngineGPU(config, stat),
    work_group_size_1dx(config.defaultGet<int>("work_group_size_1dx", 128)),
    work_group_size_2dx(config.defaultGet<int>("work_group_size_2dx", 32)),
    work_group_size_2dy(config.defaultGet<int>("work_group_size_2dy", 4)),
    work_group_size_3dx(config.defaultGet<int>("work_group_size_3dx", 32)),
    work_group_size_3dy(config.defaultGet<int>("work_group_size_3dy", 2)),
    work_group_size_3dz(config.defaultGet<int>("work_group_size_3dz", 2))
{
    int deviceCount = 0;
    CUresult err = cuInit(0);

    if (err == CUDA_SUCCESS)
        check_cuda_errors(cuDeviceGetCount(&deviceCount));

    if (deviceCount == 0) {
        throw runtime_error("Error: no devices supporting CUDA");
    }

    // get first CUDA device
    check_cuda_errors(cuDeviceGet(&device, 0));

    err = cuCtxCreate(&context, 0, device);
    if (err != CUDA_SUCCESS) {
        cuCtxDetach(context);
        throw runtime_error("Error initializing the CUDA context.");
    }

    // Let's make sure that the directories exist
    jitk::create_directories(tmp_src_dir);
    jitk::create_directories(tmp_bin_dir);
    if (not cache_bin_dir.empty()) {
        jitk::create_directories(cache_bin_dir);
    }

    // Write the compilation hash
    compilation_hash = util::hash(info());

    // Get the compiler command and replace {MAJOR} and {MINOR} with the SM versions
    string compiler_cmd = config.get<string>("compiler_cmd");

    int major = 0, minor = 0;
    check_cuda_errors(cuDeviceComputeCapability(&major, &minor, device));
    boost::replace_all(compiler_cmd, "{MAJOR}", std::to_string(major));
    boost::replace_all(compiler_cmd, "{MINOR}", std::to_string(minor));

    // Init the compiler
    compiler = jitk::Compiler(compiler_cmd, verbose, config.file_dir.string());
}

EngineCUDA::~EngineCUDA() {
    cuCtxDetach(context);

    // Move JIT kernels to the cache dir
    if (not cache_bin_dir.empty()) {
        try {
            for (const auto &kernel: _functions) {
                const fs::path src = tmp_bin_dir / jitk::hash_filename(compilation_hash, kernel.first, ".cubin");
                if (fs::exists(src)) {
                    const fs::path dst = cache_bin_dir / jitk::hash_filename(compilation_hash, kernel.first, ".cubin");
                    if (not fs::exists(dst)) {
                        fs::copy_file(src, dst);
                    }
                }
            }
        } catch (const boost::filesystem::filesystem_error &e) {
            cout << "Warning: couldn't write CUDA kernels to disk to " << cache_bin_dir
                 << ". " << e.what() << endl;
        }
    }

    // File clean up
    if (not verbose) {
        fs::remove_all(tmp_src_dir);
    }

    if (cache_file_max != -1 and not cache_bin_dir.empty()) {
        util::remove_old_files(cache_bin_dir, cache_file_max);
    }
}

pair<tuple<uint32_t, uint32_t, uint32_t>, tuple<uint32_t, uint32_t, uint32_t> >
EngineCUDA::NDRanges(const vector<uint64_t> &thread_stack) const {
    const auto &b = thread_stack;
    switch (b.size()) {
        case 1: {
            const auto gsize_and_lsize = jitk::work_ranges(work_group_size_1dx, b[0]);
            return make_pair(make_tuple(gsize_and_lsize.first, 1, 1), make_tuple(gsize_and_lsize.second, 1, 1));
        }
        case 2: {
            const auto gsize_and_lsize_x = jitk::work_ranges(work_group_size_2dx, b[0]);
            const auto gsize_and_lsize_y = jitk::work_ranges(work_group_size_2dy, b[1]);
            return make_pair(make_tuple(gsize_and_lsize_x.first, gsize_and_lsize_y.first, 1),
                             make_tuple(gsize_and_lsize_x.second, gsize_and_lsize_y.second, 1));
        }
        case 3: {
            const auto gsize_and_lsize_x = jitk::work_ranges(work_group_size_3dx, b[0]);
            const auto gsize_and_lsize_y = jitk::work_ranges(work_group_size_3dy, b[1]);
            const auto gsize_and_lsize_z = jitk::work_ranges(work_group_size_3dz, b[2]);
            return make_pair(make_tuple(gsize_and_lsize_x.first, gsize_and_lsize_y.first, gsize_and_lsize_z.first),
                             make_tuple(gsize_and_lsize_x.second, gsize_and_lsize_y.second, gsize_and_lsize_z.second));
        }
        default:
            throw runtime_error("NDRanges: maximum of three dimensions!");
    }
}

CUfunction EngineCUDA::getFunction(const string &source, const std::string &func_name) {
    uint64_t hash = util::hash(source);
    ++stat.kernel_cache_lookups;

    // Do we have the program already?
    if (_functions.find(hash) != _functions.end()) {
        return _functions.at(hash);
    }

    fs::path binfile = cache_bin_dir / jitk::hash_filename(compilation_hash, hash, ".cubin");

    // If the binary file of the kernel doesn't exist we create it
    if (verbose or cache_bin_dir.empty() or not fs::exists(binfile)) {
        ++stat.kernel_cache_misses;

        // We create the binary file in the tmp dir
        binfile = tmp_bin_dir / jitk::hash_filename(compilation_hash, hash, ".cubin");

        // Write the source file and compile it (reading from disk)
        // TODO: make nvcc read directly from stdin
        {
            std::string kernel_filename = jitk::hash_filename(compilation_hash, hash, ".cu");
            if (verbose) {
              stat.addKernel(kernel_filename);
            }
            fs::path srcfile = jitk::write_source2file(source, tmp_src_dir,
                                                       kernel_filename, verbose);
            compiler.compile(binfile.string(), srcfile.string());
        }
        /* else {
            // Pipe the source directly into the compiler thus no source file is written
            compiler.compile(binfile.string(), source.c_str(), source.size());
        }
       */
    }

    CUmodule module;
    CUresult err = cuModuleLoad(&module, binfile.string().c_str());
    if (err != CUDA_SUCCESS) {
        const char *err_name, *err_desc;
        cuGetErrorName(err, &err_name);
        cuGetErrorString(err, &err_desc);
        cout << "Error loading the module \"" << binfile.string()
             << "\", " << err_name << ": \"" << err_desc << "\"." << endl;
        cuCtxDetach(context);
        throw runtime_error("cuModuleLoad() failed");
    }

    CUfunction program;
    err = cuModuleGetFunction(&program, module, func_name.c_str());
    if (err != CUDA_SUCCESS) {
        const char *err_name, *err_desc;
        cuGetErrorName(err, &err_name);
        cuGetErrorString(err, &err_desc);
        cout << "Error getting kernel function 'execute' \"" << binfile.string()
             << "\", " << err_name << ": \"" << err_desc << "\"." << endl;
        throw runtime_error("cuModuleGetFunction() failed");
    }
    _functions[hash] = program;
    return program;
}

void EngineCUDA::writeKernel(const jitk::Block &block,
                             const jitk::SymbolTable &symbols,
                             const std::vector<uint64_t> &thread_stack,
                             uint64_t codegen_hash,
                             std::stringstream &ss) {
    // Write the need includes
    ss << "#include <kernel_dependencies/complex_cuda.h>\n";
    ss << "#include <kernel_dependencies/integer_operations.h>\n";
    if (symbols.useRandom()) { // Write the random function
        ss << "#include <kernel_dependencies/random123_cuda.h>\n";
    }
    ss << "\n";

    // Write the header of the execute function
    ss << "extern \"C\" __global__ void execute_" << codegen_hash;
    writeKernelFunctionArguments(symbols, ss, nullptr);
    ss << " {\n";

    // Write the IDs of the threaded blocks
    if (not thread_stack.empty()) {
        util::spaces(ss, 4);
        ss << "// The IDs of the threaded blocks: \n";
        for (unsigned int i=0; i < thread_stack.size(); ++i) {
            util::spaces(ss, 4);
            ss << "const " << writeType(bh_type::INT64) << " i" << i << " = " << writeThreadId(i) << "; "
               << "if (i" << i << " >= " << thread_stack[i] << ") { return; } // Prevent overflow\n";
        }
        ss << "\n";
    }
    writeLoopBlock(symbols, nullptr, block.getLoop(), thread_stack, true, ss);
    ss << "}\n\n";
}

void EngineCUDA::execute(const std::string &source,
                         uint64_t codegen_hash,
                         const std::vector<bh_base*> &non_temps,
                         const vector<uint64_t> &thread_stack,
                         const vector<const bh_view*> &offset_strides,
                         const vector<const bh_instruction*> &constants) {
    uint64_t hash = util::hash(source);
    std::string source_filename = jitk::hash_filename(compilation_hash, hash, ".cu");

    auto tcompile = chrono::steady_clock::now();
    string func_name; { stringstream t; t << "execute_" << codegen_hash; func_name = t.str(); }
    CUfunction program = getFunction(source, func_name);
    stat.time_compile += chrono::steady_clock::now() - tcompile;

    // Let's execute the CUDA kernel
    vector<void *> args;

    for (bh_base *base: non_temps) { // NB: the iteration order matters!
        args.push_back(getBuffer(base));
    }

    for (const bh_view *view: offset_strides) {
        args.push_back((void*)&view->start);
        for (int j=0; j<view->ndim; ++j) {
            args.push_back((void*)&view->stride[j]);
        }
    }

    auto exec_start = chrono::steady_clock::now();

    tuple<uint32_t, uint32_t, uint32_t> blocks, threads;
    tie(blocks, threads) = NDRanges(thread_stack);

    check_cuda_errors(cuLaunchKernel(program,
                                     get<0>(blocks), get<1>(blocks), get<2>(blocks),  // NxNxN blocks
                                     get<0>(threads), get<1>(threads), get<2>(threads),  // NxNxN threads
                                     0, 0, &args[0], 0));
    check_cuda_errors(cuCtxSynchronize());

    auto texec = chrono::steady_clock::now() - exec_start;
    stat.time_exec += texec;
    stat.time_per_kernel[source_filename].register_exec_time(texec);
}

void EngineCUDA::setConstructorFlag(std::vector<bh_instruction*> &instr_list) {
    jitk::util_set_constructor_flag(instr_list, buffers);
}

std::string EngineCUDA::info() const {

    char device_name[1000];
    cuDeviceGetName(device_name, 1000, device);
    int major = 0, minor = 0;
    check_cuda_errors(cuDeviceComputeCapability(&major, &minor, device));
    size_t totalGlobalMem;
    check_cuda_errors(cuDeviceTotalMem(&totalGlobalMem, device));

    stringstream ss;
    ss << "----" << "\n";
    ss << "CUDA:" << "\n";
    ss << "  Device: \"" << device_name << " (SM " << major << "." << minor << " compute capability)\"\n";
    ss << "  Memory: \"" <<totalGlobalMem / 1024 / 1024 << " MB\"\n";
    ss << "  JIT Command: \"" << compiler.cmd_template << "\"\n";
    return ss.str();
}

} // bohrium
