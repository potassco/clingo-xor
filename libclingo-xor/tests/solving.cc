#include <parsing.hh>
#include <solving.hh>

#include <catch.hpp>

#include <sstream>

namespace {

bool run(char const *s) {
    Propagator prp{true};
    Clingo::Control ctl;
    prp.register_control(ctl);

    ctl.add("base", {}, s);
    ctl.ground({{"base", {}}});

    return ctl.solve(Clingo::LiteralSpan{}, nullptr, false, false).get().is_satisfiable();
}

bool run_q(char const *s) {
    Propagator prp{true};
    Clingo::Control ctl;
    prp.register_control(ctl);

    ctl.add("base", {}, s);
    ctl.ground({{"base", {}}});

    return ctl.solve(Clingo::LiteralSpan{}, nullptr, false, false).get().is_satisfiable();
}

size_t run_m(std::initializer_list<char const *> m) {
    Propagator prp{true};
    Clingo::Control ctl{{"0"}};
    prp.register_control(ctl);

    int i = 0;
    int l = 0;
    for (auto const *s : m) {
        std::string n = "base" + std::to_string(i++);
        ctl.add(n.c_str(), {}, s);
        ctl.ground({{n.c_str(), {}}});
        auto h = ctl.solve();
        for (auto const &m : h) {
            ++l;
        }
    }
    return l;
}

} // namespace

TEST_CASE("solving") {
    SECTION("single-shot") {
        REQUIRE( run("{x; y; z}.\n"
                     "&even { x:x; y:y }.\n"
                     "&odd  { x:x; z:z }.\n"));

        REQUIRE(!run("{x}.\n"
                     "&odd  { x:x }.\n"
                     "&even { x:x }.\n"));

        REQUIRE(!run("{x; y}.\n"
                     "&odd  { x:x; y:y }.\n"
                     "&even { x:x; y:y }.\n"
                     "&even {      y:y }.\n"));

        REQUIRE( run("{x; y}.\n"
                     "&even { x:x; y:y }.\n"
                     "&odd  {      y:y }.\n"
                     "&odd  { x:x      }.\n"));
    }
    SECTION("multi-shot") {
        REQUIRE( run_m({"{x; y; z}.\n"
                        "&even { x:x; y:y }.\n"
                        "&odd  { z:z }.\n",
                        "&odd  { x:x }.\n",
                        "&even { y:y }.\n"}) == 3);
        REQUIRE( run_m({"{x; y; a; b}.\n"
                        "&even { x: x, a; y: y, b }.\n",
                        "&odd  { 1: x }.\n"
                        "&even { 1: y }.\n",
                        ":- a.\n"
                        ":- b.\n"}) == 13);
    }
};

