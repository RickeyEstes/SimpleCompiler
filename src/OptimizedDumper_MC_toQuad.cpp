#include "OptimizedDumper.h"

void MC::toQuad(std::ostream &stream, const std::string &indent, const Function &func) const {
    switch(o) {
    case oli:
    case omov:
        stream << indent << dst << " = " << a << std::endl;
        break;
    case oneg:
        stream << indent << dst << " = -" << a << std::endl;
        break;
    case omovv0:
        if(a.empty())
            stream << indent << dst << " = RET" << std::endl;
        else
            stream << indent << dst << " = " << a << std::endl;
        break;
    case oadd:
        stream << indent << dst << " = " << a << " + " << b << std::endl;
        break;
    case osub:
        stream << indent << dst << " = " << a << " - " << b << std::endl;
        break;
    case omul:
        stream << indent << dst << " = " << a << " * " << b << std::endl;
        break;
    case odiv:
        stream << indent << dst << " = " << a << " / " << b << std::endl;
        break;
    case oloadarr:
        stream << indent << b << " = " << lab << "[" << a << "]" << std::endl;
        break;
    case ostorearr:
        stream << indent << lab << "[" << a << "]" << " = " << b << std::endl;
        break;
    case ojmp:
        stream << indent << "GOTO " << lab << std::endl;
        break;
    case obeq:
        stream << indent << a << " == " << b << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case obne:
        stream << indent << a << " != " << b << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case oblt:
        stream << indent << a << " < " << b << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case oble:
        stream << indent << a << " <= " << b << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case obgt:
        stream << indent << a << " > " << b << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case obge:
        stream << indent << a << " >= " << b << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case obeqz:
        stream << indent << a << " == " << 0 << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case obnez:
        stream << indent << a << " != " << 0 << std::endl;
        stream << indent << "BNZ " << lab << std::endl;
        break;
    case oarg:
        stream << indent << "push " << a << std::endl;
        break;
    case ocall:
        stream << indent << "call " << lab << std::endl;
        break;
    case oret:
        stream << indent << "ret " << dst << std::endl;
        break;
    case opstr:
        stream << indent << "PRINT \"" << func.prog.strList.strings[lab]->getLiteral() << "\"" << std::endl;
        break;
    case opint:
    case opchar:
        stream << indent << "PRINT " << dst << std::endl;
        break;
    case orint:
    case orchar:
        stream << indent << "// involk syscall to read in var" << std::endl;
        break;
    default:
        assert(false);
        break;
    }
}
