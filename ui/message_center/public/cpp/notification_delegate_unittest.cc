// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/notification_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace message_center {

class NotificationDelegateTest : public testing::Test {
 public:
  NotificationDelegateTest() = default;
  ~NotificationDelegateTest() override = default;

  void BodyClickCallback() { ++callback_count_; }

  void ButtonClickCallback(base::Optional<int> button_index) {
    ++callback_count_;
    last_button_index_ = button_index;
  }

 protected:
  int callback_count_ = 0;
  base::Optional<int> last_button_index_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationDelegateTest);
};

TEST_F(NotificationDelegateTest, ClickDelegate) {
  auto delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating(&NotificationDelegateTest::BodyClickCallback,
                          base::Unretained(this)));

  delegate->Click(base::nullopt, base::nullopt);
  EXPECT_EQ(1, callback_count_);
}

TEST_F(NotificationDelegateTest, NullClickDelegate) {
  auto delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::RepeatingClosure());

  delegate->Click(base::nullopt, base::nullopt);
  EXPECT_EQ(0, callback_count_);
}

TEST_F(NotificationDelegateTest, ButtonClickDelegate) {
  auto delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating(&NotificationDelegateTest::ButtonClickCallback,
                          base::Unretained(this)));

  delegate->Click(base::nullopt, base::nullopt);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(base::nullopt, last_button_index_);

  delegate->Click(3, base::nullopt);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(3, *last_button_index_);
}

}  // namespace message_center
