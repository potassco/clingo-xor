#include "problem.hh"

std::ostream &operator<<(std::ostream &out, Term const &term) {
    if (term.co != 1) {
        out << term.co << "*";
    }
    if (term.var.type() == Clingo::SymbolType::Number) {
        out << "#aux_" << term.var;
    }
    else {
        out << term.var;
    }
    return out;
}

std::ostream &operator<<(std::ostream &out, Inequality const &x) {
    bool plus{false};
    for (auto const &term : x.lhs) {
        if (plus) {
            out << " + ";
        }
        else {
            plus = true;
        }
        out << term;
    }
    if (x.lhs.empty()) {
        out << "0";
    }
    out << " = " << x.rhs << " :- " << (x.lit < 0 ? "not " : "") << "lit_" << std::abs(x.lit);
    return out;
}
