#include <gtest/gtest.h>

#include "autodj/core/Denormals.h"
#include "autodj/core/RtGuard.h"
#include "test_util.h"

using namespace autodj;

#if AUTODJ_RT_GUARD

TEST(RtGuard, AllocationAndFreeInsideAudioScopeAreReported) {
  testutil::ViolationCounter counter;
  {
    int* p = new int(1);  // outside the scope: fine
    delete p;
  }
  EXPECT_EQ(counter.count(), 0u);
  {
    rt::AudioThreadScope scope;
    int* volatile p = new int(2);
    EXPECT_EQ(counter.count(), 1u) << "operator new on the audio thread";
    delete p;
    EXPECT_EQ(counter.count(), 2u) << "operator delete on the audio thread";
  }
  {
    int* p = new int(3);  // scope closed again
    delete p;
  }
  EXPECT_EQ(counter.count(), 2u);
}

TEST(RtGuard, ContainerGrowthIsCaught) {
  testutil::ViolationCounter counter;
  rt::AudioThreadScope scope;
  std::vector<int> v;
  v.push_back(1);
  testutil::escape(v.data());  // clang may otherwise elide the allocation behind push_back
  EXPECT_GE(counter.count(), 1u);
}

TEST(RtGuard, ScopedAllowAllocLiftsTheGuard) {
  testutil::ViolationCounter counter;
  rt::AudioThreadScope scope;
  {
    rt::ScopedAllowAlloc allow;
    int* p = new int(1);
    delete p;
  }
  EXPECT_EQ(counter.count(), 0u);
}

TEST(RtGuard, ForbiddenHookReportsOnlyOnAudioThread) {
  testutil::ViolationCounter counter;
  AUTODJ_RT_FORBIDDEN("jni call");
  EXPECT_EQ(counter.count(), 0u);
  rt::AudioThreadScope scope;
  AUTODJ_RT_FORBIDDEN("jni call");
  EXPECT_EQ(counter.count(), 1u);
}

TEST(RtGuardDeathTest, DefaultHandlerAbortsImmediately) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      {
        rt::AudioThreadScope scope;
        int* p = new int(1);
        testutil::escape(p);  // an unobserved new may be elided by clang, then nothing aborts
      },
      "RT VIOLATION");
}

#endif  // AUTODJ_RT_GUARD

TEST(Denormals, ScopeFlushesDenormalsAndRestores) {
  const bool before = ScopedFlushDenormals::active();
  {
    ScopedFlushDenormals ftz;
    EXPECT_TRUE(ScopedFlushDenormals::active());
    volatile float tiny = 1e-40f;  // denormal
    volatile float r = tiny * 1.0f;
    EXPECT_EQ(r, 0.0f) << "denormal result/operand must be flushed to zero";
  }
  EXPECT_EQ(ScopedFlushDenormals::active(), before);
}
