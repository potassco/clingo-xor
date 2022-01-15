#include "solving.hh"
#include "parsing.hh"

#include <unordered_set>

bool Solver::Variable::update_bound(Solver &s, Clingo::Assignment ass, Bound const &bound) {
    if (!has_bound()) {
        // Propagation: the number of free variables in the row decreases.
        // Here should be a good place to update the count.
        s.bound_trail_.emplace_back(bound.variable);
        this->bound = &bound;
    }
    return this->bound->value == bound.value;
}

void Solver::Variable::flip_value(Solver &s, index_t lvl) {
    // We can always assume that the assignment on a previous level was satisfying.
    // Thus, we simply store the old values to be able to restore them when backtracking.
    if (lvl != level) {
        // This assumes that the variable is contained in the variables_ vector
        // of the solver.
        s.assignment_trail_.emplace_back(level, this - s.variables_.data(), value);
        level = lvl;
    }
    value.flip();
}

bool Solver::Variable::has_conflict() const {
    return has_bound() && value != bound->value;
}

void Statistics::reset() {
    *this = {};
}

Solver::Solver(std::vector<XORConstraint> const &inequalities, bool enable_propagate)
: inequalities_{inequalities}
, enable_propagate_{enable_propagate}
{ }

Solver::Variable &Solver::basic_(index_t i) {
    assert(i < n_basic_);
    return variables_[variables_[i + n_non_basic_].index];
}

Solver::Variable &Solver::non_basic_(index_t j) {
    assert(j < n_non_basic_);
    return variables_[variables_[j].index];
}

void Solver::enqueue_(index_t i) {
    assert(i < n_basic_);
    auto ii = variables_[i + n_non_basic_].index;
    auto &xi = variables_[ii];
    if (!xi.queued && xi.has_conflict()) {
        conflicts_.emplace(ii);
        xi.queued = true;
    }
}

Value Solver::get_value(index_t i) const {
    return variables_[i].value;
}

bool Solver::prepare(Clingo::PropagateInit &init, size_t n_variables) {
    auto ass = init.assignment();

    // initialize non-basic variables
    variables_.resize(n_variables);
    n_non_basic_ = n_variables;
    for (index_t i = 0; i != n_non_basic_; ++i) {
        variables_[i].index = i;
        variables_[i].reverse_index = i;
    }

    // setup tableaux, bounds, and basic variables
    for (auto const &x : inequalities_) {
        if (ass.is_false(x.lit)) {
            continue;
        }

        // check bound against 0
        if (x.lhs.empty()) {
            if (x.rhs && !init.add_clause({-x.lit})) {
                return false;
            }
        }
        // add a bound to a non-basic variable
        else if (x.lhs.size() == 1) {
            auto j = x.lhs.front();
            auto it = bounds_.emplace(x.lit, Bound{
                Value{x.rhs},
                variables_[j].index,
                x.lit});
            variables_[j].bounds.emplace_back(&it->second);
        }
        // add an xor constraint
        else {
            // add basic variable
            auto index = variables_.size();
            variables_.emplace_back();
            variables_.back().index = index;
            variables_.back().reverse_index = index;
            auto i = n_basic_++;
            // add bound
            auto it = bounds_.emplace(x.lit, Bound{
                Value{x.rhs},
                static_cast<index_t>(variables_.size() - 1),
                x.lit});
            variables_.back().bounds.emplace_back(&it->second);
            // set tableaux
            for (auto j : x.lhs) {
                tableau_.set(i, j, true);
            }
        }
    }

    for (size_t i = 0; i < n_basic_; ++i) {
        enqueue_(i);
        propagate_row_(i);
    }

    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());

    return true;
}

bool Solver::propagate_(Clingo::PropagateControl &ctl) {
    bool ret = true;

    for (auto i : propagate_set_) {
        conflict_clause_.clear();
        size_t num_free = 0;
        Variable *free = nullptr;
        tableau_.update_row(i, [&](index_t j) {
            auto &xj = non_basic_(j);
            if (!xj.has_bound()) {
                num_free += 1;
                free = &xj;
            }
            else {
                conflict_clause_.emplace_back(-xj.bound->lit);
            }
            return num_free <= 1;
        });
        auto &xi = basic_(i);
        if (!xi.has_bound()) {
            num_free += 1;
            free = &xi;
        }
        else {
            conflict_clause_.emplace_back(-xi.bound->lit);
        }
        if (num_free == 1) {
            size_t num = 0;
            bool sat = false;
            for (auto *bound : free->bounds) {
                auto lit = free->value == bound->value ? bound->lit : -bound->lit;
                // TODO: This case can apparently occur during
                // multi-shot solving on level 0. Maybe this
                // can be avoided so that we could rather add
                // an assertion.
                if (ctl.assignment().is_true(lit)) {
                    sat = true;
                    break;
                }
                if (num > 0 && conflict_clause_.back() == lit) {
                    continue;
                }
                conflict_clause_.emplace_back(lit);
                ++num;
            }
            // TODO: Can this add non-unit clauses given the way
            // constraints are build?
            if (!sat && !ctl.add_clause(conflict_clause_)) {
                ret = false;
                break;
            }
        }
    }

    for (auto i : propagate_set_) {
        variables_[i].in_propagate_set = false;
    }
    propagate_set_.clear();

    return ret;
}

bool Solver::solve(Clingo::PropagateControl &ctl, Clingo::LiteralSpan lits) {
    index_t i{0};
    index_t j{0};

    auto ass = ctl.assignment();
    auto level = ass.decision_level();

    if (trail_offset_.empty() || trail_offset_.back().level < level) {
        trail_offset_.emplace_back(TrailOffset{
            ass.decision_level(),
            static_cast<index_t>(bound_trail_.size()),
            static_cast<index_t>(assignment_trail_.size())});
    }

    for (auto lit : lits) {
        for (auto it = bounds_.find(lit), ie = bounds_.end(); it != ie && it->first == lit; ++it) {
            auto const &[lit, bound] = *it;
            auto &x = variables_[bound.variable];
            if (!x.update_bound(*this, ctl.assignment(), bound)) {
                conflict_clause_.clear();
                conflict_clause_.emplace_back(-bound.lit);
                conflict_clause_.emplace_back(-x.bound->lit);
                ctl.add_clause(conflict_clause_);
                return false;
            }
            if (x.reverse_index < n_non_basic_) {
                if (x.has_bound() && x.value != x.bound->value) {
                    update_(level, x.reverse_index);
                }
                else {
                    propagate_col_(x.reverse_index);
                }
            }
            else {
                auto i = x.reverse_index - n_non_basic_;
                enqueue_(i);
                propagate_row_(i);
            }
        }
    }

    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());

    while (true) {
        switch (select_(i, j)) {
            case State::Satisfiable: {
#ifdef CLINGOLP_KEEP_SAT_ASSIGNMENT
                for (auto &[level, index, number] : assignment_trail_) {
                    variables_[index].level = 0;
                }
                for (auto it = trail_offset_.rbegin(), ie = trail_offset_.rend(); it != ie; ++it) {
                    if (it->assignment > 0) {
                        it->assignment = 0;
                    }
                    else {
                        break;
                    }
                }
                assignment_trail_.clear();
#endif
                return propagate_(ctl);
            }
            case State::Unsatisfiable: {
                ctl.add_clause(conflict_clause_);
                return false;
            }
            case State::Unknown: {
                pivot_(level, i, j);
            }
        }
    }
}

void Solver::undo() {
    // this function restores the last satisfying assignment
    auto &offset = trail_offset_.back();

    // undo bound updates
    for (auto it = bound_trail_.begin() + offset.bound, ie = bound_trail_.end(); it != ie; ++it) {
        variables_[*it].bound = nullptr;
    }
    bound_trail_.resize(offset.bound);

    // undo assignments
    for (auto it = assignment_trail_.begin() + offset.assignment, ie = assignment_trail_.end(); it != ie; ++it) {
        auto &[level, index, number] = *it;
        variables_[index].level = level;
        variables_[index].value = number;
    }
    assignment_trail_.resize(offset.assignment);

    // empty queue
    for (; !conflicts_.empty(); conflicts_.pop()) {
        variables_[conflicts_.top()].queued = false;
    }

    trail_offset_.pop_back();

    assert_extra(check_solution_());
}

Statistics const &Solver::statistics() const {
    return statistics_;
}

bool Solver::check_tableau_() {
    for (index_t i{0}; i < n_basic_; ++i) {
        Value v_i;
        tableau_.update_row(i, [&](index_t j){
            v_i ^= non_basic_(j).value;
            return true;
        });
        if (v_i != basic_(i).value) {
            return false;
        }
    }
    return true;
}

bool Solver::check_basic_() {
    for (index_t i = 0; i < n_basic_; ++i) {
        auto &xi = basic_(i);
        if (xi.has_bound() && xi.value != xi.bound->value && !xi.queued) {
            return false;
        }
    }
    return true;
}

bool Solver::check_non_basic_() {
    for (index_t j = 0; j < n_non_basic_; ++j) {
        auto &xj = non_basic_(j);
        if (xj.has_bound() && xj.value != xj.bound->value) {
            return false;
        }
    }
    return true;
}

bool Solver::check_solution_() {
    for (auto &x : variables_) {
        if (x.has_bound() && x.bound->value != x.value) {
            return false;
        }
    }
    return check_tableau_() && check_basic_();
}

void Solver::propagate_row_(index_t i) {
    if (enable_propagate_ && !variables_[i].in_propagate_set) {
        propagate_set_.emplace_back(i);
        variables_[i].in_propagate_set = true;
    }
}

void Solver::propagate_col_(index_t j) {
    if (enable_propagate_) {
        tableau_.update_col(j, [&](index_t i) { propagate_row_(i); });
    }
}

void Solver::update_(index_t level, index_t j) {
    auto &xj = non_basic_(j);
    tableau_.update_col(j, [&](index_t i) {
        basic_(i).flip_value(*this, level);
        enqueue_(i);
        propagate_row_(i);
    });
    xj.flip_value(*this, level);
}

void Solver::pivot_(index_t level, index_t i, index_t j) {
    auto &xi = basic_(i);
    auto &xj = non_basic_(j);

    // adjust assignment
    xi.flip_value(*this, level);
    xj.flip_value(*this, level);
    tableau_.update_col(j, [&](index_t k) {
        if (k != i) {
            basic_(k).flip_value(*this, level);
            enqueue_(k);
            propagate_row_(k);
        }
    });
    assert_extra(check_tableau_());

    // swap variables x_i and x_j
    std::swap(xi.reverse_index, xj.reverse_index);
    std::swap(variables_[i + n_non_basic_].index, variables_[j].index);
    enqueue_(i);

    // eliminate x_j from rows k != i
    // Propagation: this operation changes the number of free variables in a row
    tableau_.eliminate(i, j);

    ++statistics_.pivots_;
    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());
}

bool Solver::flippable_(Variable const &x) {
    if (!x.has_bound() || x.value != x.bound->value) {
        return true;
    }
    if (x.has_bound()) {
        conflict_clause_.emplace_back(-x.bound->lit);
    }
    return false;
}

Solver::State Solver::select_(index_t &ret_i, index_t &ret_j) {
    // This implements Bland's rule selecting the variables with the smallest
    // indices for pivoting.

    for (; !conflicts_.empty(); conflicts_.pop()) {
        auto ii = conflicts_.top();
        auto &xi = variables_[ii];
        auto i = xi.reverse_index;
        assert(ii == variables_[i].index);
        xi.queued = false;
        // the queue might contain variables that meanwhile became basic
        if (i < n_non_basic_) {
            continue;
        }
        i -= n_non_basic_;

        if (xi.has_bound() && xi.value != xi.bound->value) {
            conflict_clause_.clear();
            conflict_clause_.emplace_back(-xi.bound->lit);
            index_t kk = variables_.size();
            tableau_.update_row(i, [&](index_t j) {
                auto jj = variables_[j].index;
                if (jj < kk && flippable_(variables_[jj])) {
                    kk = jj;
                    ret_i = i;
                    ret_j = j;
                }
                return true;
            });
            if (kk == variables_.size()) {
                return State::Unsatisfiable;
            }
            return State::Unknown;
        }
    }

    assert(check_solution_());

    return State::Satisfiable;
}

Propagator::Propagator(bool enable_propagate)
: enable_propagate_{enable_propagate}  {
}

void Propagator::init(Clingo::PropagateInit &init) {
    facts_offset_ = facts_.size();
    if (facts_offset_ > 0) {
        init.set_check_mode(Clingo::PropagatorCheckMode::Partial);
    }

    evaluate_theory(init, var_map_, iqs_);
    // add watches
    for (auto &x : iqs_) {
        init.add_watch(x.lit);
    }

    slvs_.clear();
    slvs_.reserve(init.number_of_threads());
    for (size_t i = 0, e = init.number_of_threads(); i != e; ++i) {
        slvs_.emplace_back(
            std::piecewise_construct,
            std::forward_as_tuple(0),
            std::forward_as_tuple(iqs_, enable_propagate_));
        if (!slvs_.back().second.prepare(init, var_map_.size())) {
            return;
        }
    }
}

void Propagator::register_control(Clingo::Control &ctl) {
    ctl.register_propagator(*this);
    ctl.add("base", {}, THEORY);
}

void Propagator::on_statistics(Clingo::UserStatistics step, Clingo::UserStatistics accu) {
    auto step_simplex = step.add_subkey("Simplex", Clingo::StatisticsType::Map);
    auto step_pivots = step_simplex.add_subkey("Pivots", Clingo::StatisticsType::Value);
    auto accu_simplex = accu.add_subkey("Simplex", Clingo::StatisticsType::Map);
    auto accu_pivots = accu_simplex.add_subkey("Pivots", Clingo::StatisticsType::Value);
    for (auto const &[offset, slv] : slvs_) {
        step_pivots.set_value(slv.statistics().pivots_);
        accu_pivots.set_value(accu_pivots.value() + slv.statistics().pivots_);
    }
}

void Propagator::check(Clingo::PropagateControl &ctl) {
    auto ass = ctl.assignment();
    auto &[offset, slv] = slvs_[ctl.thread_id()];
    if (ass.decision_level() == 0 && offset < facts_offset_) {
        if (!slv.solve(ctl, Clingo::LiteralSpan{facts_.data() + offset, facts_offset_})) { // NOLINT
            return;
        }
        offset = facts_offset_;
    }
}

void Propagator::propagate(Clingo::PropagateControl &ctl, Clingo::LiteralSpan changes) {
    auto ass = ctl.assignment();
    if (ass.decision_level() == 0 && ctl.thread_id() == 0) {
        facts_.insert(facts_.end(), changes.begin(), changes.end());
    }
    auto &[offset, slv] = slvs_[ctl.thread_id()];
    if (!slv.solve(ctl, changes)) {
        return;
    }
}

void Propagator::undo(Clingo::PropagateControl const &ctl, Clingo::LiteralSpan changes) noexcept {
    slvs_[ctl.thread_id()].second.undo();
}
