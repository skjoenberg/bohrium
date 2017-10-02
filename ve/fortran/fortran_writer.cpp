#include <string>
#include <sstream>
#include <jitk/base_db.hpp>
#include "fortran_writer.hpp"
#include "fortran_util.hpp"

using namespace bohrium;
using namespace jitk;
using namespace std;


string FortranWriter::write_array_index(const Scope &scope, const bh_view &view,
                                        int hidden_axis, const pair<int, int> axis_offset) {
    stringstream out;
    out << view.start + 1;
    if (not bh_is_scalar(&view)) { // NB: this optimization is required when reducing a vector to a scalar!
        for (int i = 0; i < view.ndim; ++i) {
            int t = i;
            if (i >= hidden_axis)
                ++t;
            if (view.stride[i] != 0) {
                if (axis_offset.first == t) {
                    out << " +(i" << t << "+(" << axis_offset.second << ")) ";
                } else {
                    out << " +i" << t;
                }
                if (view.stride[i] != 1) {
                    out << "*" << view.stride[i];
                }
            }
        }
    }
    return out.str();
}

string FortranWriter::write_array_subscription(const Scope &scope, const bh_view &view, bool ignore_declared_indexes,
                                               int hidden_axis, const pair<int, int> axis_offset) {
    stringstream out;
    assert(view.base != NULL); // Not a constant
    // Let's check if the index is already declared as a variable
    if (not ignore_declared_indexes) {
        if (scope.isIdxDeclared(view)) {
            out << "(";
            out << scope.getIdxName(view);
            out << ")";
            return out.str();
        }
    }
    out << "(";
    if (scope.strides_as_variables and scope.isArray(view) and scope.symbols.existOffsetStridesID(view)) {
        out << write_array_index_variables(scope, view, hidden_axis, axis_offset);
    } else {
        out << write_array_index(scope, view, hidden_axis, axis_offset);
    }
    out << ")";
    return out.str();
}

string FortranWriter::write_array_index_variables(const Scope &scope, const bh_view &view,
                                                  int hidden_axis, const pair<int, int> axis_offset) {
    // Write view.start using the offset-and-strides variable
    stringstream out;
    out << "vo" << scope.symbols.offsetStridesID(view);

    if (not bh_is_scalar(&view)) { // NB: this optimization is required when reducing a vector to a scalar!
        for (int i = 0; i < view.ndim; ++i) {
            int t = i;
            if (i >= hidden_axis) {
                ++t;
            }
            if (axis_offset.first == t) {
                out << " +(i" << t << "+(" << axis_offset.second << ")) ";
            } else {
                out << " +i" << t;
            }
            out << "*vs" << scope.symbols.offsetStridesID(view) << "_" << i;
        }
    }
    return out.str();
}

void FortranWriter::write_header(const SymbolTable &symbols, Scope &scope, const LoopB &block, const ConfigParser &config){
    if (not config.defaultGet<bool>("compiler_openmp", false)) {
        return;
    }
    const bool enable_simd = config.defaultGet<bool>("compiler_openmp_simd", false);

    // All reductions that can be handle directly be the OpenMP header e.g. reduction(+:var)
    vector<InstrPtr> openmp_reductions;

    bool for_loop = false;
    stringstream tmp;
    stringstream tmp_foot;
    // "OpenMP for" goes to the outermost loop
    if (block.rank == 0 and openmp_compatible(block)) {
        for_loop = true;
        tmp << " PARALLEL DO";
        tmp_foot << " PARALLEL DO";
        // Since we are doing parallel for, we should either do OpenMP reductions or protect the sweep instructions
        for (const InstrPtr &instr: block._sweeps) {
            assert(instr->operand.size() == 3);
            const bh_view &view = instr->operand[0];
            if (openmp_reduce_compatible(instr->opcode) and (scope.isScalarReplaced(view) or scope.isTmp(view.base))) {
                openmp_reductions.push_back(instr);
            } else if (openmp_atomic_compatible(instr->opcode)) {
                scope.insertOpenmpAtomic(view);
            } else {
                scope.insertOpenmpCritical(view);
            }
        }
    }

    // "OpenMP SIMD" goes to the innermost loop (which might also be the outermost loop)
    if (enable_simd and block.isInnermost() and simd_compatible(block, scope)) {
        tmp << " SIMD";
        tmp_foot << " SIMD";

        if (block.rank > 0) { //NB: avoid multiple reduction declarations
            for (const InstrPtr instr: block._sweeps) {
                openmp_reductions.push_back(instr);
            }
        }
    }

    //Let's write the OpenMP reductions
    for (const InstrPtr instr: openmp_reductions) {
        assert(instr->operand.size() == 3);
        tmp << " REDUCTION(" << openmp_reduce_symbol(instr->opcode) << ":";
        tmp << scope.getName(instr->operand[0]);
        tmp << ")";
    }

    if (for_loop) {
        tmp << " DEFAULT(PRIVATE) SHARED(";
        bh_base *b = symbols.getParams()[0];
        tmp << "a" << symbols.baseID(b);

        for (size_t i = 1; i < symbols.getParams().size(); ++i) {
            b = symbols.getParams()[i];
            tmp << ",a" << symbols.baseID(b);
        }
        tmp << ")";
    }


    const string tmp_str = tmp.str();
    if(not tmp_str.empty()) {
        ss << "!$OMP" << tmp_str;
        endl();
        footer.push(write_spaces(4 + block.rank*4) + "!$OMP END" + tmp_foot.str());
        spaces(4 + block.rank*4);
    }
}

void FortranWriter::loop_head_writer(const SymbolTable &symbols, Scope &scope, const LoopB &block, const ConfigParser &config,
                                     bool loop_is_peeled, const vector<const LoopB *> &threaded_blocks) {
    // Let's write the OpenMP loop header
    {
        int64_t for_loop_size = block.size;
        if (block._sweeps.size() > 0 and loop_is_peeled) // If the for-loop has been peeled, its size is one less
            --for_loop_size;
        // No need to parallel one-sized loops
        if (for_loop_size > 1) {
            write_header(symbols, scope, block, config);
        }
    }

    // Write the for-loop header
    string itername;
    {stringstream t; t << "i" << block.rank; itername = t.str();}
    ss << "DO " << itername;
    if (block._sweeps.size() > 0 and loop_is_peeled) // If the for-loop has been peeled, we should start at 1
        ss << "=1,";
    else
        ss << "=0,";
    ss << block.size-1 << "\n";
}
