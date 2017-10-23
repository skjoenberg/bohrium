#include <string>
#include <sstream>
#include <jitk/base_db.hpp>
#include "opencl_writer.hpp"

using namespace bohrium;
using namespace jitk;
using namespace std;

string OpenCLWriter::write_array_subscription(const Scope &scope, const bh_view &view, bool ignore_declared_indexes,
                                              int hidden_axis, const pair<int, int> axis_offset) {
    stringstream out;
    assert(view.base != NULL); // Not a constant
    // Let's check if the index is already declared as a variable
    if (not ignore_declared_indexes) {
        if (scope.isIdxDeclared(view)) {
            out << "[";
            out << scope.getIdxName(view);
            out << "]";
            return out.str();
        }
    }
    out << "[";
    if (scope.strides_as_var and scope.isArray(view) and scope.symbols.existOffsetStridesID(view)) {
        out << write_array_index_variables(scope, view, hidden_axis, axis_offset);
    } else {
        out << write_array_index(scope, view, hidden_axis, axis_offset);
    }
    out << "]";
    return out.str();
}

string OpenCLWriter::write_array_index(const Scope &scope, const bh_view &view,
                                       int hidden_axis, const pair<int, int> axis_offset) {
    stringstream out;
    out << view.start;
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

string OpenCLWriter::write_array_index_variables(const Scope &scope, const bh_view &view,
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

const char* write_type(bh_type dtype) {
    switch (dtype) {
        case bh_type::BOOL:       return "uchar";
        case bh_type::INT8:       return "char";
        case bh_type::INT16:      return "short";
        case bh_type::INT32:      return "int";
        case bh_type::INT64:      return "long";
        case bh_type::UINT8:      return "uchar";
        case bh_type::UINT16:     return "ushort";
        case bh_type::UINT32:     return "uint";
        case bh_type::UINT64:     return "ulong";
        case bh_type::FLOAT32:    return "float";
        case bh_type::FLOAT64:    return "double";
        case bh_type::COMPLEX64:  return "float2";
        case bh_type::COMPLEX128: return "double2";
        case bh_type::R123:       return "ulong2";
        default:
            std::cerr << "Unknown OpenCL type: " << bh_type_text(dtype) << std::endl;
            throw std::runtime_error("Unknown OpenCL type");
    }
}

void OpenCLWriter::loop_head_writer(const SymbolTable &symbols, Scope &scope, const LoopB &block, const ConfigParser &config,
                                    bool loop_is_peeled, const vector<const LoopB *> &threaded_blocks) {
    // Write the for-loop header
    spaces(4+block.rank*4);
    string itername;
    {stringstream t; t << "i" << block.rank; itername = t.str();}
    // Notice that we use find_if() with a lambda function since 'threaded_blocks' contains pointers not objects
    if (std::find_if(threaded_blocks.begin(), threaded_blocks.end(),
                     [&block](const LoopB* b){return *b == block;}) == threaded_blocks.end()) {
        ss << "for(" << write_type(bh_type::UINT64) << " " << itername;
        if (block._sweeps.size() > 0 and loop_is_peeled) // If the for-loop has been peeled, we should start at 1
            ss << "=1; ";
        else
            ss << "=0; ";
        ss << itername << " < " << block.size << "; ++" << itername << ") {\n";
    } else {
        assert(block._sweeps.size() == 0);
        ss << "{ // Threaded block (ID " << itername << ")\n";
    }
}
