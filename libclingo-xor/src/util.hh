#pragma once

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <memory>
#include <algorithm>

#ifdef CLINGOXOR_CROSSCHECK
#   define assert_extra(X) assert(X) // NOLINT
#else
#   define assert_extra(X) // NOLINT
#endif

using index_t = uint32_t;

//! A Boolean value to be used with XOR operations and equality comparisons.
class Value {
public:
    constexpr Value() = default;

    explicit constexpr Value(bool value)
    : value_{value} {
    }

    //! Flip the Boolean value.
    void constexpr flip() {
        value_ = !value_;
    }

    //! Test if the Boolean value is true.
    explicit constexpr operator bool() const {
        return value_;
    }

    //! Infix XOR operation.
    friend constexpr Value operator^(Value const &a, Value const &b) {
        return Value{a.value_ != b.value_};
    }
    //! Inplace XOR assignment.
    friend constexpr Value &operator^=(Value &a, Value const &b) {
        a.value_ = a.value_ != b.value_;
        return a;
    }

    //! Check if two Boolean values are equal.
    friend constexpr bool operator==(Value const &a, Value const &b) {
        return a.value_ == b.value_;
    }
    //! Check if two Boolean values are inequal.
    friend constexpr bool operator!=(Value const &a, Value const &b) {
        return a.value_ != b.value_;
    }

    //! Print the Boolean value as an integer (0 or 1).
    friend std::ostream &operator<<(std::ostream &out, Value const &a) {
        out << static_cast<int>(a.value_);
        return out;
    }

private:
    bool value_{false};
};

//! A sparse matrix with efficient access to both rows and columns.
//!
//! Insertion into the matrix is linear in the number of rows/columns and
//! should be avoided.
class Tableau {
private:
    std::vector<index_t> &reserve_row_(index_t i) {
        if (rows_.size() <= i) {
            rows_.resize(i + 1);
        }
        return rows_[i];
    }
    std::vector<index_t> &reserve_col_(index_t j) {
        if (cols_.size() <= j) {
            cols_.resize(j + 1);
        }
        return cols_[j];
    }

public:
    //! Check if the tableau contains row `i` and column `j`.
    [[nodiscard]] bool contains(index_t i, index_t j) const {
        if (i < rows_.size()) {
            auto const &row = rows_[i];
            auto it = std::lower_bound(row.begin(), row.end(), j);
            if (it != row.end() && *it == j) {
                return true;
            }
        }
        return false;
    }

    //! Set value `a` at row `i` and column `j`.
    void set(index_t i, index_t j, bool a) {
        if (a) {
            auto &row = reserve_row_(i);
            auto it = std::lower_bound(row.begin(), row.end(), j);
            if (it == row.end() || *it != j) {
                row.emplace(it, j);
            }
            auto &col = reserve_col_(j);
            auto jt = std::lower_bound(col.begin(), col.end(), i);
            if (jt == col.end() || *jt != i) {
                col.emplace(jt, i);
            }
        }
        else {
            if (i < rows_.size()) {
                auto &row = rows_[i];
                auto it = std::lower_bound(row.begin(), row.end(), j);
                if (it != row.end() && *it == j) {
                    row.erase(it);
                }
            }
        }
    }

    //! Traverse non-zero elements in a row.
    template <typename F>
    void update_row(index_t i, F &&f) {
        if (i < rows_.size()) {
            for (auto &col : rows_[i]) {
                if (!f(col)) {
                    break;
                }
            }
        }
    }

    //! Traverse non-zero elements in a column.
    template <typename F>
    void update_col(index_t j, F &&f) {
        if (j < cols_.size()) {
            auto &col = cols_[j];
            auto it = col.begin();
            auto ie = col.end();
            for (auto jt = it; jt != ie; ++jt) {
                auto i = *jt;
                auto &row = rows_[i];
                auto kt = std::lower_bound(row.begin(), row.end(), j);
                if (kt != row.end() && *kt == j) {
                    f(i);
                    if (it != jt) {
                        std::iter_swap(it, jt);
                    }
                    ++it;
                }
            }
            col.erase(it, ie);
        }
    }

    //! Eliminate x_j from rows k != i.
    //!
    //! This is the only function specific to the simplex algorithm. It is
    //! implemented like this to offer better performance and makes a lot of
    //! assumptions.
    void eliminate(index_t i, index_t j) {
        auto ib = rows_[i].begin();
        auto ie = rows_[i].end();
        std::vector<index_t> row;
        update_col(j, [&](index_t k) {
            if (k != i) {
                // Note that this call does not invalidate active iterators:
                // - row i is unaffected because k != i
                // - there are no insertions in column j because each a_kj != 0
                for (auto it = ib, jt = rows_[k].begin(), je = rows_[k].end(); it != ie || jt != je; ) {
                    if (jt == je || (it != ie && *it < *jt)) {
                        row.emplace_back(*it);
                        auto &col = cols_[*it];
                        auto kt = std::lower_bound(col.begin(), col.end(), k);
                        if (kt == col.end() || *kt != k) {
                            col.emplace(kt, k);
                        }
                        ++it;
                    }
                    else if (it == ie || *jt < *it) {
                        row.emplace_back(*jt);
                        ++jt;
                    }
                    else {
                        if (*jt == j) {
                            row.emplace_back(*jt);
                        }
                        ++it;
                        ++jt;
                    }
                }
                std::swap(rows_[k], row);
                row.clear();
            }
        });
    }

    //! Get the number of values in the matrix.
    //!
    //! The runtime of this function is linear in the size of the matrix.
    [[nodiscard]] size_t size() const {
        size_t ret{0};
        for (auto const &row : rows_) {
            ret += row.size();
        }
        return ret;
    }

    //! Equivalent to `size() == 0`.
    [[nodiscard]] bool empty() const {
        for (auto const &row : rows_) {
            if (!row.empty()) {
                return false;
            }
        }
        return true;
    }

    //! Clear the tableau.
    void clear() {
        rows_.clear();
        cols_.clear();
    }

private:
    std::vector<std::vector<index_t>> rows_;
    std::vector<std::vector<index_t>> cols_;
};
