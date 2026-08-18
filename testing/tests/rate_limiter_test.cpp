#include <gtest/gtest.h>

#include "network/security/rate_limiter.hpp"

#include <chrono>
#include <thread>

TEST(RateLimiter, RejectsInvalidConfiguration) {
    EXPECT_THROW(rate_limiter(0.0, 1.0), std::invalid_argument);
    EXPECT_THROW(rate_limiter(1.0, 0.0), std::invalid_argument);
}

TEST(RateLimiter, AllowsInitialBurst) {
    rate_limiter limiter(3.0, 1.0);

    EXPECT_TRUE(limiter.consume("client"));
    EXPECT_TRUE(limiter.consume("client"));
    EXPECT_TRUE(limiter.consume("client"));

    EXPECT_FALSE(limiter.consume("client"));
}

TEST(RateLimiter, DifferentIdentifiersHaveIndependentBuckets) {
    rate_limiter limiter(1.0, 1.0);

    EXPECT_TRUE(limiter.consume("alice"));
    EXPECT_TRUE(limiter.consume("bob"));

    EXPECT_FALSE(limiter.consume("alice"));
    EXPECT_FALSE(limiter.consume("bob"));
}

TEST(RateLimiter, RejectsInvalidCost) {
    rate_limiter limiter(3.0, 1.0);

    EXPECT_FALSE(limiter.consume("client", 0.0));
    EXPECT_FALSE(limiter.consume("client", -1.0));
    EXPECT_FALSE(limiter.consume("client", 4.0));
}

TEST(RateLimiter, TokensRefillOverTime) {
    rate_limiter limiter(1.0, 20.0);

    ASSERT_TRUE(limiter.consume("client"));
    ASSERT_FALSE(limiter.consume("client"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(limiter.consume("client"));
}