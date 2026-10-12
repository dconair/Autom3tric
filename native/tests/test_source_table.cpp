#include <gtest/gtest.h>

#include "autodj/core/DemoSources.h"
#include "autodj/core/SourceTable.h"

using namespace autodj;

namespace {
std::vector<const void*> drainGarbage(GarbageQueue& q) {
  std::vector<const void*> out;
  GarbageMsg m;
  while (q.tryPop(m)) out.push_back(m.ptr);
  return out;
}
}  // namespace

TEST(SampleBuffer, GuardFramesAreZeroAndDataIsAligned) {
  const auto b = demo::sine(440.0, 0.1, 48000);
  ASSERT_GT(b->frames(), 4000);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(b->data() - 2 * SampleBuffer::kGuardFrames) % 64, 0u);
  for (int i = -SampleBuffer::kGuardFrames; i < 0; ++i) {
    EXPECT_EQ(b->data()[2 * i], 0.f);
    EXPECT_EQ(b->data()[2 * i + 1], 0.f);
  }
  for (int i = 0; i < SampleBuffer::kGuardFrames; ++i) {
    EXPECT_EQ(b->data()[2 * (b->frames() + i)], 0.f);
  }
}

TEST(SampleBuffer, MonoIsDuplicatedIntoBothChannels) {
  const float mono[4] = {0.1f, 0.2f, 0.3f, 0.4f};
  const auto b = SampleBuffer::fromPlanar(mono, nullptr, 4, 48000);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(b->data()[2 * i], mono[i]);
    EXPECT_EQ(b->data()[2 * i + 1], mono[i]);
  }
}

TEST(SourceTable, BufferIsReturnedOnlyAfterLastGrainReleasedIt) {
  GarbageQueue garbage;
  SourceTable t;
  const auto a = demo::sine(100, 0.05, 48000);
  const auto b = demo::sine(200, 0.05, 48000);

  ASSERT_TRUE(t.publish(0, a.get(), garbage));
  const int ea = t.entryOfSlot(0);
  ASSERT_GE(ea, 0);
  t.acquire(ea);
  t.acquire(ea);  // two grains read from A

  ASSERT_TRUE(t.publish(0, b.get(), garbage));  // slot switched live
  EXPECT_NE(t.entryOfSlot(0), ea);
  EXPECT_TRUE(drainGarbage(garbage).empty()) << "A is still used by playing grains";
  EXPECT_EQ(t.buffer(ea), a.get()) << "grains keep reading A";

  t.release(ea, garbage);
  EXPECT_TRUE(drainGarbage(garbage).empty());
  t.release(ea, garbage);
  const auto freed = drainGarbage(garbage);
  ASSERT_EQ(freed.size(), 1u);
  EXPECT_EQ(freed[0], a.get());
  EXPECT_EQ(t.liveEntries(), 1);
}

TEST(SourceTable, FullGarbageQueueIsRetriedNotLost) {
  GarbageQueue garbage;
  SourceTable t;
  // Fill the garbage queue completely.
  for (std::size_t i = 0; i < GarbageQueue::capacity(); ++i) ASSERT_TRUE(garbage.tryPush(GarbageMsg{}));
  const auto a = demo::sine(100, 0.05, 48000);
  t.publish(3, a.get(), garbage);
  t.publish(3, nullptr, garbage);  // unmap -> wants to retire, queue is full
  EXPECT_EQ(t.liveEntries(), 1) << "still held: could not be handed back yet";

  GarbageMsg m;
  while (garbage.tryPop(m)) {}  // control side drains
  t.flushRetries(garbage);
  const auto freed = drainGarbage(garbage);
  ASSERT_EQ(freed.size(), 1u);
  EXPECT_EQ(freed[0], a.get());
  EXPECT_EQ(t.liveEntries(), 0);
}

TEST(SourceTable, InvalidSlotAndFullTableReturnTheBuffer) {
  GarbageQueue garbage;
  SourceTable t;
  const auto a = demo::sine(100, 0.05, 48000);
  EXPECT_FALSE(t.publish(99, a.get(), garbage));
  EXPECT_EQ(drainGarbage(garbage).size(), 1u);

  std::vector<std::shared_ptr<const SampleBuffer>> keep;
  int ok = 0;
  for (int i = 0; i < SourceTable::kEntries + 3; ++i) {
    keep.push_back(demo::sine(100 + i, 0.01, 48000));
    const int slot = i % SourceTable::kSlots;
    // Pin every mapped buffer with a grain reference so the table really fills up.
    if (t.publish(slot, keep.back().get(), garbage)) {
      ++ok;
      t.acquire(t.entryOfSlot(slot));
    }
  }
  EXPECT_LT(ok, SourceTable::kEntries + 3);
  EXPECT_FALSE(drainGarbage(garbage).empty());
}
