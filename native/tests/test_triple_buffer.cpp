#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "autodj/core/TripleBuffer.h"

using autodj::TripleBuffer;

TEST(TripleBuffer, ReaderSeesNothingBeforeFirstPublish) {
  TripleBuffer<int> tb;
  int v = 0;
  EXPECT_FALSE(tb.read(v));
}

TEST(TripleBuffer, ReaderGetsLatestAndSkipsStale) {
  TripleBuffer<int> tb;
  tb.writeSlot() = 1;
  tb.publish();
  tb.writeSlot() = 2;
  tb.publish();
  tb.writeSlot() = 3;
  tb.publish();
  int v = 0;
  ASSERT_TRUE(tb.read(v));
  EXPECT_EQ(v, 3);
  EXPECT_FALSE(tb.read(v)) << "no new data since the last read";
  tb.writeSlot() = 4;
  tb.publish();
  ASSERT_TRUE(tb.read(v));
  EXPECT_EQ(v, 4);
}

namespace {
struct Wide {
  std::uint64_t a[16];
};
}  // namespace

TEST(TripleBuffer, NeverReturnsTornSnapshotsAcrossThreads) {
  TripleBuffer<Wide> tb;
  std::atomic<bool> stop{false};
  std::thread writer([&] {
    std::uint64_t n = 0;
    while (!stop.load(std::memory_order_relaxed)) {
      Wide& w = tb.writeSlot();
      ++n;
      for (auto& x : w.a) x = n;
      tb.publish();
    }
  });
  std::uint64_t last = 0;
  int reads = 0;
  bool torn = false, backwards = false;
  // Run until enough snapshots were observed (or a generous deadline: sanitizer builds are slow and
  // the writer may be descheduled for a while on a loaded CI machine).
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  for (std::uint64_t i = 0; !torn && reads < 200000; ++i) {
    if ((i & 0x3FF) == 0 && std::chrono::steady_clock::now() > deadline) break;
    Wide w;
    if (!tb.read(w)) continue;
    ++reads;
    for (auto x : w.a) torn |= (x != w.a[0]);
    backwards |= (w.a[0] < last);
    last = w.a[0];
  }
  stop = true;
  writer.join();
  EXPECT_FALSE(torn);
  EXPECT_FALSE(backwards);
  EXPECT_GT(reads, 100) << "reader starved - machine overloaded?";
}
