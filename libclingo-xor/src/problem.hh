#pragma once

#include <clingo.hh>
#include "util.hh"

struct XORConstraint {
    std::vector<index_t> lhs;
    Value rhs;
    Clingo::literal_t lit;
};

std::ostream &operator<<(std::ostream &out, XORConstraint const &x);
