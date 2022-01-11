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


inline unsigned uabs(int a) {
    if (a >= 0) {
        return static_cast<unsigned>(a);
    }
    return -static_cast<unsigned>(a);
}

// Number class implementing arithmetic operations modulo 2
class Number {
public:
    Number() = default;

    Number(int num)
    : num_{static_cast<uint8_t>(uabs(num) % 2U)} {
    }

    void swap(Number &x) {
        std::swap(num_, x.num_);
    }

    void canonicalize() {
    }

    friend Number operator*(Number const &a, Number const &b) {
        return Number(a.num_ * b.num_);
    }

    friend Number &operator*=(Number &a, Number const &b) {
        a.num_ *= b.num_;
        return a;
    }

    friend Number operator/(Number const &a, Number const &b) {
        assert(b.num_ != 0);
        return Number(a.num_ / b.num_);
    }

    friend Number &operator/=(Number &a, Number const &b) {
        assert(b.num_ != 0);
        a.num_ /= b.num_;
        return a;
    }

    friend Number operator+(Number const &a, Number const &b) {
        return Number(a.num_ ^ b.num_);
    }
    friend Number &operator+=(Number &a, Number const &b) {
        a.num_ ^= b.num_;
        return a;
    }

    friend Number operator-(Number const &a) {
        return a;
    }
    friend Number operator-(Number const &a, Number const &b) {
        return a + b;
    }
    friend Number &operator-=(Number &a, Number const &b) {
        return a += b;
    }

    friend bool operator<(Number const &a, Number const &b) {
        return a.num_ < b.num_;
    }
    friend bool operator<=(Number const &a, Number const &b) {
        return a.num_ <= b.num_;
    }
    friend bool operator>(Number const &a, Number const &b) {
        return a.num_ > b.num_;
    }
    friend bool operator>=(Number const &a, Number const &b) {
        return a.num_ >= b.num_;
    }
    friend bool operator==(Number const &a, Number const &b) {
        return a.num_ == b.num_;
    }
    friend bool operator!=(Number const &a, Number const &b) {
        return a.num_ != b.num_;
    }

    friend bool operator<(Number const &a, int b) {
        return a < Number{b};
    }
    friend bool operator<=(Number const &a, int b) {
        return a <= Number{b};
    }
    friend bool operator>(Number const &a, int b) {
        return a > Number{b};
    }
    friend bool operator>=(Number const &a, int b) {
        return a >= Number{b};
    }
    friend bool operator==(Number const &a, int b) {
        return a == Number{b};
    }
    friend bool operator!=(Number const &a, int b) {
        return a != Number{b};
    }

    friend int compare(Number const &a, Number const &b) {
        return (int)a.num_ - (int)b.num_;
    }

    friend std::ostream &operator<<(std::ostream &out, Number const &a) {
        out << static_cast<int>(a.num_);
        return out;
    }

private:
    explicit Number(uint8_t num)
    : num_{num} {
    }

    uint8_t num_{0};
};

//! A sparse matrix with efficient access to both rows and columns.
//!
//! Insertion into the matrix is linear in the number of rows/columns and
//! should be avoided.
class Tableau {
private:
    struct Cell {
        Cell(index_t col, Number val)
        : col{col}
        , val{val} { }
        Cell(Cell const &) = default;
        Cell(Cell &&) = default;
        Cell &operator=(Cell const &) = default;
        Cell &operator=(Cell &&) = default;
        ~Cell() = default;

        friend bool operator==(Cell const &x, Cell const &y) {
            return x.col == y.col && x.val == y.val;
        }
        friend bool operator<(Cell const &x, Cell const &y) {
            return x.col < y.col;
        }
        friend bool operator<(Cell const &x, index_t col) {
            return x.col < col;
        }
        friend bool operator<(index_t col, Cell const &x) {
            return col < x.col;
        }

        index_t col;
        Number val;
    };
    std::vector<Cell> &reserve_row_(index_t i) {
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
    static Number const &zero_() {
        static Number zero{0};
        return zero;
    }

public:
    //! Get value at row `i` and column `j`.
    [[nodiscard]] Number const &get(index_t i, index_t j) const {
        if (i < rows_.size()) {
            auto const &row = rows_[i];
            auto it = std::lower_bound(row.begin(), row.end(), j);
            if (it != row.end() && it->col == j) {
                return it->val;
            }
        }
        return zero_();
    }

    //! Get value at modifiable reference to row `i` and column `j` assuming
    //! that the value exists.
    //!
    //! Only non-zero values should be accessed and they should not be set to
    //! zero.
    [[nodiscard]] Number &unsafe_get(index_t i, index_t j) {
        return std::lower_bound(rows_[i].begin(), rows_[i].end(), j)->val;
    }

    //! Set value `a` at row `i` and column `j`.
    void set(index_t i, index_t j, Number const &a) {
        if (a == 0) {
            if (i < rows_.size()) {
                auto &row = rows_[i];
                auto it = std::lower_bound(row.begin(), row.end(), j);
                if (it != row.end() && it->col == j) {
                    row.erase(it);
                }
            }
        }
        else {
            auto &row = reserve_row_(i);
            auto it = std::lower_bound(row.begin(), row.end(), j);
            if (it == row.end() || it->col != j) {
                row.emplace(it, j, a);
            }
            else {
                it->val = a;
            }
            auto &col = reserve_col_(j);
            auto jt = std::lower_bound(col.begin(), col.end(), i);
            if (jt == col.end() || *jt != i) {
                col.emplace(jt, i);
            }
        }
    }

    //! Traverse non-zero elements in a row.
    //!
    //! The given function must not set the value to zero.
    template <typename F>
    void update_row(index_t i, F &&f) {
        if (i < rows_.size()) {
            for (auto &[col, val] : rows_[i]) {
                f(col, val);
            }
        }
    }

    //! Traverse non-zero elements in a column.
    //!
    //! The given function must not set the value to zero.
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
                if (kt != row.end() && kt->col == j) {
                    f(i, kt->val);
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
        std::vector<Cell> row;
        update_col(j, [&](index_t k, Number const &a_kj) {
            if (k != i) {
                // Note that this call does not invalidate active iterators:
                // - row i is unaffected because k != i
                // - there are no insertions in column j because each a_kj != 0
                for (auto it = ib, jt = rows_[k].begin(), je = rows_[k].end(); it != ie || jt != je; ) {
                    if (jt == je || (it != ie && it->col < jt->col)) {
                        row.emplace_back(it->col, it->val * a_kj);
                        auto &col = cols_[it->col];
                        auto kt = std::lower_bound(col.begin(), col.end(), k);
                        if (kt == col.end() || *kt != k) {
                            col.emplace(kt, k);
                        }
                        ++it;
                    }
                    else if (it == ie || jt->col < it->col) {
                        row.emplace_back(std::move(*jt));
                        ++jt;
                    }
                    else {
                        if (jt->col != j) {
                            row.emplace_back(jt->col, std::move(jt->val));
                            row.back().val += it->val * a_kj;
                            if (row.back().val == 0) {
                                row.pop_back();
                            }
                        }
                        else {
                            row.emplace_back(jt->col, jt->val * it->val);
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
    std::vector<std::vector<Cell>> rows_;
    std::vector<std::vector<index_t>> cols_;
};
