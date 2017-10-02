//
// Created by sebastian on 10/2/17.
//
#ifndef BH_INSTRUCTION_HPP
#define BH_INSTRUCTION_HPP

#include <string>
#include <sstream>

using namespace std;

namespace bohrium {
namespace jitk {
    string operation(const bh_instruction &instr, const vector<string> &ops);
}
}
#endif
