#pragma once

#include <clingo.hh>
#include "util.hh"

struct Inequality {
    std::vector<Clingo::Symbol> lhs;
    Number rhs;
    Clingo::literal_t lit;
};

std::ostream &operator<<(std::ostream &out, Inequality const &x);
