#include <parsing.hh>
#include <solving.hh>

#include <catch.hpp>

#include <sstream>

namespace {

using S = std::vector<std::vector<std::string>>;
using SV = std::vector<S>;

struct ModelHandler : Clingo::SolveEventHandler {
    ModelHandler(Propagator &prp)
    : prp{prp} { }
    bool on_model(Clingo::Model &model) override {
        res.emplace_back();
        auto symbols = model.symbols();
        std::sort(symbols.begin(), symbols.end());
        for (auto const &sym : symbols) {
            std::ostringstream ss;
            ss << sym;
            res.back().emplace_back(ss.str());
        }
        return true;
    }
    Propagator &prp;
    S res;
};

SV run_m(std::initializer_list<char const *> m) {
    Propagator prp{true};
    ModelHandler hnd{prp};
    Clingo::Control ctl{{"0"}};
    prp.register_control(ctl);
    SV res;

    int i = 0;
    int l = 0;
    for (auto const *s : m) {
        std::string n = "base" + std::to_string(i++);
        ctl.add(n.c_str(), {}, s);
        ctl.ground({{n.c_str(), {}}});
        ctl.solve(Clingo::LiteralSpan{}, &hnd, false, false).get();
        res.emplace_back();
        std::sort(hnd.res.begin(), hnd.res.end());
        std::swap(res.back(), hnd.res);
    }
    return res;
}

S run(char const *s) {
    return run_m({s}).front();
}

} // namespace

TEST_CASE("solving") {
    SECTION("single-shot") {
        REQUIRE(run("{x; y; z}.\n"
                    "&even { x:x; y:y }.\n"
                    "&odd  { x:x; z:z }.\n") == S{{"x", "y"}, {"z"}});

        REQUIRE(run("{x}.\n"
                    "&odd  { x:x }.\n"
                    "&even { x:x }.\n").empty());

        REQUIRE(run("{x; y}.\n"
                    "&odd  { x:x; y:y }.\n"
                    "&even { x:x; y:y }.\n"
                    "&even {      y:y }.\n").empty());

        REQUIRE(run("{x; y}.\n"
                    "&even { x:x; y:y }.\n"
                    "&odd  {      y:y }.\n"
                    "&odd  { x:x      }.\n") == S{{"x", "y"}});
    }
    SECTION("multi-shot") {
        REQUIRE(run_m({"{x; y; z}.\n"
                        "&even { x:x; y:y }.\n"
                        "&odd  { z:z }.\n",
                        "&odd  { x:x }.\n",
                        "&even { y:y }.\n"}) == SV{
                      {{"x","y","z"}, {"z"}},
                      {{"x", "y", "z"}}, {}});
        REQUIRE(run_m({"{x; y; a; b}.\n"
                       "&even { x: x, a; y: y, b }.\n",
                       "&odd  { 1: x }.\n"
                       "&even { 1: y }.\n",
                       ":- a.\n"
                       ":- b.\n"}) == SV{
                      {{},
                       {"a"},
                       {"a", "b"},
                       {"a", "b", "x", "y"},
                       {"a", "y"},
                       {"b"},
                       {"b", "x"},
                       {"x"},
                       {"x", "y"},
                       {"y"}},
                      {{"b", "x"},
                       {"x"}},
                      {{"x"}}});
    }
};

