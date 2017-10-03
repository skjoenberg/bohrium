#ifndef __BH_WRITER_HPP
#define __BH_WRITER_HPP

#include <string>
#include <sstream>
#include <jitk/base_db.hpp>

using namespace std;

namespace bohrium {
namespace jitk {
class Writer {
public:
    std::stringstream ss;

    Writer() {};

    virtual void write(const std::string s) = 0;

    virtual void declare(const bh_view &view, const std::string &type_str, int spacing, Scope* scope) = 0;

    virtual void declareIdx(const bh_view &view, const std::string &type_str, int spacing, Scope* scope) = 0;

    virtual void spaces(const int spacing) = 0;

    virtual void write_instr(const Scope &scope, const bh_instruction &instr) = 0;

    virtual void write_operation(const bh_instruction &instr, const vector<string> &ops) = 0;

    virtual void endl() = 0;

    virtual void end_loop() = 0;

    virtual void comment(const string s) = 0;

    virtual string write_array_index(const Scope &scope, const bh_view &view, int hidden_axis = BH_MAXDIM,
                                     const pair<int, int> axis_offset = std::make_pair(BH_MAXDIM, 0)) = 0;

    virtual string write_array_subscription(const Scope &scope, const bh_view &view,
                                            bool ignore_declared_indexes=false, int hidden_axis = BH_MAXDIM,
                                            const pair<int, int> axis_offset  = std::make_pair(BH_MAXDIM, 0)) = 0;

    virtual string write_array_index_variables(const Scope &scope, const bh_view &view, int hidden_axis = BH_MAXDIM,
                                               const pair<int, int> axis_offset = std::make_pair(BH_MAXDIM, 0)) = 0;

    virtual void write_header(const SymbolTable &symbols, Scope &scope, const LoopB &block,
                              const ConfigParser &config) = 0;

    virtual void loop_head_writer(const SymbolTable &symbols, Scope &scope, const LoopB &block,
                                  const ConfigParser &config, bool loop_is_peeled,
                                  const vector<const LoopB *> &threaded_blocks) = 0;
};
}
}
#endif
