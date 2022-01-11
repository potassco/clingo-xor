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

#ifndef CLINGOXOR_H
#define CLINGOXOR_H

//! Major version number.
#define CLINGOXOR_VERSION_MAJOR 1
//! Minor version number.
#define CLINGOXOR_VERSION_MINOR 1
//! Revision number.
#define CLINGOXOR_VERSION_REVISION 1
//! String representation of version.
#define CLINGOXOR_VERSION "1.1.1"

#ifdef __cplusplus
extern "C" {
#endif

#if defined _WIN32 || defined __CYGWIN__
#   define CLINGOXOR_WIN
#endif
#ifdef CLINGOXOR_NO_VISIBILITY
#   define CLINGOXOR_VISIBILITY_DEFAULT
#   define CLINGOXOR_VISIBILITY_PRIVATE
#else
#   ifdef CLINGOXOR_WIN
#       ifdef CLINGOXOR_BUILD_LIBRARY
#           define CLINGOXOR_VISIBILITY_DEFAULT __declspec (dllexport)
#       else
#           define CLINGOXOR_VISIBILITY_DEFAULT __declspec (dllimport)
#       endif
#       define CLINGOXOR_VISIBILITY_PRIVATE
#   else
#       if __GNUC__ >= 4
#           define CLINGOXOR_VISIBILITY_DEFAULT  __attribute__ ((visibility ("default")))
#           define CLINGOXOR_VISIBILITY_PRIVATE __attribute__ ((visibility ("hidden")))
#       else
#           define CLINGOXOR_VISIBILITY_DEFAULT
#           define CLINGOXOR_VISIBILITY_PRIVATE
#       endif
#   endif
#endif

#include <clingo.h>

enum clingoxor_value_type {
    clingoxor_value_type_int = 0,
    clingoxor_value_type_double = 1,
    clingoxor_value_type_symbol = 2
};
typedef int clingoxor_value_type_t;

typedef struct clingoxor_value {
    clingoxor_value_type_t type;
    union {
        int int_number;
        double double_number;
        clingo_symbol_t symbol;
    };
} clingoxor_value_t;

//! Callback to rewrite statements (see ::clingoxor_rewrite_ast).
typedef bool (*clingoxor_ast_callback_t)(clingo_ast_t *ast, void *data);

typedef struct clingoxor_theory clingoxor_theory_t;

//! Return the version of the theory.
CLINGOXOR_VISIBILITY_DEFAULT void clingoxor_version(int *major, int *minor, int *patch);

//! creates the theory
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_create(clingoxor_theory_t **theory);

//! registers the theory with the control
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_register(clingoxor_theory_t *theory, clingo_control_t* control);

//! Rewrite asts before adding them via the given callback.
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_rewrite_ast(clingoxor_theory_t *theory, clingo_ast_t *ast, clingoxor_ast_callback_t add, void *data);

//! prepare the theory between grounding and solving
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_prepare(clingoxor_theory_t *theory, clingo_control_t* control);

//! destroys the theory, currently no way to unregister a theory
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_destroy(clingoxor_theory_t *theory);

//! configure theory manually (without using clingo's options facility)
//! Note that the theory has to be configured before registering it and cannot be reconfigured.
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_configure(clingoxor_theory_t *theory, char const *key, char const *value);

//! add options for your theory
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_register_options(clingoxor_theory_t *theory, clingo_options_t* options);

//! validate options for your theory
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_validate_options(clingoxor_theory_t *theory);

//! callback on every model
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_on_model(clingoxor_theory_t *theory, clingo_model_t* model);

//! obtain a symbol index which can be used to get the value of a symbol
//! returns true if the symbol exists
//! does not throw
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_lookup_symbol(clingoxor_theory_t *theory, clingo_symbol_t symbol, size_t *index);

//! obtain the symbol at the given index
//! does not throw
CLINGOXOR_VISIBILITY_DEFAULT clingo_symbol_t clingoxor_get_symbol(clingoxor_theory_t *theory, size_t index);

//! initialize index so that it can be used with clingoxor_assignment_next
//! does not throw
CLINGOXOR_VISIBILITY_DEFAULT void clingoxor_assignment_begin(clingoxor_theory_t *theory, uint32_t thread_id, size_t *index);

//! move to the next index that has a value
//! returns true if the updated index is valid
//! does not throw
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_assignment_next(clingoxor_theory_t *theory, uint32_t thread_id, size_t *index);

//! check if the symbol at the given index has a value
//! does not throw
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_assignment_has_value(clingoxor_theory_t *theory, uint32_t thread_id, size_t index);

//! get the symbol and it's value at the given index
//! does not throw
CLINGOXOR_VISIBILITY_DEFAULT void clingoxor_assignment_get_value(clingoxor_theory_t *theory, uint32_t thread_id, size_t index, clingoxor_value_t *value);

//! callback on statistic updates
/// please add a subkey with the name of your theory
CLINGOXOR_VISIBILITY_DEFAULT bool clingoxor_on_statistics(clingoxor_theory_t *theory, clingo_statistics_t* step, clingo_statistics_t* accu);

#ifdef __cplusplus
}
#endif

#endif
