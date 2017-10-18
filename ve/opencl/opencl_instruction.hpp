//
// Created by sebastian on 10/9/17.
//

#ifndef BOHRIUM_OPENCL_INSTRUCTION_HPP_HPP
#define BOHRIUM_OPENCL_INSTRUCTION_HPP_HPP

#include <string>
#include <sstream>

using namespace std;

namespace bohrium {
namespace jitk {
string operation(const bh_instruction &instr, const vector<string> &ops);
}
}

#endif //BOHRIUM_OPENCL_INSTRUCTION_HPP_HPP
