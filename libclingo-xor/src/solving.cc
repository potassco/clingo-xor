#include "solving.hh"
#include "parsing.hh"

#include <unordered_set>

bool Solver::Variable::update_bound(Solver &s, Clingo::Assignment ass, Bound const &bound) {
    if (!has_bound()) {
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
        // (guaranteed to have at least two elements)
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
    }

    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());

    statistics_.basic = n_basic_;
    statistics_.non_basic = n_non_basic_;
    statistics_.bounds = bounds_.size();
    statistics_.tableau_initial = tableau_.size();
    statistics_.tableau_average = tableau_.size();
    statistics_.tableau_average_n = 1;

    return true;
}

bool Solver::propagate_(Clingo::PropagateControl &ctl) {
    auto timer = statistics_.propagate.start();
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
                // Note: This case can happen if a bound is propagated but the
                // propagator has not yet been notified about the change.
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
            // Note: By construction, a variable has at most two bounds. In
            // case it has two bounds, they have opposite literals and values.
            // Thus, the conflict_clause_ is guaranteed to be unit-resulting.
            if (!sat && !ctl.add_clause(conflict_clause_)) {
                ret = false;
                break;
            }
        }
    }

    return ret;
}

bool Solver::solve(Clingo::PropagateControl &ctl, Clingo::LiteralSpan lits) {
    auto timer = statistics_.total.start();
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

    // NOTE: Initially, all rows in the tableaux are guaranteed to have at
    // least two elements. This means that initially there are no rows that can
    // be propagated because at least one bound has to be set. If any row
    // becomes unit-resulting, it is enqueued below and propagated at the end
    // in case the XOR constraints are found to be satisfiable.
    for (auto i : propagate_set_) {
        variables_[i].in_propagate_set = false;
    }
    propagate_set_.clear();

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
                // NOTE: The way rows are marked for propagation here is
                // probably not as efficient as it could be.
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
                // Note: This hopefully is not too expensive to compute.
                // Otherwise, size could also be maintained by the tableaux to
                // make this constant.
                ++statistics_.tableau_average_n;
                double n = statistics_.tableau_average_n;
                double a = (n - 1) / n;
                statistics_.tableau_average *= a;
                statistics_.tableau_average += tableau_.size() / n;
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

    ++statistics_.pivots;
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
                ++statistics_.unsat;
                return State::Unsatisfiable;
            }
            return State::Unknown;
        }
    }

    assert(check_solution_());

    ++statistics_.sat;
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
    auto simplex = accu.add_subkey("Simplex", Clingo::StatisticsType::Map);
    auto tableaux = simplex.add_subkey("Initial Tableau Size", Clingo::StatisticsType::Value);
    auto basic = simplex.add_subkey("Basic", Clingo::StatisticsType::Value);
    auto non_basic = simplex.add_subkey("Nonbasic", Clingo::StatisticsType::Value);
    auto bounds = simplex.add_subkey("Bounds", Clingo::StatisticsType::Value);
    auto threads = simplex.add_subkey("Threads", Clingo::StatisticsType::Array);
    auto const &master_stats = slvs_.front().second.statistics();

    // global values
    basic.set_value(master_stats.basic);
    non_basic.set_value(master_stats.non_basic);
    bounds.set_value(master_stats.bounds);
    tableaux.set_value(master_stats.tableau_initial);

    // per thread values
    size_t thread_id = 0;
    threads.ensure_size(slvs_.size(), Clingo::StatisticsType::Map);
    for (auto const &[offset, slv] : slvs_) {
        auto thread = threads[thread_id++];
        auto time = thread.add_subkey("Time", Clingo::StatisticsType::Map);
        auto total = time.add_subkey("Total", Clingo::StatisticsType::Value);
        auto propagate = time.add_subkey("Propagate", Clingo::StatisticsType::Value);
        auto avg = thread.add_subkey("Average Tableau Size", Clingo::StatisticsType::Value);
        auto pivots = thread.add_subkey("Pivots", Clingo::StatisticsType::Value);
        auto sat = thread.add_subkey("SAT", Clingo::StatisticsType::Value);
        auto unsat = thread.add_subkey("UNSAT", Clingo::StatisticsType::Value);

        auto const &stats = slv.statistics();
        pivots.set_value(pivots.value() + stats.pivots);
        total.set_value(total.value() + stats.total.total());
        propagate.set_value(propagate.value() + stats.propagate.total());
        sat.set_value(sat.value() + stats.sat);
        unsat.set_value(unsat.value() + stats.unsat);
        avg.set_value(stats.tableau_average);
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
