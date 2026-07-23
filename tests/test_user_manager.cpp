#include <gtest/gtest.h>
#include "UserManager.h"

TEST(UserManager, AddAndGet) {
    UserManager um;
    User* u = um.add(1);
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(u->id, 1u);
    EXPECT_EQ(u->room_id, 0);

    User* fetched = um.get(1);
    EXPECT_EQ(fetched, u);
}

TEST(UserManager, Remove) {
    UserManager um;
    um.add(1);
    um.remove(1);
    EXPECT_EQ(um.get(1), nullptr);
}

TEST(UserManager, GetNonExistent) {
    UserManager um;
    EXPECT_EQ(um.get(999), nullptr);
}

TEST(UserManager, MultipleUsers) {
    UserManager um;
    User* u1 = um.add(1);
    User* u2 = um.add(2);
    User* u3 = um.add(3);

    EXPECT_NE(u1, nullptr);
    EXPECT_NE(u2, nullptr);
    EXPECT_NE(u3, nullptr);
    EXPECT_NE(u1, u2);
    EXPECT_NE(u2, u3);

    EXPECT_EQ(um.get(1)->id, 1u);
    EXPECT_EQ(um.get(2)->id, 2u);
    EXPECT_EQ(um.get(3)->id, 3u);
}

TEST(UserManager, RemoveOne_OthersRemain) {
    UserManager um;
    um.add(1);
    um.add(2);
    um.remove(1);

    EXPECT_EQ(um.get(1), nullptr);
    EXPECT_NE(um.get(2), nullptr);
}
