#include "solving.hh"
#include "parsing.hh"

#include <unordered_set>

typename Solver::BoundRelation bound_rel(Relation rel) {
    switch (rel) {
        case Relation::Less:
        case Relation::LessEqual: {
            return Solver::BoundRelation::LessEqual;
        }
        case Relation::Greater:
        case Relation::GreaterEqual: {
            return Solver::BoundRelation::GreaterEqual;
        }
        case Relation::Equal: {
            break;
        }
    }
    return Solver::BoundRelation::Equal;
}

Number bound_val(Number x, Relation rel) {
    assert(rel != Relation::Less && rel != Relation::Greater);
    return x;
}

struct Solver::Prepare {
    Prepare(Solver &slv, SymbolMap const &map)
    : slv{slv}
    , map{map} {
        slv.variables_.resize(map.size());
        slv.n_non_basic_ = map.size();
        for (index_t i = 0; i != slv.n_non_basic_; ++i) {
            slv.variables_[i].index = i;
            slv.variables_[i].reserve_index = i;
        }
    }

    index_t get_non_basic(Clingo::Symbol var) {
        auto jt = map.find(var);
        assert(jt != map.end());
        return slv.variables_[jt->second].reserve_index;
    }

    index_t add_basic() {
        auto index = slv.variables_.size();
        slv.variables_.emplace_back();
        slv.variables_.back().index = index;
        slv.variables_.back().reserve_index = index;
        return slv.n_basic_++;
    }

    std::vector<std::pair<index_t, Number>> add_row(Inequality const &x) {
        std::vector<std::pair<index_t, Number>> row;

        // add non-basic variables
        for (auto const &term : x.lhs) {
            row.emplace_back(get_non_basic(term.var), term.co);
        }

        return row;
    }

    Solver &slv;
    SymbolMap const &map;
    std::vector<index_t> basic;
};

bool Solver::Variable::update_upper(Solver &s, Clingo::Assignment ass, Bound const &bound) {
    if (!has_upper() || bound.value < upper()) {
        if (!has_upper() || ass.level(upper_bound->lit) < ass.decision_level()) {
            s.bound_trail_.emplace_back(bound.variable, BoundRelation::LessEqual, upper_bound);
        }
        upper_bound = &bound;
    }
    return !has_lower() || lower() <= upper();
}

bool Solver::Variable::update_lower(Solver &s, Clingo::Assignment ass, Bound const &bound) {
    if (!has_lower() || bound.value > lower()) {
        if (!has_lower() || ass.level(lower_bound->lit) < ass.decision_level()) {
            if (upper_bound != &bound) {
                s.bound_trail_.emplace_back(bound.variable, BoundRelation::GreaterEqual, lower_bound);
            }
            else {
                // Note: this assumes that update_lower is called right after update_upper for the same bound
                std::get<1>(s.bound_trail_.back()) = BoundRelation::Equal;
            }
        }
        lower_bound = &bound;
    }
    return !has_upper() || lower() <= upper();
}

bool Solver::Variable::update(Solver &s, Clingo::Assignment ass, Bound const &bound) {
    switch (bound.rel) {
        case BoundRelation::LessEqual: {
            return update_upper(s, ass, bound);
        }
        case BoundRelation::GreaterEqual: {
            return update_lower(s, ass, bound);
        }
        case BoundRelation::Equal: {
            break;
        }
    }
    return update_upper(s, ass, bound) && update_lower(s, ass, bound);
}

void Solver::Variable::set_value(Solver &s, index_t lvl, Value const &val, bool add) {
    // We can always assume that the assignment on a previous level was satisfying.
    // Thus, we simply store the old values to be able to restore them when backtracking.
    if (lvl != level) {
        s.assignment_trail_.emplace_back(level, this - s.variables_.data(), value);
        level = lvl;
    }
    if (add) {
        value += val;
    }
    else {
        value = val;
    }
}

bool Solver::Variable::has_conflict() const {
    return (has_lower() && value < lower()) || (has_upper() && value > upper());
}

void Statistics::reset() {
    *this = {};
}

Solver::Solver(std::vector<Inequality> const &inequalities)
: inequalities_{inequalities}
{ }

Solver::Variable &Solver::basic_(index_t i) {
    assert(i < n_basic_);
    return variables_[variables_[i + n_non_basic_].index];
}

typename Solver::Variable &Solver::non_basic_(index_t j) {
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

bool Solver::prepare(Clingo::PropagateInit &init, SymbolMap const &symbols) {
    // TODO: Bounds associated with a variable form a propagation chain. We can
    // add binary clauses to propagate them. For example
    //
    //     `x >= u` implies not `x <= l` for all `l < u`.
    //
    // Care has to be taken because we cannot use
    //
    //     `x >= u` implies `x >= u'` for all u' >= u
    //
    // because I am going for a non-strict defined semantics.

    auto ass = init.assignment();

    Prepare prep{*this, symbols};
    for (auto const &x : inequalities_) {
        if (ass.is_false(x.lit)) {
            continue;
        }

        // transform inequality into row suitable for tableau
        auto row = prep.add_row(x);

        // check bound against 0
        if (row.empty()) {
            switch (x.rel) {
                case Relation::Less: {
                    if (x.rhs >= 0 && !init.add_clause({-x.lit})) {
                        return false;
                    }
                    break;
                }
                case Relation::LessEqual: {
                    if (x.rhs > 0 && !init.add_clause({-x.lit})) {
                        return false;
                    }
                    break;
                }
                case Relation::Greater: {
                    if (x.rhs <= 0 && !init.add_clause({-x.lit})) {
                        return false;
                    }
                    break;
                }
                case Relation::GreaterEqual: {
                    if (x.rhs < 0 && !init.add_clause({-x.lit})) {
                        return false;
                    }
                    break;
                }
                case Relation::Equal: {
                    if (x.rhs != 0 && !init.add_clause({-x.lit})) {
                        return false;
                    }
                    break;
                }
            }
        }
        // add a bound to a non-basic variable
        else if (row.size() == 1) {
            auto const &[j, v] = row.front();
            auto &xj = non_basic_(j);
            auto rel = v < 0 ? invert(x.rel) : x.rel;
            bounds_.emplace(x.lit, Bound{
                bound_val(Factor{x.rhs / v}, rel),
                variables_[j].index,
                x.lit,
                bound_rel(rel)});
        }
        // add an inequality
        else {
            auto i = prep.add_basic();
            bounds_.emplace(x.lit, Bound{
                bound_val(Factor{x.rhs}, x.rel),
                static_cast<index_t>(variables_.size() - 1),
                x.lit,
                bound_rel(x.rel)});
            for (auto const &[j, v] : row) {
                tableau_.set(i, j, v);
            }
        }
    }

    for (size_t i = 0; i < n_basic_; ++i) {
        enqueue_(i);
    }

    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());

    return true;
}

bool Solver::solve(Clingo::PropagateControl &ctl, Clingo::LiteralSpan lits) {
    index_t i{0};
    index_t j{0};
    Value const *v{nullptr};

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
            if (!x.update(*this, ctl.assignment(), bound)) {
                conflict_clause_.clear();
                conflict_clause_.emplace_back(-x.upper_bound->lit);
                conflict_clause_.emplace_back(-x.lower_bound->lit);
                return false;
            }
            if (x.reserve_index < n_non_basic_) {
                if (x.has_lower() && x.value < x.lower()) {
                    update_(level, x.reserve_index, x.lower());
                }
                else if (x.has_upper() && x.value > x.upper()) {
                    update_(level, x.reserve_index, x.upper());
                }
            }
            else {
                enqueue_(x.reserve_index - n_non_basic_);
            }
        }
    }

    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());

    while (true) {
        switch (select_(i, j, v)) {
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
                return true;
            }
            case State::Unsatisfiable: {
                return false;
            }
            case State::Unknown: {
                assert(v != nullptr);
                pivot_(level, i, j, *v); // NOLINT
            }
        }
    }
}

void Solver::undo() {
    // this function restores the last satisfying assignment
    auto &offset = trail_offset_.back();

    // undo bound updates
    for (auto it = bound_trail_.begin() + offset.bound, ie = bound_trail_.end(); it != ie; ++it) {
        auto [var, rel, bound] = *it;
        switch (rel) {
            case BoundRelation::LessEqual: {
                variables_[var].upper_bound = bound;
                break;
            }
            case BoundRelation::GreaterEqual: {
                variables_[var].lower_bound = bound;
                break;
            }
            case BoundRelation::Equal: {
                variables_[var].upper_bound = bound;
                variables_[var].lower_bound = bound;
                break;
            }
        }
    }
    bound_trail_.resize(offset.bound);

    // undo assignments
    for (auto it = assignment_trail_.begin() + offset.assignment, ie = assignment_trail_.end(); it != ie; ++it) {
        auto &[level, index, number] = *it;
        variables_[index].level = level;
        variables_[index].value.swap(number);
    }
    assignment_trail_.resize(offset.assignment);

    // empty queue
    for (; !conflicts_.empty(); conflicts_.pop()) {
        variables_[conflicts_.top()].queued = false;
    }

    trail_offset_.pop_back();

    assert_extra(check_solution());
}

Statistics const &Solver::statistics() const {
    return statistics_;
}

bool Solver::check_tableau_() {
    for (index_t i{0}; i < n_basic_; ++i) {
        Value v_i;
        tableau_.update_row(i, [&](index_t j, Number const &a_ij){
            v_i += non_basic_(j).value * a_ij;
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
        if (xi.has_lower() && xi.value < xi.lower() && !xi.queued) {
            return false;
        }
        if (xi.has_upper() && xi.value > xi.upper() && !xi.queued) {
            return false;
        }
    }
    return true;
}

bool Solver::check_non_basic_() {
    for (index_t j = 0; j < n_non_basic_; ++j) {
        auto &xj = non_basic_(j);
        if (xj.has_lower() && xj.value < xj.lower()) {
            return false;
        }
        if (xj.has_upper() && xj.value > xj.upper()) {
            return false;
        }
    }
    return true;
}

bool Solver::check_solution(bool trace) {
    for (auto &x : variables_) {
        if (x.has_lower() && x.lower() > x.value) {
            return false;
        }
        if (x.has_upper() && x.value > x.upper()) {
            return false;
        }
    }
    return check_tableau_() && check_basic_();
}

void Solver::update_(index_t level, index_t j, Value v) {
    auto &xj = non_basic_(j);
    tableau_.update_col(j, [&](index_t i, Number const &a_ij) {
        basic_(i).set_value(*this, level, a_ij * (v - xj.value), true);
        enqueue_(i);
    });
    xj.set_value(*this, level, v, false);
}

void Solver::pivot_(index_t level, index_t i, index_t j, Value const &v) {
    auto &a_ij = tableau_.unsafe_get(i, j);
    assert(a_ij != 0);

    auto &xi = basic_(i);
    auto &xj = non_basic_(j);

    // adjust assignment
    Value dj = (v - xi.value) / a_ij;
    assert(dj != 0);
    xi.set_value(*this, level, v, false);
    xj.set_value(*this, level, dj, true);
    // TODO: can this be merged into the loop below?:
    tableau_.update_col(j, [&](index_t k, Number const &a_kj) {
        if (k != i) {
            basic_(k).set_value(*this, level, a_kj * dj, true);
            enqueue_(k);
        }
    });
    assert_extra(check_tableau_());

    // swap variables x_i and x_j
    std::swap(xi.reserve_index, xj.reserve_index);
    std::swap(variables_[i + n_non_basic_].index, variables_[j].index);
    enqueue_(i);

    // invert row i
    tableau_.update_row(i, [&](index_t k, Number &a_ik) {
        if (k != j) {
            a_ik /= -a_ij;
        }
    });
    a_ij = 1 / a_ij;

    // eliminate x_j from rows k != i
    tableau_.eliminate(i, j);

    ++statistics_.pivots_;
    assert_extra(check_tableau_());
    assert_extra(check_basic_());
    assert_extra(check_non_basic_());
}

bool Solver::select_(Variable &x) {
    if ((!x.has_upper() || x.value < x.upper()) && (!x.has_lower() || x.value > x.lower())) {
        return true;
    }
    if (x.has_upper()) {
        conflict_clause_.emplace_back(-x.upper_bound->lit);
    }
    if (x.has_lower()) {
        conflict_clause_.emplace_back(-x.lower_bound->lit);
    }
    return false;
}

typename Solver::State Solver::select_(index_t &ret_i, index_t &ret_j, Value const *&ret_v) {
    // This implements Bland's rule selecting the variables with the smallest
    // indices for pivoting.

    for (; !conflicts_.empty(); conflicts_.pop()) {
        auto ii = conflicts_.top();
        auto &xi = variables_[ii];
        auto i = xi.reserve_index;
        assert(ii == variables_[i].index);
        xi.queued = false;
        // the queue might contain variables that meanwhile became basic
        if (i < n_non_basic_) {
            continue;
        }
        i -= n_non_basic_;

        if (xi.has_lower() && xi.value < xi.lower()) {
            conflict_clause_.clear();
            conflict_clause_.emplace_back(-xi.lower_bound->lit);
            index_t kk = variables_.size();
            tableau_.update_row(i, [&](index_t j, Number const &a_ij) {
                auto jj = variables_[j].index;
                if (jj < kk && select_(variables_[jj])) {
                    kk = jj;
                    ret_i = i;
                    ret_j = j;
                    ret_v = &xi.lower();
                }
            });
            if (kk == variables_.size()) {
                return State::Unsatisfiable;
            }
            return State::Unknown;
        }

        if (xi.has_upper() && xi.value > xi.upper()) {
            conflict_clause_.clear();
            conflict_clause_.emplace_back(-xi.upper_bound->lit);
            index_t kk = variables_.size();
            tableau_.update_row(i, [&](index_t j, Number const &a_ij) {
                auto jj = variables_[j].index;
                if (jj < kk && select_(variables_[jj])) {
                    kk = jj;
                    ret_i = i;
                    ret_j = j;
                    ret_v = &xi.upper();
                }
            });
            if (kk == variables_.size()) {
                return State::Unsatisfiable;
            }
            return State::Unknown;
        }
    }

    assert(check_solution());

    return State::Satisfiable;
}

void Propagator::init(Clingo::PropagateInit &init) {
    facts_offset_ = facts_.size();
    if (facts_offset_ > 0) {
        init.set_check_mode(Clingo::PropagatorCheckMode::Partial);
    }

    std::unordered_map<Clingo::Symbol, Term&> cos;
    evaluate_theory(init.theory_atoms(), [&](Clingo::literal_t lit) { return init.solver_literal(lit); }, aux_map_, iqs_);
    for (auto &x : iqs_) {
        auto ib = x.lhs.begin();
        auto ie = x.lhs.end();

        // combine cofficients
        std::for_each(ib, ie, [&cos, this](Term &term) {
            if (var_map_.emplace(term.var, var_map_.size()).second) {
                var_vec_.emplace_back(term.var);
            }
            if (auto [jt, res] = cos.emplace(term.var, term); !res) {
                jt->second.co += term.co;
                term.co = 0;
            }
        });
        cos.clear();

        // remove terms with zero coeffcients
        x.lhs.erase(std::remove_if(ib, ie, [](Term const &term) {
            return term.co == 0;
        }), ie);

        // add watch
        init.add_watch(x.lit);
    }

    slvs_.clear();
    slvs_.reserve(init.number_of_threads());
    for (size_t i = 0, e = init.number_of_threads(); i != e; ++i) {
        slvs_.emplace_back(0, iqs_);
        if (!slvs_.back().second.prepare(init, var_map_)) {
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
            if (!ctl.add_clause(slv.reason())) {
                return;
            }
        }
        offset = facts_offset_;
    }
    if (ass.is_total()) {
        if (!slv.check_solution(true)) {
            throw std::runtime_error("invalid solution");
        }
    }
}

void Propagator::propagate(Clingo::PropagateControl &ctl, Clingo::LiteralSpan changes) {
    auto ass = ctl.assignment();
    if (ass.decision_level() == 0 && ctl.thread_id() == 0) {
        facts_.insert(facts_.end(), changes.begin(), changes.end());
    }
    auto &[offset, slv] = slvs_[ctl.thread_id()];
    if (!slv.solve(ctl, changes)) {
        ctl.add_clause(slv.reason());
        return;
    }
}

void Propagator::undo(Clingo::PropagateControl const &ctl, Clingo::LiteralSpan changes) noexcept {
    slvs_[ctl.thread_id()].second.undo();
}

std::optional<index_t> Propagator::lookup_symbol(Clingo::Symbol symbol) const {
    if (auto it = var_map_.find(symbol); it != var_map_.end()) {
        return it->second;
    }
    return {};
}

Clingo::Symbol Propagator::get_symbol(index_t i) const {
    return var_vec_[i];
}

bool Propagator::has_value(index_t thread_id, index_t i) const {
    static_cast<void>(thread_id);
    return i < var_vec_.size();
}

Value Propagator::get_value(index_t thread_id, index_t i) const {
    return slvs_[thread_id].second.get_value(i);
}

index_t Propagator::n_values(index_t thread_id) const {
    return var_vec_.size();
}
