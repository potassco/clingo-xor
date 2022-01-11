# clingo modulo simplex

A simplistic simplex solver for checking satisfiability of a set of XOR
constraints.

The system is a slightly adjusted version of the clingo-lpx system. I just
wanted to see if it possible to apply the simplex algorithm to XOR constraints.
A system to solve XOR constraints efficiently still needs improvements. Since
the system uses Bland's rule to select pivot elements, it also needs some
thought whether the algorithm always terminates.


## Example

Given its inheritance to clingo-lpx, the input to the system might look a bit
unusual. Ideally, there would be a dedicated XOR constraint and no need to
connect atoms and integer variables. For now, we can express the constraint
`XOR(x,y)` as follows:

```
{ x; y }.

&sum { x; y } >= 1.

&sum { x } >= 1 :-     x.
&sum { x } <= 0 :- not x.

&sum { y } >= 1 :-     y.
&sum { y } <= 0 :- not y.
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

## Input format

It does not make much sense, but clingo-xor shares the same input syntax as
clingo-lpx. All arithmetics is performed modulo 2. This means we have
```
x = -x = |x| mod 2`.
```

The system supports `&sum` constraints over rationals with relations among
`<=`, `>=`, and `=`. The elements of the sum constraints must have form `x`,
`-x`, `n*x`, `-n*x`, or `-n/d*x` where `x` is a function symbol or tuple, and
`n` and `d` are non-negative integers.

For example, the following program is accepted:
```
{ x }.
&sum { x; -y; 2*x; -3*y; 2/3*x; -3/4*y } >= 100 :- x.
```

Furthermore, `&dom` constraints are supported, which are shortcuts for `&sum`
constraints. The program
```
{ x }.
&dom { 1..2 } = x.
```
is equivalent to
```
{ x }.
&sum { x } >= 1.
&sum { x } <= 2.
```

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
