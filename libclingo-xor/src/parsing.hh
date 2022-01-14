#pragma once

#include "problem.hh"

#include <map>

constexpr char const *THEORY = R"(
#theory xor {
    xor_term { };
    &even/0 : xor_term, head;
    &odd/0 : xor_term, head
}.
)";

using VarMap = std::map<Clingo::literal_t, Clingo::Symbol>;

void evaluate_theory(Clingo::PropagateInit &init, VarMap &var_map, std::vector<XORConstraint> &iqs);
