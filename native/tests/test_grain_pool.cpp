#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <vector>

#include "autodj/core/Dsp.h"
#include "autodj/granular/GrainPool.h"

using autodj::XorShift32;
using autodj::granular::GrainPool;

TEST(GrainPool, AllocatesExactlyCapacityThenFails) {
  auto pool = std::make_unique<GrainPool>();
  std::set<int> slots;
  for (int i = 0; i < GrainPool::kCapacity; ++i) {
    const int s = pool->allocate();
    ASSERT_GE(s, 0);
    EXPECT_TRUE(slots.insert(s).second) << "slot handed out twice";
  }
  EXPECT_EQ(pool->allocate(), -1);
  EXPECT_EQ(pool->activeCount(), GrainPool::kCapacity);
  EXPECT_EQ(pool->freeCount(), 0);
  EXPECT_TRUE(pool->checkInvariants());
}

TEST(GrainPool, FreedSlotsAreReusedWithoutSearch) {
  auto pool = std::make_unique<GrainPool>();
  const int a = pool->allocate();
  const int b = pool->allocate();
  pool->recycle(a);
  EXPECT_EQ(pool->allocate(), a) << "freelist is LIFO: the most recently freed slot comes back first";
  EXPECT_NE(a, b);
  EXPECT_TRUE(pool->checkInvariants());
}

TEST(GrainPool, RandomAllocFreeSequenceKeepsInvariants) {
  auto pool = std::make_unique<GrainPool>();
  XorShift32 rng(5);
  std::vector<int> live;
  for (int op = 0; op < 200000; ++op) {
    const bool alloc = live.empty() || (rng.uniform() < 0.5f && static_cast<int>(live.size()) < GrainPool::kCapacity);
    if (alloc) {
      const int s = pool->allocate();
      ASSERT_GE(s, 0);
      live.push_back(s);
    } else {
      const std::size_t i = rng.nextU32() % live.size();
      pool->recycle(live[i]);
      live[i] = live.back();
      live.pop_back();
    }
    ASSERT_EQ(pool->activeCount(), static_cast<int>(live.size()));
    if (op % 997 == 0) {
      ASSERT_TRUE(pool->checkInvariants());
    }
  }
  EXPECT_TRUE(pool->checkInvariants());
  std::set<int> active;
  for (int i = 0; i < pool->activeCount(); ++i) active.insert(pool->activeSlot(i));
  EXPECT_EQ(active, std::set<int>(live.begin(), live.end()));
}

TEST(GrainPool, DescendingIterationWithRemovalVisitsEveryGrainOnce) {
  auto pool = std::make_unique<GrainPool>();
  for (int i = 0; i < 100; ++i) {
    const int s = pool->allocate();
    ASSERT_GE(s, 0);
    pool->voice[s] = 0;
  }
  std::set<int> visited;
  for (int ai = pool->activeCount() - 1; ai >= 0; --ai) {
    const int s = pool->activeSlot(ai);
    EXPECT_TRUE(visited.insert(s).second);
    if (s % 3 == 0) pool->recycle(s);
  }
  EXPECT_EQ(visited.size(), 100u);
  EXPECT_TRUE(pool->checkInvariants());
}

TEST(GrainPool, ClearResetsEverything) {
  auto pool = std::make_unique<GrainPool>();
  for (int i = 0; i < 50; ++i) pool->allocate();
  pool->clear();
  EXPECT_EQ(pool->activeCount(), 0);
  EXPECT_EQ(pool->freeCount(), GrainPool::kCapacity);
  EXPECT_TRUE(pool->checkInvariants());
}
