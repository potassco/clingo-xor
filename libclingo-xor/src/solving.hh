#pragma once

#include "problem.hh"
#include "parsing.hh"
#include "util.hh"

#include <queue>
#include <map>
#include <optional>

struct Statistics {
    void reset();

    Timer total;
    Timer propagate;
    size_t pivots{0};
    size_t sat{0};
    size_t unsat{0};
};

//! A solver for finding an assignment satisfying a set of inequalities.
class Solver {
private:
    //! The bounds associated with a Variable.
    //!
    //! In practice, there should be a lot of variables with just one bound.
    struct Bound {
        Value value;
        index_t variable{0};
        Clingo::literal_t lit{0};
    };
    //! Capture the current state of a variable.
    struct Variable {
        //! Adjusts the bound of the variable if possible.
        [[nodiscard]] bool update_bound(Solver &s, Clingo::Assignment ass, Bound const &bound);
        //! Check if te value of the variable conflicts with its bound.
        [[nodiscard]] bool has_conflict() const;
        //! Check if the variable has a bound.
        [[nodiscard]] bool has_bound() const { return bound != nullptr; }
        //! Flip the value of the variable.
        void flip_value(Solver &s, index_t level);

        //! The bounds associated with the variable.
        std::vector<Bound*> bounds;
        //! The bound of a variable.
        Bound const *bound{nullptr};
        //! Helper index for pivoting variables.
        index_t index{0};
        //! Helper index to obtain row/column index of a variable.
        index_t reverse_index{0};
        //! The level the variable was assigned on.
        index_t level{0};
        //! The current value of the variable.
        Value value{false};
        //! Whether this variales is in the queue of conflicting variables.
        bool queued{false};
        //! Whether the row has to be propagated.
        //!
        //! Note that this is not really associated with the variable. This is
        //! just a convenient location to store the flag.
        bool in_propagate_set{false};
    };
    struct TrailOffset {
        index_t level;
        index_t bound;
        index_t assignment;
    };
    //! Captures what is know about of the satisfiability of a problem while
    //! solving.
    enum class State {
        Satisfiable = 0,
        Unsatisfiable = 1,
        Unknown = 2
    };

public:
    //! Construct a new solver object.
    Solver(std::vector<XORConstraint> const &inequalities, bool enable_propagate);

    //! Prepare inequalities for solving.
    [[nodiscard]] bool prepare(Clingo::PropagateInit &init, size_t n_variables);

    //! Solve the (previously prepared) problem.
    //!
    //! If the function returns false, the solver has to backtrack.
    [[nodiscard]] bool solve(Clingo::PropagateControl &ctl, Clingo::LiteralSpan lits);

    //! Undo assignments on the current level.
    void undo();

    //! Get the currently assigned value.
    [[nodiscard]] Value get_value(index_t i) const;

    //! Return the solve statistics.
    [[nodiscard]] Statistics const &statistics() const;

private:
    //! Check if the tableau.
    [[nodiscard]] bool check_tableau_();
    //! Check if basic variables with unsatisfied bounds are enqueued.
    [[nodiscard]] bool check_basic_();
    //! Check if bounds of non-basic variables are satisfied.
    [[nodiscard]] bool check_non_basic_();
    //! Check if the current assignment is a solution.
    [[nodiscard]] bool check_solution_();

    //! Enqueue basic variable `x_i` if it is conflicting.
    void enqueue_(index_t i);

    //! Mark row `i` for propagation.
    void propagate_row_(index_t i);
    //! Mark rows in column `j` for propagation.
    void propagate_col_(index_t j);

    //! Propagate marked rows.
    bool propagate_(Clingo::PropagateControl &ctl);

    //! Flip the value of non-basic `x_j` variable.
    void update_(index_t level, index_t j);

    //! Pivots basic variable `x_i` and non-basic variable `x_j`.
    void pivot_(index_t level, index_t i, index_t j);

    //! Check if the given non-basic variable is flippable.
    //!
    //! If the variable cannot be flipped, the literal of its bound is
    //! proactively added to the conflict clause as a side effect.
    [[nodiscard]] bool flippable_(Variable const &x);
    //! Select pivot point using Bland's rule.
    //!
    //! If the problem is unsatisfiable, the conflict clause is set as a side
    //! effect.
    State select_(index_t &ret_i, index_t &ret_j);

    //! Get basic variable associated with row `i`.
    Variable &basic_(index_t i);
    //! Get non-basic variable associated with column `j`.
    Variable &non_basic_(index_t j);

    //! The set of inequalities.
    std::vector<XORConstraint> const &inequalities_;
    //! Mapping from literals to bounds.
    std::unordered_multimap<Clingo::literal_t, Bound> bounds_;
    //! Trail of bound assignments (variable, relation, Value).
    std::vector<index_t> bound_trail_;
    //! Trail for assignments (level, variable, Value).
    std::vector<std::tuple<index_t, index_t, Value>> assignment_trail_;
    //! Trail offsets per level.
    std::vector<TrailOffset> trail_offset_;
    //! The tableau of coefficients.
    Tableau tableau_;
    //! The non-basic and basic variables.
    std::vector<Variable> variables_;
    //! The set of conflicting variables.
    std::priority_queue<index_t, std::vector<index_t>, std::greater<>> conflicts_;
    //! The conflict clause.
    std::vector<Clingo::literal_t> conflict_clause_;
    //! The rowes to be propagated.
    std::vector<Clingo::literal_t> propagate_set_;
    //! Problem and solving statistics.
    Statistics statistics_;
    //! The number of non-basic variables.
    index_t n_non_basic_{0};
    //! The number of basic variables.
    index_t n_basic_{0};
    //! Whether propagation is enabled.
    bool enable_propagate_;
};

class Propagator : public Clingo::Propagator {
public:
    Propagator(bool enable_propagate);
    Propagator(Propagator const &) = default;
    Propagator(Propagator &&) noexcept = default;
    Propagator &operator=(Propagator const &) = default;
    Propagator &operator=(Propagator &&) noexcept = default;
    ~Propagator() override = default;
    void register_control(Clingo::Control &ctl);
    void on_statistics(Clingo::UserStatistics step, Clingo::UserStatistics accu);

    void init(Clingo::PropagateInit &init) override;
    void check(Clingo::PropagateControl &ctl) override;
    void propagate(Clingo::PropagateControl &ctl, Clingo::LiteralSpan changes) override;
    void undo(Clingo::PropagateControl const &ctl, Clingo::LiteralSpan changes) noexcept override;

private:
    VarMap var_map_;
    std::vector<XORConstraint> iqs_;
    size_t facts_offset_{0};
    std::vector<Clingo::literal_t> facts_;
    std::vector<std::pair<size_t, Solver>> slvs_;
    bool enable_propagate_;
};
