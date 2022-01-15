#include "../src/parsing.hh"
#include "catch.hpp"

#include <sstream>

namespace {

struct TestPropagator : public Clingo::Propagator {
    void init(Clingo::PropagateInit &init) override {
        VarMap vars;
        evaluate_theory(init, vars, eqs);
    }
    std::vector<XORConstraint> eqs;
};

using S = std::vector<std::string>;

S evaluate(char const *prg) {
    TestPropagator prp;
    Clingo::Control ctl;
    ctl.register_propagator(prp);
    ctl.add("base", {}, THEORY);
    ctl.add("base", {}, prg);
    ctl.ground({{"base", {}}});
    ctl.solve(Clingo::LiteralSpan{}, nullptr, false, false).get();
    S ret;
    for (auto &eq : prp.eqs) {
        std::ostringstream ss;
        eq.lit = eq.lit > 0 ? 1 : -1;
        ss << eq;
        ret.emplace_back(ss.str());
    }
    return ret;
}

} // namespace

TEST_CASE("parsing") {
    // translation to clauses
    REQUIRE(evaluate("&even { x }.").empty());
    REQUIRE(evaluate("&odd  { x }.").empty());
    REQUIRE(evaluate("{x}. &odd  { x: x }.").empty());

    REQUIRE(evaluate("{x; y}. &even { x: x; y: y }.") == S{
        "var_0 = 0 :- not lit_1",       // x = 0
        "var_0 = 1 :- lit_1",           // x = 1
        "var_1 = 0 :- not lit_1",       // y = 0
        "var_1 = 1 :- lit_1",           // y = 1
        "var_0 + var_1 = 0 :- lit_1"}); // x + y = 0

    REQUIRE(evaluate("{x; y; z}. &even { x: x; yz: y; yz: z }.") == S{
        "var_0 = 0 :- not lit_1",       // x  = 0
        "var_0 = 1 :- lit_1",           // x  = 1
        "var_1 = 0 :- not lit_1",       // yz = 0
        "var_1 = 1 :- lit_1",           // yz = 1
        "var_0 + var_1 = 0 :- lit_1"}); // x + y = 0

};

