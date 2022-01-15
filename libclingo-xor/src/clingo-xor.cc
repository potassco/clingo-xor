// {{{ MIT License
//
// Copyright Roland Kaminski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// }}}

#include <clingo-xor.h>
#include "solving.hh"
#include "parsing.hh"

#include <sstream>

#ifdef CLINGOXOR_PROFILE

class Profiler {
public:
    Profiler(char const *path) {
        ProfilerStart(path);
    }
    ~Profiler() {
        ProfilerStop();
    }
};

#endif

#define CLINGOXOR_TRY try // NOLINT
#define CLINGOXOR_CATCH catch (...){ Clingo::Detail::handle_cxx_error(); return false; } return true // NOLINT

namespace {

using Clingo::Detail::handle_error;

//! C initialization callback for the XOR propagator.
bool init(clingo_propagate_init_t* i, void* data) {
    CLINGOXOR_TRY {
        Clingo::PropagateInit in(i);
        static_cast<Propagator*>(data)->init(in);
    }
    CLINGOXOR_CATCH;
}

//! C propagation callback for the XOR propagator.
bool propagate(clingo_propagate_control_t* i, const clingo_literal_t *changes, size_t size, void* data) {
    CLINGOXOR_TRY {
        Clingo::PropagateControl in(i);
        static_cast<Propagator*>(data)->propagate(in, {changes, size});
    }
    CLINGOXOR_CATCH;
}

//! C undo callback for the XOR propagator.
void undo(clingo_propagate_control_t const* i, const clingo_literal_t *changes, size_t size, void* data) {
    Clingo::PropagateControl in(const_cast<clingo_propagate_control_t *>(i)); // NOLINT
    static_cast<Propagator*>(data)->undo(in, {changes, size});
}

//! C check callback for the XOR propagator.
bool check(clingo_propagate_control_t* i, void* data) {
    CLINGOXOR_TRY {
        Clingo::PropagateControl in(i);
        static_cast<Propagator*>(data)->check(in);
    }
    CLINGOXOR_CATCH;
}

//! High level interface to use the XOR propagator.
class XORPropagatorFacade {
public:
    XORPropagatorFacade(clingo_control_t *control, char const *theory, bool enable_propagate)
    : prop_{enable_propagate} {
        handle_error(clingo_control_add(control, "base", nullptr, 0, theory));
        static clingo_propagator_t prop = {
            init,
            propagate,
            undo,
            check,
            nullptr
        };
        handle_error(clingo_control_register_propagator(control, &prop, &prop_, false));
    }

    //! Add the propagator statistics to clingo's statistics.
    void on_statistics(Clingo::UserStatistics& step, Clingo::UserStatistics &accu) {
        prop_.on_statistics(step, accu);
    }

private:
    Propagator prop_; //!< The underlying XOR propagator.
    std::ostringstream ss_;
};

//! Check if b is a lower case prefix of a returning a pointer to the remainder of a.
char const *iequals_pre(char const *a, char const *b) {
    for (; *a && *b; ++a, ++b) { // NOLINT
        if (tolower(*a) != tolower(*b)) { return nullptr; }
    }
    return *b != '\0' ? nullptr : a;
}

//! Check if two strings are lower case equal.
bool iequals(char const *a, char const *b) {
    a = iequals_pre(a, b);
    return a != nullptr && *a == '\0';
}

//! Parse a Boolean and store it in data.
//!
//! Return false if there is a parse error.
bool parse_bool(const char *value, void *data) {
    auto &result = *static_cast<bool*>(data);
    if (iequals(value, "no") || iequals(value, "off") || iequals(value, "0")) {
        result = false;
        return true;
    }
    if (iequals(value, "yes") || iequals(value, "on") || iequals(value, "1")) {
        result = true;
        return true;
    }
    return false;
}

//! Set the given error message if the Boolean is false.
//!
//! Return false if there is a parse error.
bool check_parse(char const *key, bool ret) {
    if (!ret) {
        std::ostringstream msg;
        msg << "invalid value for '" << key << "'";
        clingo_set_error(clingo_error_runtime, msg.str().c_str());
    }
    return ret;
}

} // namespace

struct clingoxor_theory {
    bool propagate{true};
    std::unique_ptr<XORPropagatorFacade> clingoxor{nullptr};
};

extern "C" void clingoxor_version(int *major, int *minor, int *patch) {
    if (major != nullptr) {
        *major = CLINGOXOR_VERSION_MAJOR;
    }
    if (minor != nullptr) {
        *minor = CLINGOXOR_VERSION_MINOR;
    }
    if (patch != nullptr) {
        *patch = CLINGOXOR_VERSION_REVISION;
    }
}

extern "C" bool clingoxor_create(clingoxor_theory_t **theory) {
    CLINGOXOR_TRY { *theory = new clingoxor_theory{}; } // NOLINT
    CLINGOXOR_CATCH;
}

extern "C" bool clingoxor_register(clingoxor_theory_t *theory, clingo_control_t* control) {
    CLINGOXOR_TRY {
        theory->clingoxor = std::make_unique<XORPropagatorFacade>(control, THEORY, theory->propagate);
    }
    CLINGOXOR_CATCH;
}

extern "C" bool clingoxor_rewrite_ast(clingoxor_theory_t *theory, clingo_ast_t *ast, clingoxor_ast_callback_t add, void *data) {
    static_cast<void>(theory);
    return add(ast, data);
}

extern "C" bool clingoxor_prepare(clingoxor_theory_t *theory, clingo_control_t *control) {
    static_cast<void>(theory);
    static_cast<void>(control);
    return true;
}

extern "C" bool clingoxor_destroy(clingoxor_theory_t *theory) {
    CLINGOXOR_TRY { delete theory; } // NOLINT
    CLINGOXOR_CATCH;
}

extern "C" bool clingoxor_configure(clingoxor_theory_t *theory, char const *key, char const *value) {
    CLINGOXOR_TRY {
        if (strcmp(key, "propagate") == 0) {
            return check_parse("propagate", parse_bool(value, &theory->propagate));
        }
        std::ostringstream msg;
        msg << "invalid configuration key '" << key << "'";
        clingo_set_error(clingo_error_runtime, msg.str().c_str());
        return false;
    }
    CLINGOXOR_CATCH;
}

extern "C" bool clingoxor_register_options(clingoxor_theory_t *theory, clingo_options_t* options) {
    CLINGOXOR_TRY {
        char const *group = "Clingo.XOR Options";
        handle_error(clingo_options_add_flag(options, group, "propagate",
            "Enable propagation [yes]",
            &theory->propagate));
    }
    CLINGOXOR_CATCH;
}

extern "C" bool clingoxor_validate_options(clingoxor_theory_t *theory) {
    static_cast<void>(theory);
    return true;
}

extern "C" bool clingoxor_on_model(clingoxor_theory_t *theory, clingo_model_t* model) {
    return true;
}

extern "C" bool clingoxor_lookup_symbol(clingoxor_theory_t *theory, clingo_symbol_t symbol, size_t *index) {
    return false;
}

extern "C" clingo_symbol_t clingoxor_get_symbol(clingoxor_theory_t *theory, size_t index) {
    return Clingo::Number(0).to_c();
}

extern "C" void clingoxor_assignment_begin(clingoxor_theory_t *theory, uint32_t thread_id, size_t *index) {
    static_cast<void>(theory);
    static_cast<void>(thread_id);
    *index = 0;
}

extern "C" bool clingoxor_assignment_next(clingoxor_theory_t *theory, uint32_t thread_id, size_t *index) {
    return false;
}

extern "C" bool clingoxor_assignment_has_value(clingoxor_theory_t *theory, uint32_t thread_id, size_t index) {
    return false;
}

extern "C" void clingoxor_assignment_get_value(clingoxor_theory_t *theory, uint32_t thread_id, size_t index, clingoxor_value_t *value) {
}

extern "C" bool clingoxor_on_statistics(clingoxor_theory_t *theory, clingo_statistics_t* step, clingo_statistics_t* accu) {
    CLINGOXOR_TRY {
        uint64_t root_s{0};
        uint64_t root_a{0};
        handle_error(clingo_statistics_root(step, &root_s));
        handle_error(clingo_statistics_root(accu, &root_a));
        Clingo::UserStatistics s(step, root_s);
        Clingo::UserStatistics a(accu, root_a);
        theory->clingoxor->on_statistics(s, a);
    }
    CLINGOXOR_CATCH;
}

#undef CLINGOXOR_TRY
#undef CLINGOXOR_CATCH
