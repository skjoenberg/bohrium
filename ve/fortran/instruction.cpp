//
// Created by sebastian on 10/2/17.
//

#include <string>
#include <sstream>
#include <jitk/block.hpp>
#include <jitk/base_db.hpp>

using namespace std;

namespace bohrium{
namespace jitk{

void write_sign_function(const string &operand, stringstream &out) {
    out << "((" << operand << " > 0) - (0 > " << operand << "))";
}

string operation(const bh_instruction &instr, const vector <string> &ops) {
    stringstream ss;
    switch (instr.opcode) {
        // Opcodes that are Complex/OpenCL agnostic
        case BH_BITWISE_AND:
            ss << ops[0] << " = " << ops[1] << " & " << ops[2] << ";\n";
            break;
        case BH_BITWISE_AND_REDUCE:
            ss << ops[0] << " = " << ops[0] << " & " << ops[1] << ";\n";
            break;
        case BH_BITWISE_OR:
            ss << ops[0] << " = " << ops[1] << " | " << ops[2] << ";\n";
            break;
        case BH_BITWISE_OR_REDUCE:
            ss << ops[0] << " = " << ops[0] << " | " << ops[1] << ";\n";
            break;
        case BH_BITWISE_XOR:
            ss << ops[0] << " = " << ops[1] << " ^ " << ops[2] << ";\n";
            break;
        case BH_BITWISE_XOR_REDUCE:
            ss << ops[0] << " = " << ops[0] << " ^ " << ops[1] << ";\n";
            break;
        case BH_LOGICAL_NOT:
            ss << ops[0] << " = !" << ops[1] << ";\n";
            break;
        case BH_LOGICAL_OR:
            ss << ops[0] << " = " << ops[1] << " || " << ops[2] << ";\n";
            break;
        case BH_LOGICAL_OR_REDUCE:
            ss << ops[0] << " = " << ops[0] << " || " << ops[1] << ";\n";
            break;
        case BH_LOGICAL_AND:
            ss << ops[0] << " = " << ops[1] << " && " << ops[2] << ";\n";
            break;
        case BH_LOGICAL_AND_REDUCE:
            ss << ops[0] << " = " << ops[0] << " && " << ops[1] << ";\n";
            break;
        case BH_LOGICAL_XOR:
            ss << ops[0] << " = !" << ops[1] << " != !" << ops[2] << ";\n";
            break;
        case BH_LOGICAL_XOR_REDUCE:
            ss << ops[0] << " = !" << ops[0] << " != !" << ops[1] << ";\n";
            break;
        case BH_LEFT_SHIFT:
            ss << ops[0] << " = " << ops[1] << " << " << ops[2] << ";\n";
            break;
        case BH_RIGHT_SHIFT:
            ss << ops[0] << " = " << ops[1] << " >> " << ops[2] << ";\n";
            break;
        case BH_GREATER:
            ss << ops[0] << " = " << ops[1] << " > " << ops[2] << ";\n";
            break;
        case BH_GREATER_EQUAL:
            ss << ops[0] << " = " << ops[1] << " >= " << ops[2] << ";\n";
            break;
        case BH_LESS:
            ss << ops[0] << " = " << ops[1] << " < " << ops[2] << ";\n";
            break;
        case BH_LESS_EQUAL:
            ss << ops[0] << " = " << ops[1] << " <= " << ops[2] << ";\n";
            break;
        case BH_MAXIMUM:
            ss << ops[0] << " = " << ops[1] << " > " << ops[2] << " ? " << ops[1] << " : "
               << ops[2] << ";\n";
            break;
        case BH_MAXIMUM_REDUCE:
            ss << ops[0] << " = " << ops[0] << " > " << ops[1] << " ? " << ops[0] << " : "
               << ops[1] << ";\n";
            break;
        case BH_MINIMUM:
            ss << ops[0] << " = " << ops[1] << " < " << ops[2] << " ? " << ops[1] << " : "
               << ops[2] << ";\n";
            break;
        case BH_MINIMUM_REDUCE:
            ss << ops[0] << " = " << ops[0] << " < " << ops[1] << " ? " << ops[0] << " : "
               << ops[1] << ";\n";
            break;
        case BH_INVERT:
            if (instr.operand[0].base->type == bh_type::BOOL)
                ss << ops[0] << " = !" << ops[1] << ";\n";
            else
                ss << ops[0] << " = ~" << ops[1] << ";\n";
            break;
        case BH_MOD:
            if (bh_type_is_float(instr.operand[0].base->type))
                ss << ops[0] << " = fmod(" << ops[1] << ", " << ops[2] << ");\n";
            else
                ss << ops[0] << " = " << ops[1] << " % " << ops[2] << ";\n";
            break;
        case BH_REMAINDER:
            if (bh_type_is_float(instr.operand[0].base->type)) {
                ss << ops[0] << " = " << ops[1] << " - floor(" << ops[1] << " / " << ops[2] << ") * " << ops[2]
                   << ";\n";
            } else if (bh_type_is_unsigned_integer(instr.operand[0].base->type)) {
                ss << ops[0] << " = " << ops[1] << " % " << ops[2] << ";\n";
            } else {
                /* The Python/NumPy implementation of remainder on signed integers
                    const @type@ rem = in1 % in2;
                    if ((in1 > 0) == (in2 > 0) || rem == 0) {
                        *((@type@ *)op1) = rem;
                    }
                    else {
                        *((@type@ *)op1) = rem + in2;
                    }
                */
                ss << ops[0] << " = ((" << ops[1] << " > 0) == (" << ops[2] << " > 0) || "
                        "(" << ops[1] << " % " << ops[2] << ") == 0)?"
                           "(" << ops[1] << " % " << ops[2] << "):"
                           "(" << ops[1] << " % " << ops[2] << ") + " << ops[2] << ";\n";
            }
            break;
        case BH_RINT:
            ss << ops[0] << " = rint(" << ops[1] << ");\n";
            break;
        case BH_EXP2:
            ss << ops[0] << " = exp2(" << ops[1] << ");\n";
            break;
        case BH_EXPM1:
            ss << ops[0] << " = expm1(" << ops[1] << ");\n";
            break;
        case BH_LOG1P:
            ss << ops[0] << " = log1p(" << ops[1] << ");\n";
            break;
        case BH_ARCSIN:
            ss << ops[0] << " = asin(" << ops[1] << ");\n";
            break;
        case BH_ARCCOS:
            ss << ops[0] << " = acos(" << ops[1] << ");\n";
            break;
        case BH_ARCTAN:
            ss << ops[0] << " = atan(" << ops[1] << ");\n";
            break;
        case BH_ARCTAN2:
            ss << ops[0] << " = atan2(" << ops[1] << ", " << ops[2] << ");\n";
            break;
        case BH_ARCSINH:
            ss << ops[0] << " = asinh(" << ops[1] << ");\n";
            break;
        case BH_ARCCOSH:
            ss << ops[0] << " = acosh(" << ops[1] << ");\n";
            break;
        case BH_ARCTANH:
            ss << ops[0] << " = atanh(" << ops[1] << ");\n";
            break;
        case BH_FLOOR:
            ss << ops[0] << " = floor(" << ops[1] << ");\n";
            break;
        case BH_CEIL:
            ss << ops[0] << " = ceil(" << ops[1] << ");\n";
            break;
        case BH_TRUNC:
            ss << ops[0] << " = trunc(" << ops[1] << ");\n";
            break;
        case BH_LOG2:
            ss << ops[0] << " = log2(" << ops[1] << ");\n";
            break;
        case BH_ISNAN: {
            const bh_type t0 = instr.operand_type(1);
            if (bh_type_is_complex(t0)) {
                ss << ops[0] << " = isnan(creal(" << ops[1] << "));\n";
            } else if (bh_type_is_float(t0)) {
                ss << ops[0] << " = isnan(" << ops[1] << ");\n";
            } else {
                ss << ops[0] << " = false;\n";
            }
            break;
        }
        case BH_ISINF: {
            const bh_type t0 = instr.operand_type(1);

            if (bh_type_is_complex(t0)) {
                ss << ops[0] << " = isinf(creal(" << ops[1] << "));\n";
            } else if (bh_type_is_float(t0)) {
                ss << ops[0] << " = isinf(" << ops[1] << ");\n";
            } else {
                ss << ops[0] << " = false;\n";
            }
            break;
        }
        case BH_ISFINITE: {
            const bh_type t0 = instr.operand_type(1);

            if (bh_type_is_complex(t0)) {
                ss << ops[0] << " = isfinite(creal(" << ops[1] << "));\n";
            } else if (bh_type_is_float(t0)) {
                ss << ops[0] << " = isfinite(" << ops[1] << ");\n";
            } else {
                ss << ops[0] << " = true;\n";
            }
            break;
        }
        case BH_CONJ:
            ss << ops[0] << " = conj(" << ops[1] << ");\n";
            break;
        case BH_RANGE:
            ss << ops[0] << " = " << ops[1] << ";\n";
            break;
        case BH_RANDOM:
            ss << ops[0] << " = " << ops[1] << ";\n";
            break;
            // Opcodes that uses a different function name in OpenCL
        case BH_SIN:
            ss << ops[0] << " = sin(" << ops[1] << ");\n";
            break;
        case BH_COS:
            ss << ops[0] << " = cos(" << ops[1] << ");\n";
            break;
        case BH_TAN:
            ss << ops[0] << " = tan(" << ops[1] << ");\n";
            break;
        case BH_SINH:
            ss << ops[0] << " = sinh(" << ops[1] << ");\n";
            break;
        case BH_COSH:
            ss << ops[0] << " = cosh(" << ops[1] << ");\n";
            break;
        case BH_TANH:
            ss << ops[0] << " = tanh(" << ops[1] << ");\n";
            break;
        case BH_EXP:
            ss << ops[0] << " = exp(" << ops[1] << ");\n";
            break;
        case BH_ABSOLUTE: {
            const bh_type t0 = instr.operand_type(1);

            if (t0 == bh_type::BOOL) {
                ss << ops[0] << " = true;\n";
            } else if (bh_type_is_unsigned_integer(t0)) {
                ss << ops[0] << " = " << ops[1] << ";\n"; // no-op
            } else if (bh_type_is_float(t0)) {
                ss << ops[0] << " = abs(" << ops[1] << ");\n";
            } else if (t0 == bh_type::INT64) {
                ss << ops[0] << " = llabs(" << ops[1] << ");\n";
            } else {
                ss << ops[0] << " = abs((int)" << ops[1] << ");\n";
            }
            break;
        }
        case BH_SQRT:
            ss << ops[0] << " = sqrt(" << ops[1] << ");\n";
            break;
        case BH_LOG:
            ss << ops[0] << " = log(" << ops[1] << ");\n";
            break;
        case BH_NOT_EQUAL:
            ss << ops[0] << " = " << ops[1] << " != " << ops[2] << ";\n";
            break;
        case BH_EQUAL:
            ss << ops[0] << " = " << ops[1] << " == " << ops[2] << ";\n";
            break;
        case BH_POWER: {
            ss << ops[0] << " = pow(" << ops[1] << ", " << ops[2] << ");\n";
            break;
        }
        case BH_ADD:
            ss << ops[0] << " = " << ops[1] << " + " << ops[2] << ";\n";
            break;
        case BH_ADD_REDUCE:
            ss << ops[0] << " = " << ops[0] << " + " << ops[1] << ";\n";
            break;
        case BH_ADD_ACCUMULATE:
            ss << ops[0] << " = " << ops[1] << " + " << ops[2] << ";\n";
            break;
        case BH_SUBTRACT:
            ss << ops[0] << " = " << ops[1] << " - " << ops[2] << ";\n";
            break;
        case BH_MULTIPLY:
            ss << ops[0] << " = " << ops[1] << " * " << ops[2] << ";\n";
            break;
        case BH_MULTIPLY_REDUCE:
            ss << ops[0] << " *= " << ops[1] << ";\n";
            break;
        case BH_MULTIPLY_ACCUMULATE:
            ss << ops[0] << " = " << ops[1] << " * " << ops[2] << ";\n";
            break;
        case BH_DIVIDE: {
            if (bh_type_is_signed_integer(instr.operand[0].base->type)) {
                /* Python/NumPy signed integer division
                    if (in2 == 0 || (in1 == NPY_MIN_@TYPE@ && in2 == -1)) {
                        npy_set_floatstatus_divbyzero();
                        *((@type@ *)op1) = 0;
                    }
                    else if (((in1 > 0) != (in2 > 0)) && (in1 % in2 != 0)) {
                        *((@type@ *)op1) = in1/in2 - 1;
                    }
                    else {
                        *((@type@ *)op1) = in1/in2;
                    }
                */
                ss << ops[0] << " = ((" << ops[1] << " > 0) != (" << ops[2] << " > 0) && "
                        "(" << ops[1] << " % " << ops[2] << ") != 0)?"
                           "(" << ops[1] << " / " << ops[2] << " - 1):"
                           "(" << ops[1] << " / " << ops[2] << ");\n";
            } else {
                ss << ops[0] << " = " << ops[1] << " / " << ops[2] << ";\n";
            }
            break;
        }

            // In OpenCL we have to do explicit conversion of complex numbers
        case BH_IDENTITY: {
            ss << ops[0] << " = ";
            ss << ops[1];
            ss << ";\n";
            break;
        }

            // C99 does not have log10 for complex, so we use the formula: clog(z) = log(z)/log(10)
        case BH_LOG10: {
            const bh_type t0 = instr.operand_type(0);
            if (bh_type_is_complex(t0)) {
                ss << ops[0] << " = clog(" << ops[1] << ") / log(10.0f);\n";
            } else {
                ss << ops[0] << " = log10(" << ops[1] << ");\n";
            }
            break;
        }

            // Extracting the real or imaginary part differ in OpenCL
        case BH_REAL:
            ss << ops[0] << " = creal(" << ops[1] << ");\n";
            break;
        case BH_IMAG:
            ss << ops[0] << " = cimag(" << ops[1] << ");\n";
            break;

            /* NOTE: There are different ways to define sign of complex numbers.
             * We uses the same definition as in NumPy
             * <http://docs.scipy.org/doc/numpy-dev/reference/generated/numpy.sign.html>
             */
        case BH_SIGN: {
            const bh_type t0 = instr.operand_type(0);
            if (bh_type_is_complex(t0)) {
                //              1         if Re(z) > 0
                // csgn(z) = { -1         if Re(z) < 0
                //             sgn(Im(z)) if Re(z) = 0
                const char *ctype = (t0 == bh_type::COMPLEX64 ? "float" : "double");
                ss << ctype << " real = creal(" << ops[1] << "); \n";
                ss << ctype << " imag = cimag(" << ops[1] << "); \n";
                ss << ops[0] << " ";
                ss << "= (real == 0 ? ";
                write_sign_function("imag", ss);
                ss << " : ";
                write_sign_function("real", ss);
                ss << ");\n";
            } else {
                ss << ops[0] << " = ";
                write_sign_function(ops[1], ss);
                ss << ";\n";
            }
            break;
        }
        case BH_GATHER:
            ss << ops[0] << " = " << ops[1] << ";\n";

            break;
        case BH_SCATTER:
            ss << ops[0] << " = " << ops[1] << ";\n";
            break;
        case BH_COND_SCATTER:
            ss << "if (" << ops[2] << ") {" << ops[0] << " = " << ops[1] << ";}\n";
            break;
        default:
            cerr << "Instruction \"" << instr << "\" not supported\n";
            throw runtime_error("Instruction not supported.");
    }
    return ss.str();
}
}
}