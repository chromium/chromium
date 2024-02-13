// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/notification_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace message_center {

class NotificationDelegateTest : public testing::Test {
 public:
  NotificationDelegateTest() = default;

  NotificationDelegateTest(const NotificationDelegateTest&) = delete;
  NotificationDelegateTest& operator=(const NotificationDelegateTest&) = delete;

  ~NotificationDelegateTest() override = default;

  void BodyClickCallback() { ++callback_count_; }

  void ButtonClickCallback(std::optional<int> button_index) {
    ++callback_count_;
    last_button_index_ = button_index;
  }

 protected:
  int callback_count_ = 0;
  std::optional<int> last_button_index_;
};

TEST_F(NotificationDelegateTest, ClickDelegate) {
  auto delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating(&NotificationDelegateTest::BodyClickCallback,
                          base::Unretained(this)));

  delegate->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(1, callback_count_);
}

TEST_F(NotificationDelegateTest, NullClickDelegate) {
  auto delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::RepeatingClosure());

  delegate->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(0, callback_count_);
}

TEST_F(NotificationDelegateTest, ButtonClickDelegate) {
  auto delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating(&NotificationDelegateTest::ButtonClickCallback,
                          base::Unretained(this)));

  delegate->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(std::nullopt, last_button_index_);

  delegate->Click(3, std::nullopt);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(3, *last_button_index_);
}

}  // namespace message_center
