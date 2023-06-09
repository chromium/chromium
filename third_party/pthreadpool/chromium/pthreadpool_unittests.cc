// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthreadpool.h>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef std::unique_ptr<pthreadpool, decltype(&pthreadpool_destroy)>
    auto_pthreadpool_t;

TEST(CreateAndDestroy, NullThreadPool) {
  pthreadpool* threadpool = nullptr;
  pthreadpool_destroy(threadpool);
}

TEST(CreateAndDestroy, SingleThreadPool) {
  pthreadpool* threadpool = pthreadpool_create(1);
  ASSERT_TRUE(threadpool);
  pthreadpool_destroy(threadpool);
}

TEST(CreateAndDestroy, MultiThreadPool) {
  pthreadpool* threadpool = pthreadpool_create(0);
  ASSERT_TRUE(threadpool);
  pthreadpool_destroy(threadpool);
}

struct ElementWiseAdd1DTester {
  struct pthreadpool* threadpool;
  size_t size;
  std::vector<float> lhs;
  std::vector<float> rhs;
  std::vector<float> sum;

  static void Compute(void* context, size_t i) {
    ElementWiseAdd1DTester* tester =
        static_cast<ElementWiseAdd1DTester*>(context);
    EXPECT_LT(i, tester->size);
    tester->sum[i] = tester->lhs[i] + tester->rhs[i];
  }

  void Run(uint32_t flags = 0) {
    pthreadpool_parallelize_1d(threadpool,
                               reinterpret_cast<pthreadpool_task_1d_t>(Compute),
                               static_cast<void*>(this), size, flags);
  }
};

TEST(ElementWiseAdd1D, SingleThreadPool) {
  base::test::TaskEnvironment task_environment;

  auto_pthreadpool_t threadpool(pthreadpool_create(1), pthreadpool_destroy);
  ASSERT_TRUE(threadpool.get());

  const size_t kSize = 100;
  ElementWiseAdd1DTester tester{.threadpool = threadpool.get(),
                                .size = kSize,
                                .lhs = std::vector<float>(kSize, 1.0),
                                .rhs = std::vector<float>(kSize, 2.0),
                                .sum = std::vector<float>(kSize, 0.0)};
  tester.Run();

  for (size_t i = 0; i < kSize; i++) {
    EXPECT_EQ(tester.sum[i], tester.lhs[i] + tester.rhs[i]);
  }
}

TEST(ElementWiseAdd1D, MultiThreadPool) {
  base::test::TaskEnvironment task_environment;

  auto_pthreadpool_t threadpool(pthreadpool_create(0), pthreadpool_destroy);
  ASSERT_TRUE(threadpool.get());

  const size_t kSize = 200;
  ElementWiseAdd1DTester tester{.threadpool = threadpool.get(),
                                .size = kSize,
                                .lhs = std::vector<float>(kSize, 1.0),
                                .rhs = std::vector<float>(kSize, 2.0),
                                .sum = std::vector<float>(kSize, 0.0)};
  tester.Run();

  for (size_t i = 0; i < kSize; i++) {
    EXPECT_EQ(tester.sum[i], tester.lhs[i] + tester.rhs[i]);
  }
}

TEST(ElementWiseAdd1D, WithDisableDenormalsFlag) {
  base::test::TaskEnvironment task_environment;

  auto_pthreadpool_t threadpool(pthreadpool_create(0), pthreadpool_destroy);
  ASSERT_TRUE(threadpool.get());

  const size_t kSize = 300;
  ElementWiseAdd1DTester tester{.threadpool = threadpool.get(),
                                .size = kSize,
                                .lhs = std::vector<float>(kSize, 1.0),
                                .rhs = std::vector<float>(kSize, 2.0),
                                .sum = std::vector<float>(kSize, 0.0)};
  tester.Run(PTHREADPOOL_FLAG_DISABLE_DENORMALS);

  for (size_t i = 0; i < kSize; i++) {
    EXPECT_EQ(tester.sum[i], tester.lhs[i] + tester.rhs[i]);
  }
}

struct ElementWiseAdd2DTester {
  struct pthreadpool* threadpool;
  size_t width;
  size_t height;
  std::vector<float> lhs;
  std::vector<float> rhs;
  std::vector<float> sum;

  static void Compute(void* context, size_t i, size_t j) {
    ElementWiseAdd2DTester* tester =
        static_cast<ElementWiseAdd2DTester*>(context);
    EXPECT_LT(i, tester->width);
    EXPECT_LT(i, tester->height);
    const size_t offset = i * tester->height + j;
    tester->sum[offset] = tester->lhs[offset] + tester->rhs[offset];
  }

  void Run(uint32_t flags = 0) {
    pthreadpool_parallelize_2d(threadpool,
                               reinterpret_cast<pthreadpool_task_2d_t>(Compute),
                               static_cast<void*>(this), width, height, flags);
  }
};

TEST(ElementWiseAdd2D, SingleThreadPool) {
  base::test::TaskEnvironment task_environment;

  auto_pthreadpool_t threadpool(pthreadpool_create(1), pthreadpool_destroy);
  ASSERT_TRUE(threadpool.get());

  const size_t kWidth = 10;
  const size_t kHeight = 10;
  ElementWiseAdd2DTester tester{
      .threadpool = threadpool.get(),
      .width = kWidth,
      .height = kHeight,
      .lhs = std::vector<float>(kWidth * kHeight, 1.0),
      .rhs = std::vector<float>(kWidth * kHeight, 2.0),
      .sum = std::vector<float>(kWidth * kHeight, 0.0)};
  tester.Run();

  for (size_t i = 0; i < kWidth; i++) {
    for (size_t j = 0; j < kHeight; j++) {
      const size_t index = i * kHeight + j;
      EXPECT_EQ(tester.sum[index], tester.lhs[index] + tester.rhs[index]);
    }
  }
}

TEST(ElementWiseAdd2D, MultiThreadPool) {
  base::test::TaskEnvironment task_environment;

  auto_pthreadpool_t threadpool(pthreadpool_create(0), pthreadpool_destroy);
  ASSERT_TRUE(threadpool.get());

  const size_t kWidth = 20;
  const size_t kHeight = 20;
  ElementWiseAdd2DTester tester{
      .threadpool = threadpool.get(),
      .width = kWidth,
      .height = kHeight,
      .lhs = std::vector<float>(kWidth * kHeight, 1.0),
      .rhs = std::vector<float>(kWidth * kHeight, 2.0),
      .sum = std::vector<float>(kWidth * kHeight, 0.0)};
  tester.Run();

  for (size_t i = 0; i < kWidth; i++) {
    for (size_t j = 0; j < kHeight; j++) {
      const size_t index = i * kHeight + j;
      EXPECT_EQ(tester.sum[index], tester.lhs[index] + tester.rhs[index]);
    }
  }
}

TEST(ElementWiseAdd2D, WithDisableDenormalsFlag) {
  base::test::TaskEnvironment task_environment;

  auto_pthreadpool_t threadpool(pthreadpool_create(0), pthreadpool_destroy);
  ASSERT_TRUE(threadpool.get());

  const size_t kWidth = 30;
  const size_t kHeight = 30;
  ElementWiseAdd2DTester tester{
      .threadpool = threadpool.get(),
      .width = kWidth,
      .height = kHeight,
      .lhs = std::vector<float>(kWidth * kHeight, 1.0),
      .rhs = std::vector<float>(kWidth * kHeight, 2.0),
      .sum = std::vector<float>(kWidth * kHeight, 0.0)};
  tester.Run(PTHREADPOOL_FLAG_DISABLE_DENORMALS);

  for (size_t i = 0; i < kWidth; i++) {
    for (size_t j = 0; j < kHeight; j++) {
      const size_t index = i * kHeight + j;
      EXPECT_EQ(tester.sum[index], tester.lhs[index] + tester.rhs[index]);
    }
  }
}
