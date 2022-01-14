#pragma once

#include <clingo.hh>
#include "util.hh"

struct XORConstraint {
    std::vector<Clingo::Symbol> lhs;
    Value rhs;
    Clingo::literal_t lit;
};

std::ostream &operator<<(std::ostream &out, XORConstraint const &x);
