#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "autodj/core/SpscQueue.h"

using autodj::SpscQueue;

TEST(SpscQueue, FifoOrderFullAndEmpty) {
  SpscQueue<int, 8> q;
  int v = -1;
  EXPECT_FALSE(q.tryPop(v));
  for (int i = 0; i < 8; ++i) EXPECT_TRUE(q.tryPush(i));
  EXPECT_FALSE(q.tryPush(99)) << "9th push must fail on a capacity-8 queue";
  for (int i = 0; i < 8; ++i) {
    ASSERT_TRUE(q.tryPop(v));
    EXPECT_EQ(v, i);
  }
  EXPECT_FALSE(q.tryPop(v));
}

TEST(SpscQueue, WrapsAroundManyTimes) {
  SpscQueue<std::uint32_t, 4> q;
  std::uint32_t next = 0, expect = 0;
  for (int round = 0; round < 10000; ++round) {
    const int burst = 1 + round % 4;
    for (int i = 0; i < burst; ++i) ASSERT_TRUE(q.tryPush(next++));
    for (int i = 0; i < burst; ++i) {
      std::uint32_t v;
      ASSERT_TRUE(q.tryPop(v));
      ASSERT_EQ(v, expect++);
    }
  }
}

TEST(SpscQueue, DrainRespectsLimit) {
  SpscQueue<int, 16> q;
  for (int i = 0; i < 10; ++i) q.tryPush(i);
  int sum = 0;
  EXPECT_EQ(q.drain([&](int v) { sum += v; }, 4), 4u);
  EXPECT_EQ(sum, 0 + 1 + 2 + 3);
  EXPECT_EQ(q.drain([&](int v) { sum += v; }), 6u);
  EXPECT_EQ(q.sizeApprox(), 0u);
}

TEST(SpscQueue, TwoThreadsKeepOrderAndLoseNothing) {
  constexpr std::uint64_t kCount = 2'000'000;
  SpscQueue<std::uint64_t, 1024> q;
  std::thread producer([&] {
    for (std::uint64_t i = 0; i < kCount;) {
      if (q.tryPush(i)) ++i;
    }
  });
  std::uint64_t expect = 0;
  bool ordered = true;
  while (expect < kCount) {
    std::uint64_t v;
    if (q.tryPop(v)) {
      if (v != expect) {
        ordered = false;
        break;
      }
      ++expect;
    }
  }
  if (!ordered) {
    // unblock the producer so join() cannot hang
    std::uint64_t v;
    while (q.tryPop(v)) {}
  }
  producer.join();
  EXPECT_TRUE(ordered);
  EXPECT_EQ(expect, kCount);
}
