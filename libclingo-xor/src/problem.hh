#pragma once

#include <clingo.hh>
#include "util.hh"

struct Term {
    Number co;
    Clingo::Symbol var;
};

std::ostream &operator<<(std::ostream &out, Term const &term);

struct Inequality {
    std::vector<Term> lhs;
    Number rhs;
    Clingo::literal_t lit;
};

std::ostream &operator<<(std::ostream &out, Inequality const &x);
