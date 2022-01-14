# clingo modulo simplex

A simplistic simplex solver for checking satisfiability of a set of XOR
constraints.

The system is an adjusted version of the clingo-lpx system. Interestingly, the
satisfiable of a set of XOR constraints can be determined using (a simplified
version of) the simplex algorithm.

## Input

The system supports `&even` and `&odd` constraints over integers in rule heads.
The elements of the constraint are tuples with conditions where the first
element of the tuple must be an integer.

The following program corresponds to `p(1) xor p(2) xor p(3)`:

```
{ p(1..3) }.

&odd { 1,X: p(X) }.
```

## Installation

To compile the package, [cmake], [clingo], and a C++ compiler supporting C++17
have to be installed. All these requirements can be installed with [anaconda].

```bash
conda create -n simplex -c potassco/label/dev cmake ninja clingo gxx_linux-64
conda activate simplex
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

[cmake]: https://cmake.org
[clingo]: https://github.com/potassco/clingo
[anaconda]: https://anaconda.org

## Profiling

Profiling with the [gperftools] can be enabled via cmake.

```bash
conda create -n profile -c conda-forge -c potassco/label/dev cmake ninja clingo gxx_linux-64 gperftools
conda activate profile
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCLINGOLP_PROFILE=ON
cmake --build build
```

The resulting binary can then be profiled using the following calls:

```bash
CPUPROFILE_FREQUENCY=1000 ./build/clingo-xor examples/xor.lp --stats -c n=132 -q 0
google-pprof --gv ./build/clingo-xor profile.out
```

[gperftools]: https://gperftools.github.io/gperftools/cpuprofile.html

## Literature

- "Integrating Simplex with `DPLL(T)`" by Bruno Dutertre and Leonardo de Moura

## TODO

- propagate atoms
  - **we can maintain a counter for each row that stores how many variabless in the row do not have bounds**
  - if this counter reaches 1, the remaining variable can be propagated
- values could be packed better because they only need one bit (this is probably not worth the effort)
