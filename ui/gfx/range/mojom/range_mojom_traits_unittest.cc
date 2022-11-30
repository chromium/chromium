// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/mojom/range_traits_test_service.mojom.h"

namespace gfx {

namespace {

class RangeStructTraitsTest : public testing::Test,
                              public mojom::RangeTraitsTestService {
 public:
  RangeStructTraitsTest() {}

  RangeStructTraitsTest(const RangeStructTraitsTest&) = delete;
  RangeStructTraitsTest& operator=(const RangeStructTraitsTest&) = delete;

 protected:
  mojo::Remote<mojom::RangeTraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::RangeTraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // RangeTraitsTestService:
  void EchoRange(const Range& p, EchoRangeCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoRangeF(const RangeF& p, EchoRangeFCallback callback) override {
    std::move(callback).Run(p);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<RangeTraitsTestService> traits_test_receivers_;
};

}  // namespace

TEST_F(RangeStructTraitsTest, Range) {
  const size_t start = 1234;
  const size_t end = 5678;
  gfx::Range input(start, end);
  mojo::Remote<mojom::RangeTraitsTestService> remote = GetTraitsTestRemote();
  gfx::Range output;
  remote->EchoRange(input, &output);
  EXPECT_EQ(start, output.start());
  EXPECT_EQ(end, output.end());

  remote->EchoRange(gfx::Range::InvalidRange(), &output);
  EXPECT_FALSE(output.IsValid());
}

TEST_F(RangeStructTraitsTest, RangeF) {
  const float start = 1234.5f;
  const float end = 6789.6f;
  gfx::RangeF input(start, end);
  mojo::Remote<mojom::RangeTraitsTestService> remote = GetTraitsTestRemote();
  gfx::RangeF output;
  remote->EchoRangeF(input, &output);
  EXPECT_EQ(start, output.start());
  EXPECT_EQ(end, output.end());
}

}  // namespace gfx
