// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojom/ime_types_mojom_traits.h"

#include <utility>

#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/mojom/ime_mojom_traits_test.mojom.h"

namespace ui {

namespace {

class IMEStructTraitsTest : public testing::Test,
                            public mojom::IMEStructTraitsTest {
 public:
  IMEStructTraitsTest() {}

 protected:
  mojo::Remote<mojom::IMEStructTraitsTest> GetTraitsTestRemote() {
    mojo::Remote<mojom::IMEStructTraitsTest> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // mojom::IMEStructTraitsTest:
  void EchoTextInputType(ui::TextInputType in,
                         EchoTextInputTypeCallback callback) override {
    std::move(callback).Run(in);
  }

  base::test::TaskEnvironment task_environment_;  // A MessageLoop is needed for
                                                  // Mojo IPC to work.
  mojo::ReceiverSet<mojom::IMEStructTraitsTest> traits_test_receivers_;

  DISALLOW_COPY_AND_ASSIGN(IMEStructTraitsTest);
};

}  // namespace

TEST_F(IMEStructTraitsTest, TextInputType) {
  const ui::TextInputType kTextInputTypes[] = {
      ui::TEXT_INPUT_TYPE_NONE,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_TYPE_PASSWORD,
      ui::TEXT_INPUT_TYPE_SEARCH,
      ui::TEXT_INPUT_TYPE_EMAIL,
      ui::TEXT_INPUT_TYPE_NUMBER,
      ui::TEXT_INPUT_TYPE_TELEPHONE,
      ui::TEXT_INPUT_TYPE_URL,
      ui::TEXT_INPUT_TYPE_DATE,
      ui::TEXT_INPUT_TYPE_DATE_TIME,
      ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
      ui::TEXT_INPUT_TYPE_MONTH,
      ui::TEXT_INPUT_TYPE_TIME,
      ui::TEXT_INPUT_TYPE_WEEK,
      ui::TEXT_INPUT_TYPE_TEXT_AREA,
      ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE,
      ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD,
  };

  mojo::Remote<mojom::IMEStructTraitsTest> remote = GetTraitsTestRemote();
  for (size_t i = 0; i < base::size(kTextInputTypes); i++) {
    ui::TextInputType type_out;
    ASSERT_TRUE(remote->EchoTextInputType(kTextInputTypes[i], &type_out));
    EXPECT_EQ(kTextInputTypes[i], type_out);
  }
}

}  // namespace ui
