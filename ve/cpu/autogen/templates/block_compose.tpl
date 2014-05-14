#compiler-settings
directiveStartToken= %
#end compiler-settings
%slurp
#include "block.hpp"
//
//  NOTE: This file is autogenerated based on the tac-definition.
//        You should therefore not edit it manually.
//
using namespace std;
namespace bohrium{
namespace core{

/**
 *  Compose a block based on the instruction-nodes within a dag.
 */
bool Block::compose()
{
    if (this->dag.nnode<1) {
        fprintf(stderr, "Got an empty dag. This cannot be right...\n");
        return false;
    }
    bool compose_res = compose(0, this->dag.nnode-1);

    return compose_res;
}

/**
 *  Compose a block based on the instruction-nodes within a dag.
 *  Starting from and including node_start to and including node_end.
 */
bool Block::compose(bh_intp node_start, bh_intp node_end)
{
    if (this->dag.nnode<1) {
        fprintf(stderr, "Got an empty dag. This cannot be right...\n");
        return false;
    }
    
    // Reset metadata
    ntacs_      = 0;        // The number of tacs in block
    noperands_  = 0;        // The number of operands
    omask_      = 0;        // And the operation mask
    symbol_         = "";
    symbol_text_    = "";       // Symbol of the block
    operand_map.clear();    // tac-operand to block scope mapping
    
    size_t pc = 0;
    for (int node_idx=node_start; node_idx<=node_end; ++node_idx, ++pc, ++ntacs_) {
        
        if (dag.node_map[node_idx] <0) {
            fprintf(stderr, "Code-generation for subgraphs is not supported yet.\n");
            return false;
        }

        this->instr_[pc] = &this->ir.instr_list[dag.node_map[node_idx]];
        bh_instruction& instr = *this->instr_[pc];

        uint32_t out=0, in1=0, in2=0;

        //
        // Program packing: output argument
        // NOTE: All but BH_NONE has an output which is an array
        if (instr.opcode != BH_NONE) {
            out = this->add_operand(instr, 0);
        }

        //
        // Program packing; operator, operand and input argument(s).
        switch (instr.opcode) {    // [OPCODE_SWITCH]

            %for $opcode, $operation, $operator, $nin in $operations
            case $opcode:
                %if nin >= 1
                in1 = this->add_operand(instr, 1);
                %end if
                %if nin >= 2
                in2 = this->add_operand(instr, 2);
                %end if

                this->tacs[pc].op    = $operation;  // TAC
                this->tacs[pc].oper  = $operator;
                this->tacs[pc].out   = out;
                this->tacs[pc].in1   = in1;
                this->tacs[pc].in2   = in2;
            
                this->omask_ |= $operation;    // Operationmask
                break;
            %end for

            default:
                if (instr.opcode>=BH_MAX_OPCODE_ID) {   // Handle extensions here

                    in1 = this->add_operand(instr, 1);
                    in2 = this->add_operand(instr, 2);

                    this->tacs[pc].op   = EXTENSION;
                    this->tacs[pc].oper = EXTENSION_OPERATOR;
                    this->tacs[pc].out  = out;
                    this->tacs[pc].in1  = in1;
                    this->tacs[pc].in2  = in2;

                    this->omask_ |= EXTENSION;
                    break;

                } else {
                    fprintf(stderr, "Block::compose: Err=[Unsupported instruction] {\n");
                    bh_pprint_instr(&instr);
                    fprintf(stderr, "}\n");
                    return false;
                }
        }

        //
        // Update the ref count
        symbol_table.ref_count(this->tacs[pc]);
    }
    return true;
}

}}
