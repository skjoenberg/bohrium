//
// Created by sebastian on 10/3/17.
//

#ifndef BOHRIUM_OPENMP_INSTRUCTION_HPP
#define BOHRIUM_OPENMP_INSTRUCTION_HPP

#include <string>
#include <sstream>

using namespace std;

namespace bohrium {
namespace jitk {
string operation(const bh_instruction &instr, const vector<string> &ops);
}
}

#endif //BOHRIUM_OPENMP_INSTRUCTION_HPP