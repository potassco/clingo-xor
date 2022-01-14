#include <parsing.hh>
#include <solving.hh>

#include <catch.hpp>

#include <sstream>

namespace {

struct ModelHandler : Clingo::SolveEventHandler {
    ModelHandler(Propagator &prp)
    : prp{prp} { }
    bool next(uint32_t thread_id, size_t *current) const {
        while (*current < prp.n_values(thread_id)) {
            ++*current;
            return true;
        }
        return false;
    }
    bool on_model(Clingo::Model &model) override {
        std::vector<Clingo::Symbol> symbols;
        auto thread_id = model.thread_id();
        for (size_t i = 0; next(thread_id, &i);) {
            ss.str("");
            ss << prp.get_value(thread_id, i - 1);
            symbols.emplace_back(Clingo::Function("__xor", {prp.get_symbol(i - 1), Clingo::String(ss.str().c_str())}));
        }
        model.extend(symbols);
        return true;
    }
    static void print_model(Clingo::Model const &m, char const *ident) {
        std::cerr << ident << "Model:";
        auto symbols = m.symbols();
        std::sort(symbols.begin(), symbols.end());
        for (auto const &sym : symbols) {
            std::cerr << " ";
            std::cerr << sym;
        }
        std::cerr << "\n" << ident << "Assignment:";
        symbols = m.symbols(Clingo::ShowType::Theory);
        std::sort(symbols.begin(), symbols.end());
        for (auto const &sym : symbols) {
            if (std::strcmp("__xor", sym.name()) != 0) {
                continue;
            }
            std::cerr << " ";
            auto args = sym.arguments();
            std::cerr << args.front() << "=" << args.back().string();
        }
        std::cerr << std::endl;
    }
    Propagator &prp;
    std::ostringstream ss;
};

bool run(char const *s) {
    Propagator prp;
    Clingo::Control ctl;
    prp.register_control(ctl);

    ctl.add("base", {}, s);
    ctl.ground({{"base", {}}});

    return ctl.solve(Clingo::LiteralSpan{}, nullptr, false, false).get().is_satisfiable();
}

bool run_q(char const *s) {
    Propagator prp;
    Clingo::Control ctl;
    prp.register_control(ctl);

    ctl.add("base", {}, s);
    ctl.ground({{"base", {}}});

    return ctl.solve(Clingo::LiteralSpan{}, nullptr, false, false).get().is_satisfiable();
}

size_t run_m(std::initializer_list<char const *> m) {
    Propagator prp;
    ModelHandler hnd{prp};
    Clingo::Control ctl{{"0"}};
    prp.register_control(ctl);

    int i = 0;
    int l = 0;
    for (auto const *s : m) {
        std::string n = "base" + std::to_string(i++);
        ctl.add(n.c_str(), {}, s);
        ctl.ground({{n.c_str(), {}}});
        auto h = ctl.solve(Clingo::LiteralSpan{}, &hnd);
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

