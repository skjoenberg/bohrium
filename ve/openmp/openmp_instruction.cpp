//
// Created by sebastian on 10/3/17.
//

#include <string>
#include <sstream>
#include <jitk/block.hpp>
#include <jitk/base_db.hpp>

using namespace std;
namespace bohrium {
namespace jitk {

void write_sign_function(const string &operand, stringstream &out) {
    out << "((" << operand << " > 0) - (0 > " << operand << "))";
}

// Write opcodes that uses a different complex functions when targeting OpenCL
void write_opcodes_with_special_opencl_complex(const bh_instruction &instr, const vector<string> &ops,
                                               stringstream &out, int opencl, const char *fname,
                                               const char *fname_complex) {
    const bh_type t0 = instr.operand_type(0);
    if (opencl and bh_type_is_complex(t0)) {
        out << fname_complex << "(" << (t0 == bh_type::COMPLEX64 ? "float" : "double") << ", " << ops[0] \
 << ", " << ops[1] << ");\n";
    } else {
        out << ops[0] << " = " << fname << "(" << ops[1] << ");\n";
    }
}

string operation(const bh_instruction &instr, const vector<string> &ops) {
    stringstream out;
    switch (instr.opcode) {
        // Opcodes that are Complex/OpenCL agnostic
        case BH_BITWISE_AND:
            out << ops[0] << " = " << ops[1] << " & " << ops[2] << ";\n";
            break;
        case BH_BITWISE_AND_REDUCE:
            out << ops[0] << " = " << ops[0] << " & " << ops[1] << ";\n";
            break;
        case BH_BITWISE_OR:
            out << ops[0] << " = " << ops[1] << " | " << ops[2] << ";\n";
            break;
        case BH_BITWISE_OR_REDUCE:
            out << ops[0] << " = " << ops[0] << " | " << ops[1] << ";\n";
            break;
        case BH_BITWISE_XOR:
            out << ops[0] << " = " << ops[1] << " ^ " << ops[2] << ";\n";
            break;
        case BH_BITWISE_XOR_REDUCE:
            out << ops[0] << " = " << ops[0] << " ^ " << ops[1] << ";\n";
            break;
        case BH_LOGICAL_NOT:
            out << ops[0] << " = !" << ops[1] << ";\n";
            break;
        case BH_LOGICAL_OR:
            out << ops[0] << " = " << ops[1] << " || " << ops[2] << ";\n";
            break;
        case BH_LOGICAL_OR_REDUCE:
            out << ops[0] << " = " << ops[0] << " || " << ops[1] << ";\n";
            break;
        case BH_LOGICAL_AND:
            out << ops[0] << " = " << ops[1] << " && " << ops[2] << ";\n";
            break;
        case BH_LOGICAL_AND_REDUCE:
            out << ops[0] << " = " << ops[0] << " && " << ops[1] << ";\n";
            break;
        case BH_LOGICAL_XOR:
            out << ops[0] << " = !" << ops[1] << " != !" << ops[2] << ";\n";
            break;
        case BH_LOGICAL_XOR_REDUCE:
            out << ops[0] << " = !" << ops[0] << " != !" << ops[1] << ";\n";
            break;
        case BH_LEFT_SHIFT:
            out << ops[0] << " = " << ops[1] << " << " << ops[2] << ";\n";
            break;
        case BH_RIGHT_SHIFT:
            out << ops[0] << " = " << ops[1] << " >> " << ops[2] << ";\n";
            break;
        case BH_GREATER:
            out << ops[0] << " = " << ops[1] << " > " << ops[2] << ";\n";
            break;
        case BH_GREATER_EQUAL:
            out << ops[0] << " = " << ops[1] << " >= " << ops[2] << ";\n";
            break;
        case BH_LESS:
            out << ops[0] << " = " << ops[1] << " < " << ops[2] << ";\n";
            break;
        case BH_LESS_EQUAL:
            out << ops[0] << " = " << ops[1] << " <= " << ops[2] << ";\n";
            break;
        case BH_MAXIMUM:
            out << ops[0] << " = " << ops[1] << " > " << ops[2] << " ? " << ops[1] << " : "
                << ops[2] << ";\n";
            break;
        case BH_MAXIMUM_REDUCE:
            out << ops[0] << " = " << ops[0] << " > " << ops[1] << " ? " << ops[0] << " : "
                << ops[1] << ";\n";
            break;
        case BH_MINIMUM:
            out << ops[0] << " = " << ops[1] << " < " << ops[2] << " ? " << ops[1] << " : "
                << ops[2] << ";\n";
            break;
        case BH_MINIMUM_REDUCE:
            out << ops[0] << " = " << ops[0] << " < " << ops[1] << " ? " << ops[0] << " : "
                << ops[1] << ";\n";
            break;
        case BH_INVERT:
            if (instr.operand[0].base->type == bh_type::BOOL)
                out << ops[0] << " = !" << ops[1] << ";\n";
            else
                out << ops[0] << " = ~" << ops[1] << ";\n";
            break;
        case BH_MOD:
            if (bh_type_is_float(instr.operand[0].base->type))
                out << ops[0] << " = fmod(" << ops[1] << ", " << ops[2] << ");\n";
            else
                out << ops[0] << " = " << ops[1] << " % " << ops[2] << ";\n";
            break;
        case BH_REMAINDER:
            if (bh_type_is_float(instr.operand[0].base->type)) {
                out << ops[0] << " = " << ops[1] << " - floor(" << ops[1] << " / " << ops[2] << ") * " << ops[2]
                    << ";\n";
            } else if (bh_type_is_unsigned_integer(instr.operand[0].base->type)) {
                out << ops[0] << " = " << ops[1] << " % " << ops[2] << ";\n";
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
                out << ops[0] << " = ((" << ops[1] << " > 0) == (" << ops[2] << " > 0) || "
                        "(" << ops[1] << " % " << ops[2] << ") == 0)?"
                            "(" << ops[1] << " % " << ops[2] << "):"
                            "(" << ops[1] << " % " << ops[2] << ") + " << ops[2] << ";\n";
            }
            break;
        case BH_RINT:
            out << ops[0] << " = rint(" << ops[1] << ");\n";
            break;
        case BH_EXP2:
            out << ops[0] << " = exp2(" << ops[1] << ");\n";
            break;
        case BH_EXPM1:
            out << ops[0] << " = expm1(" << ops[1] << ");\n";
            break;
        case BH_LOG1P:
            out << ops[0] << " = log1p(" << ops[1] << ");\n";
            break;
        case BH_ARCSIN:
            out << ops[0] << " = asin(" << ops[1] << ");\n";
            break;
        case BH_ARCCOS:
            out << ops[0] << " = acos(" << ops[1] << ");\n";
            break;
        case BH_ARCTAN:
            out << ops[0] << " = atan(" << ops[1] << ");\n";
            break;
        case BH_ARCTAN2:
            out << ops[0] << " = atan2(" << ops[1] << ", " << ops[2] << ");\n";
            break;
        case BH_ARCSINH:
            out << ops[0] << " = asinh(" << ops[1] << ");\n";
            break;
        case BH_ARCCOSH:
            out << ops[0] << " = acosh(" << ops[1] << ");\n";
            break;
        case BH_ARCTANH:
            out << ops[0] << " = atanh(" << ops[1] << ");\n";
            break;
        case BH_FLOOR:
            out << ops[0] << " = floor(" << ops[1] << ");\n";
            break;
        case BH_CEIL:
            out << ops[0] << " = ceil(" << ops[1] << ");\n";
            break;
        case BH_TRUNC:
            out << ops[0] << " = trunc(" << ops[1] << ");\n";
            break;
        case BH_LOG2:
            out << ops[0] << " = log2(" << ops[1] << ");\n";
            break;
        case BH_ISNAN: {
            const bh_type t0 = instr.operand_type(1);

            if (bh_type_is_complex(t0)) {
                out << ops[0] << " = isnan(creal(" << ops[1] << "));\n";
            } else if (bh_type_is_float(t0)) {
                out << ops[0] << " = isnan(" << ops[1] << ");\n";
            } else {
                out << ops[0] << " = false;\n";
            }
            break;
        }
        case BH_ISINF: {
            const bh_type t0 = instr.operand_type(1);

            if (bh_type_is_complex(t0)) {
                out << ops[0] << " = isinf(creal(" << ops[1] << "));\n";
            } else if (bh_type_is_float(t0)) {
                out << ops[0] << " = isinf(" << ops[1] << ");\n";
            } else {
                out << ops[0] << " = false;\n";
            }
            break;
        }
        case BH_ISFINITE: {
            const bh_type t0 = instr.operand_type(1);

            if (bh_type_is_complex(t0)) {
                out << ops[0] << " = isfinite(creal(" << ops[1] << "));\n";
            } else if (bh_type_is_float(t0)) {
                out << ops[0] << " = isfinite(" << ops[1] << ");\n";
            } else {
                out << ops[0] << " = true;\n";
            }
            break;
        }
        case BH_CONJ:
            out << ops[0] << " = conj(" << ops[1] << ");\n";
            break;
        case BH_RANGE:
            out << ops[0] << " = " << ops[1] << ";\n";
            break;
        case BH_RANDOM:
            out << ops[0] << " = " << ops[1] << ";\n";
            break;

// Opcodes that uses a different function name in OpenCL
        case BH_SIN:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "sin", "CSIN");
            break;
        case BH_COS:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "cos", "CCOS");
            break;
        case BH_TAN:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "tan", "CTAN");
            break;
        case BH_SINH:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "sinh", "CSINH");
            break;
        case BH_COSH:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "cosh", "CCOSH");
            break;
        case BH_TANH:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "tanh", "CTANH");
            break;
        case BH_EXP:
            write_opcodes_with_special_opencl_complex(instr, ops, out, false, "exp", "CEXP");
            break;
        case BH_ABSOLUTE: {
            const bh_type t0 = instr.operand_type(1);

            if (t0 == bh_type::BOOL) {
                out << ops[0] << " = true;\n";
            } else if (bh_type_is_unsigned_integer(t0)) {
                out << ops[0] << " = " << ops[1] << ";\n"; // no-op
            } else if (bh_type_is_float(t0)) {
                out << ops[0] << " = fabs(" << ops[1] << ");\n";
            } else if (t0 == bh_type::INT64) {
                out << ops[0] << " = llabs(" << ops[1] << ");\n";
            } else
                out << ops[0] << " = abs((int)" << ops[1] << ");\n";
        }
            break;
        case BH_SQRT:
            out << ops[0] << " = sqrt(" << ops[1] << ");\n";
            break;
        case BH_LOG:
            out << ops[0] << " = log(" << ops[1] << ");\n";
            break;
        case BH_NOT_EQUAL:
            out << ops[0] << " = " << ops[1] << " != " << ops[2] << ";\n";
            break;
        case BH_EQUAL:
            out << ops[0] << " = " << ops[1] << " == " << ops[2] << ";\n";
            break;
        case BH_POWER: {
            out << ops[0] << " = pow(" << ops[1] << ", " << ops[2] << ");\n";
            break;
        }

// The operations that has to be handled differently in OpenCL
        case BH_ADD:
            out << ops[0] << " = " << ops[1] << " + " << ops[2] << ";\n";
            break;
        case BH_ADD_REDUCE:
            out << ops[0] << " += " << ops[1] << ";\n";
            break;
        case BH_ADD_ACCUMULATE:
            out << ops[0] << " = " << ops[1] << " + " << ops[2] << ";\n";
            break;
        case BH_SUBTRACT:
            out << ops[0] << " = " << ops[1] << " - " << ops[2] << ";\n";
            break;
        case BH_MULTIPLY:
            out << ops[0] << " = " << ops[1] << " * " << ops[2] << ";\n";
            break;
        case BH_MULTIPLY_REDUCE:
            out << ops[0] << " *= " << ops[1] << ";\n";
            break;
        case BH_MULTIPLY_ACCUMULATE:
            out << ops[0] << " = " << ops[1] << " * " << ops[2] << ";\n";
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
                out << ops[0] << " = ((" << ops[1] << " > 0) != (" << ops[2] << " > 0) && "
                        "(" << ops[1] << " % " << ops[2] << ") != 0)?"
                            "(" << ops[1] << " / " << ops[2] << " - 1):"
                            "(" << ops[1] << " / " << ops[2] << ");\n";
            } else {
                out << ops[0] << " = " << ops[1] << " / " << ops[2] << ";\n";
            }
            break;
        }

// In OpenCL we have to do explicit conversion of complex numbers
        case BH_IDENTITY: {
            out << ops[0] << " = ";
            const bh_type t0 = instr.operand_type(0);
            const bh_type t1 = instr.operand_type(1);
            out << ops[1];
            out << ";\n";
            break;
        }

// C99 does not have log10 for complex, so we use the formula: clog(z) = log(z)/log(10)
        case BH_LOG10: {
            const bh_type t0 = instr.operand_type(0);
            if (bh_type_is_complex(t0)) {
                out << ops[0] << " = clog(" << ops[1] << ") / log(10.0f);\n";
            } else {
                out << ops[0] << " = log10(" << ops[1] << ");\n";
            }
            break;
        }

// Extracting the real or imaginary part differ in OpenCL
        case BH_REAL:
            out << ops[0] << " = creal(" << ops[1] << ");\n";
            break;
        case BH_IMAG:
            out << ops[0] << " = cimag(" << ops[1] << ");\n";
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
                out << ctype << " real = creal(" << ops[1] << "); \n";
                out << ctype << " imag = cimag(" << ops[1] << "); \n";
                out << ops[0] << " ";
                out << "= (real == 0 ? ";

                write_sign_function("imag", out);
                out << " : ";
                write_sign_function("real", out);
                out << ");\n";
            } else {
                out << ops[0] << " = ";
                write_sign_function(ops[1], out);
                out << ";\n";
            }
            break;
        }
        case BH_GATHER:
            out << ops[0] << " = " << ops[1] << ";\n";
            break;
        case BH_SCATTER:
            out << ops[0] << " = " << ops[1] << ";\n";
            break;
        case BH_COND_SCATTER:
            out << "if (" << ops[2] << ") {" << ops[0] << " = " << ops[1] << ";}\n";
            break;
        default:
            cerr << "Instruction \"" << instr << "\" not supported\n";
            throw runtime_error("Instruction not supported.");
    }
    return out.str();
}
}
}
