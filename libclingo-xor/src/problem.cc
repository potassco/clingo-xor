#include "problem.hh"

std::ostream &operator<<(std::ostream &out, XORConstraint const &x) {
    bool plus{false};
    for (auto const &term : x.lhs) {
        if (plus) {
            out << " + ";
        }
        else {
            plus = true;
        }
        out << "var_" << term;
    }
    if (x.lhs.empty()) {
        out << "0";
    }
    out << " = " << x.rhs << " :- " << (x.lit < 0 ? "not " : "") << "lit_" << std::abs(x.lit);
    return out;
}
