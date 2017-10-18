#ifndef BOHRIUM_WRITER_OPENCL_HPP
#define BOHRIUM_WRITER_OPENCL_HPP

#include <string>
#include <sstream>
#include <jitk/writer.hpp>
#include "opencl_instruction.hpp"


using namespace std;
using namespace bohrium;
using namespace jitk;

class OpenCLWriter : public Writer {
public:
    std::stringstream ss;

    OpenCLWriter() {}
    ~OpenCLWriter() {}

    void write(const std::string s) {
        ss << s;
    }

    void declare(const bh_view &view, const string &type_str, const int spacing, Scope* scope) {
        spaces(spacing);
        (*scope).writeDeclaration(view, type_str, ss);
        endl();
    }

    void declareIdx(const bh_view &view, const std::string &type_str, const int spacing, Scope* scope) {
        spaces(spacing);
        (*scope).writeIdxDeclaration(view, type_str, ss);
        endl();
    }

    void spaces(const int spacing) {
        for (int i=0; i < spacing; i++) {
            ss << " ";
        }
    }

    void comment(const string s) {
        ss << "// " << s;
    }

    void endl() {
        ss << ";\n";
    }

    void end_loop() {
        ss << "}\n";
    }

    string write_array_index(const Scope &scope, const bh_view &view,
                             int hidden_axis = BH_MAXDIM, const pair<int, int> axis_offset = std::make_pair(BH_MAXDIM, 0));

    string write_array_index_variables(const Scope &scope, const bh_view &view,
                                       int hidden_axis = BH_MAXDIM, const pair<int, int> axis_offset = std::make_pair(BH_MAXDIM, 0));

    string write_array_subscription(const Scope &scope, const bh_view &view, bool ignore_declared_indexes=false,
                                    int hidden_axis = BH_MAXDIM, const pair<int, int> axis_offset = std::make_pair(BH_MAXDIM, 0));

    void write_operation(const bh_instruction &instr, const vector<string> &ops) {
        ss << operation(instr, ops);
    }

    void write_instr(const Scope &scope, const bh_instruction &instr) {
        if (bh_opcode_is_system(instr.opcode))
            return;
        if (instr.opcode == BH_RANGE) {
            vector<string> ops;
            // Write output operand
            {
                stringstream out;
                out << scope.getName(instr.operand[0]);
                if (scope.isArray(instr.operand[0])) {
                    out << write_array_subscription(scope, instr.operand[0]);
                }
                ops.push_back(out.str());
            }
            // Let's find the flatten index of the output view
            {
                stringstream out;
                out << "(";
                for(int64_t i=0; i < instr.operand[0].ndim; ++i) {
                    out << "+i" << i << "*" << instr.operand[0].stride[i];
                }
                out << ")";
                ops.push_back(out.str());
            }
            write_operation(instr, ops);
            return;
        }
        if (instr.opcode == BH_RANDOM) {
            vector<string> ops;
            // Write output operand
            {
                stringstream out;
                out << scope.getName(instr.operand[0]);
                if (scope.isArray(instr.operand[0])) {
                    out << write_array_subscription(scope, instr.operand[0]);
                }
                ops.push_back(out.str());
            }
            // Write the random generation
            {
                stringstream out;
                out << "random123(" << instr.constant.value.r123.start \
               << ", " << instr.constant.value.r123.key << ", ";

                // Let's find the flatten index of the output view
                for(int64_t i=0; i < instr.operand[0].ndim; ++i) {
                    out << "+i" << i << "*" << instr.operand[0].stride[i];
                }
                out << ")";
                ops.push_back(out.str());
            }
            write_operation(instr, ops);
            return;
        }
        if (bh_opcode_is_accumulate(instr.opcode)) {
            vector<string> ops;
            // Write output operand
            {
                stringstream out;
                out << scope.getName(instr.operand[0]);
                if (scope.isArray(instr.operand[0])) {
                    out << write_array_subscription(scope, instr.operand[0]);
                }
                ops.push_back(out.str());
            }
            // Write the previous element access, NB: this works because of loop peeling
            {
                stringstream out;
                out << scope.getName(instr.operand[0]);
                out << write_array_subscription(scope, instr.operand[0], true, BH_MAXDIM, make_pair(instr.sweep_axis(), -1));
                ops.push_back(out.str());
            }
            // Write the current element access
            {
                stringstream out;
                out << scope.getName(instr.operand[1]);
                if (scope.isArray(instr.operand[1])) {
                    out << write_array_subscription(scope, instr.operand[1]);
                }
                ops.push_back(out.str());
            }
            write_operation(instr, ops);
            return;
        }
        if (instr.opcode == BH_GATHER) {
            // Format of GATHER: out[<loop-indexes>] = in1[in1.start + in2[<loop-indexes>]]
            vector<string> ops;
            {
                stringstream out;
                out << scope.getName(instr.operand[0]);
                if (scope.isArray(instr.operand[0])) {
                    out << write_array_subscription(scope, instr.operand[0]);
                }
                ops.push_back(out.str());
            }
            {
                assert(not bh_is_constant(&instr.operand[1]));
                stringstream out;
                out << scope.getName(instr.operand[1]);
                out << "[" << instr.operand[1].start << " + ";
                out << scope.getName(instr.operand[2]);
                if (scope.isArray(instr.operand[2])) {
                    out << write_array_subscription(scope, instr.operand[2]);
                }
                out << "]";
                ops.push_back(out.str());
            }
            write_operation(instr, ops);
            return;
        }
        if (instr.opcode == BH_SCATTER or instr.opcode == BH_COND_SCATTER) {
            // Format of SCATTER: out[out.start + in2[<loop-indexes>]] = in1[<loop-indexes>]
            vector<string> ops;
            {
                stringstream out;
                out << scope.getName(instr.operand[0]);
                out << "[" << instr.operand[0].start << " + ";
                out << scope.getName(instr.operand[2]);
                if (scope.isArray(instr.operand[2])) {
                    out << write_array_subscription(scope, instr.operand[2]);
                }
                out << "]";
                ops.push_back(out.str());
            }
            {
                stringstream out;
                out << scope.getName(instr.operand[1]);
                if (scope.isArray(instr.operand[1])) {
                    out << write_array_subscription(scope, instr.operand[1]);
                }
                ops.push_back(out.str());
            }
            if (instr.opcode == BH_COND_SCATTER) { // Add the conditional array (fourth operand)
                stringstream out;
                out << scope.getName(instr.operand[3]);
                if (scope.isArray(instr.operand[3])) {
                    out << write_array_subscription(scope, instr.operand[3]);
                }
                ops.push_back(out.str());
            }
            write_operation(instr, ops);
            return;
        }

        vector<string> ops;
        for (size_t o = 0; o < instr.operand.size(); ++o) {
            const bh_view &view = instr.operand[o];
            stringstream out;
            if (bh_is_constant(&view)) {
                const int64_t constID = scope.symbols.constID(instr);
                if (constID >= 0) {
                    out << "c" << scope.symbols.constID(instr);
                } else {
                    instr.constant.pprint(out, false);
                }
            } else {
                out << scope.getName(view);
                if (scope.isArray(view)) {
                    if (o == 0 and bh_opcode_is_reduction(instr.opcode) and instr.operand[1].ndim > 1) {
                        // If 'instr' is a reduction we have to ignore the reduced axis of the output array when
                        // reducing to a non-scalar
                        out <<write_array_subscription(scope, view, true, instr.sweep_axis());
                    } else {
                        out << write_array_subscription(scope, view);
                    }
                }
            }
            ops.push_back(out.str());
        }
        write_operation(instr, ops);
    }

    void write_header(const SymbolTable &symbols, Scope &scope, const LoopB &block,
                      const ConfigParser &config);

    void loop_head_writer(const SymbolTable &symbols, Scope &scope, const LoopB &block,
                          const ConfigParser &config, bool loop_is_peeled,
                          const vector<const LoopB *> &threaded_blocks);
};
#endif //BOHRIUM_WRITER_OPENCL_HPP
