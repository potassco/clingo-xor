#include "parsing.hh"

#include <map>
#include <regex>

namespace {

template <typename T=void>
[[nodiscard]] T throw_syntax_error(char const *message="Invalid Syntax") {
    throw std::runtime_error(message);
}

void check_syntax(bool condition, char const *message="Invalid Syntax") {
    if (!condition) {
        throw_syntax_error(message);
    }
}

[[nodiscard]] bool match(Clingo::TheoryTerm const &term, char const *name, size_t arity) {
    return (term.type() == Clingo::TheoryTermType::Symbol &&
        std::strcmp(term.name(), name) == 0 &&
        arity == 0) ||
        (term.type() == Clingo::TheoryTermType::Function &&
        std::strcmp(term.name(), name) == 0 &&
        term.arguments().size() == arity);
}

bool is_string(Clingo::TheoryTerm const &term) {
    if (term.type() != Clingo::TheoryTermType::Symbol) {
        return false;
    }
    char const *name = term.name();
    size_t len = std::strlen(term.name());
    return len >= 2 && name[0] == '"' && name[len - 1] == '"'; // NOLINT
}

[[nodiscard]] Clingo::Symbol evaluate(Clingo::TheoryTerm const &term) {
    if (is_string(term)) {
        char const *name = term.name();
        size_t len = std::strlen(term.name());
        return Clingo::String(std::string{name + 1, name + len - 1}.c_str()); // NOLINT
    }

    if (term.type() == Clingo::TheoryTermType::Symbol) {
        return Clingo::Function(term.name(), {});
    }

    if (term.type() == Clingo::TheoryTermType::Number) {
        return Clingo::Number(term.number());
    }

    if (term.type() == Clingo::TheoryTermType::Tuple || term.type() == Clingo::TheoryTermType::Function) {
        std::vector<Clingo::Symbol> args;
        args.reserve(term.arguments().size());
        for (auto const &arg : term.arguments()) {
            args.emplace_back(evaluate(arg));
        }
        return Clingo::Function(term.type() == Clingo::TheoryTermType::Function ? term.name() : "", args);
    }

    return throw_syntax_error<Clingo::Symbol>();
}

} // namespace

void evaluate_theory(Clingo::PropagateInit &init, VarMap &var_map, std::vector<XORConstraint> &iqs) {
    auto theory = init.theory_atoms();
    auto ass = init.assignment();
    for (auto &&atom : theory) {
        bool even = match(atom.term(), "even", 0);
        bool odd  = match(atom.term(), "odd", 0);
        if (even || odd) {
            auto lit = init.solver_literal(atom.literal());
            if (ass.is_false(lit)) {
                continue;
            }

            // map from tuples to condition
            std::map<std::vector<Clingo::Symbol>, size_t> elem_ids;
            std::vector<std::vector<Clingo::literal_t>> elems;
            for (auto &&elem : atom.elements()) {
                check_syntax(!elem.tuple().empty());
                std::vector<Clingo::Symbol> tuple;
                tuple.reserve(elem.tuple().size());
                for (auto &&term : elem.tuple()) {
                    tuple.emplace_back(evaluate(term));
                }
                auto res = elem_ids.emplace(std::move(tuple), elems.size());
                auto xor_lit = elem.condition().empty() ? 0 : init.solver_literal(elem.condition_id());
                if (res.second) {
                    elems.emplace_back();
                    if (xor_lit != 0) {
                        elems.back().emplace_back(xor_lit);
                    }
                }
                else {
                    auto &lits = elems[res.first->second];
                    if (xor_lit != 0) {
                        lits.emplace_back(xor_lit);
                    }
                    else {
                        lits.clear();
                    }
                }
            }
            // sort conditions and find duplicates
            std::map<std::reference_wrapper<std::vector<Clingo::literal_t>>, size_t, std::less<std::vector<Clingo::literal_t>>> seen; // NOLINT
            for (auto &lits : elems) {
                std::sort(lits.begin(), lits.end());
                lits.erase(std::unique(lits.begin(), lits.end()), lits.end());
                seen[lits] += 1;
            }

            // gather literals for XOR constraint
            Value rhs{odd};
            std::vector<Clingo::literal_t> lhs_lits;
            for (auto &&lits : elems) {
                auto it = seen.find(lits);
                if (it->second % 2 == 0) {
                    continue;
                }
                it->second = 0;
                if (lits.empty()) {
                    rhs.flip();
                    continue;
                }
                Clingo::literal_t xor_lit{0};
                if (lits.size() == 1) {
                    lhs_lits.emplace_back(lits.front());
                    continue;
                }
                xor_lit = init.add_literal();
                std::vector<Clingo::literal_t> clause;
                clause.reserve(lits.size() + 1);
                clause.emplace_back(-xor_lit);
                for (auto &&eq_lit : lits) {
                    clause.emplace_back(eq_lit);
                    init.add_clause({-eq_lit, xor_lit});
                }
                init.add_clause(clause);
                lhs_lits.emplace_back(xor_lit);
            }

            // build XOR constraint over intermediate variables
            if (lhs_lits.empty()) {
                if (rhs && !init.add_clause({-lit})) {
                    return;
                }
                continue;
            }
            if (lhs_lits.size() == 1) {
                auto xor_lit = lhs_lits.front();
                if (!init.add_clause({-lit, rhs ? xor_lit : -xor_lit})) {
                    return;
                }
                continue;
            }
            std::vector<index_t> lhs_syms;
            for (auto eq_lit : lhs_lits) {
                auto res = var_map.try_emplace(eq_lit, var_map.size());
                if (res.second) {
                    // Note: With this setup, variables can have at most two
                    // bounds. Data structures could be optimized for this.
                    iqs.emplace_back(XORConstraint{{res.first->second}, Value{false}, -eq_lit});
                    iqs.emplace_back(XORConstraint{{res.first->second}, Value{true}, eq_lit});
                }
                lhs_syms.emplace_back(res.first->second);
            }
            iqs.emplace_back(XORConstraint{std::move(lhs_syms), rhs, lit});
        }
    }
}
