#include "../src/parsing.hh"
#include "catch.hpp"

#include <sstream>

template <typename T>
std::string str(T &&x) {
    std::ostringstream oss;
    oss << x;
    return oss.str();
}

TEST_CASE("parsing") {
    Clingo::Control ctl;
    ctl.add("base", {}, THEORY);
    Clingo::literal_t n = 0;

    // TODO: needs a propagator to test

    SECTION("example 1") {
        ctl.add("base", {}, "{x; y}. &even { 1,x: x; 1,y: y }.\n");
        ctl.ground({{"base", {}}});

        /*
        VarMap vars;
        std::vector<Inequality> eqs;
        evaluate_theory(ctl.theory_atoms(), mapper, vars, eqs);
        REQUIRE(eqs.size() == 5);
        REQUIRE(str(eqs.back()) == "#aux_0 + #aux_1 = 0 :- lit_3");
        */
    }

    SECTION("example 2") {
        ctl.add("base", {}, "&even { 1,x }.\n");
        ctl.ground({{"base", {}}});

        /*
        VarMap vars;
        std::vector<Inequality> eqs;
        evaluate_theory(ctl.theory_atoms(), mapper, vars, eqs);
        REQUIRE(eqs.size() == 1);
        REQUIRE(str(eqs.front()) == "0 = 1 :- lit_1");
        */
    }

    SECTION("example 3") {
        ctl.add("base", {}, "&odd {  1,x; 1,y }.\n");
        ctl.ground({{"base", {}}});

        /*
        VarMap vars;
        std::vector<Inequality> eqs;
        evaluate_theory(ctl.theory_atoms(), mapper, vars, eqs);
        REQUIRE(eqs.size() == 1);
        REQUIRE(str(eqs.front()) == "0 = 1 :- lit_1");
        */
    }
};

