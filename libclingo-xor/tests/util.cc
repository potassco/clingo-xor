#include <util.hh>

#include <catch.hpp>

TEST_CASE("util") {
    SECTION("tableau") {
        Tableau t;

        // check default value 0 at (0,0)
        REQUIRE(t.empty());
        REQUIRE(!t.contains(0, 0));

        // set 1 at (0,0)
        t.set(0, 0, true);
        REQUIRE(t.size() == 1);
        REQUIRE(t.contains(0, 0));

        // set 1 at (0,0)
        t.set(0, 0, true);
        REQUIRE(t.size() == 1);
        REQUIRE(t.contains(0, 0));

        // remove at (0,0)
        t.set(0, 0, false);
        REQUIRE(!t.contains(0, 0));
        REQUIRE(t.empty());

        // set 1 at (0,2)
        t.set(0, 2, true);
        REQUIRE(t.size() == 1);

        // traverse the first row
        t.update_row(0, [](index_t j) {
            REQUIRE(j == 2);
            return true;
        });
        REQUIRE(t.size() == 1);

        // traverse the third column
        t.update_col(2, [](index_t i) {
            REQUIRE(i == 0);
        });
        REQUIRE(t.size() == 1);

        // traverse the first column
        t.update_col(0, [](index_t j) { });
        REQUIRE(t.size() == 1);
    }
};
