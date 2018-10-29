// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gestures/gesture_recognizer_observer.h"

namespace ui {

class TestGestureRecognizerObserver : public GestureRecognizerObserver {
 public:
  TestGestureRecognizerObserver(GestureRecognizer* gesture_recognizer)
      : gesture_recognizer_(gesture_recognizer) {
    gesture_recognizer_->AddObserver(this);
  }

  ~TestGestureRecognizerObserver() override {
    gesture_recognizer_->RemoveObserver(this);
  }

  int active_touches_cancelled_except_call_count() const {
    return active_touches_cancelled_except_call_count_;
  }
  int active_touches_cancelled_call_count() const {
    return active_touches_cancelled_call_count_;
  }
  int events_transfer_call_count() const { return events_transfer_call_count_; }

  const GestureConsumer* last_not_cancelled() const {
    return last_not_cancelled_;
  }
  const GestureConsumer* last_cancelled() const {
    return last_cancelled_consumer_;
  }

  const GestureConsumer* last_transferred_source() const {
    return last_transferred_source_;
  }
  const GestureConsumer* last_transferred_destination() const {
    return last_transferred_destination_;
  }
  TransferTouchesBehavior last_transfer_touches_behavior() const {
    return last_transfer_touches_behavior_;
  }

 private:
  // GestureRecognizerObserver:
  void OnActiveTouchesCanceledExcept(GestureConsumer* not_cancelled) override {
    active_touches_cancelled_except_call_count_++;
    last_not_cancelled_ = not_cancelled;
  }
  void OnEventsTransferred(
      GestureConsumer* current_consumer,
      GestureConsumer* new_consumer,
      TransferTouchesBehavior transfer_touches_behavior) override {
    events_transfer_call_count_++;
    last_transferred_source_ = current_consumer;
    last_transferred_destination_ = new_consumer;
    last_transfer_touches_behavior_ = transfer_touches_behavior;
  }
  void OnActiveTouchesCanceled(GestureConsumer* consumer) override {
    active_touches_cancelled_call_count_++;
    last_cancelled_consumer_ = consumer;
  }

  GestureRecognizer* gesture_recognizer_;

  int active_touches_cancelled_except_call_count_ = 0;
  int active_touches_cancelled_call_count_ = 0;
  int events_transfer_call_count_ = 0;
  GestureConsumer* last_not_cancelled_ = nullptr;
  GestureConsumer* last_cancelled_consumer_ = nullptr;
  GestureConsumer* last_transferred_source_ = nullptr;
  GestureConsumer* last_transferred_destination_ = nullptr;
  TransferTouchesBehavior last_transfer_touches_behavior_ =
      TransferTouchesBehavior::kCancel;

  DISALLOW_COPY_AND_ASSIGN(TestGestureRecognizerObserver);
};

class TestGestureEventHelper : public GestureEventHelper {
 public:
  TestGestureEventHelper() = default;
  ~TestGestureEventHelper() override = default;

 private:
  // GestureEventHelper:
  bool CanDispatchToConsumer(GestureConsumer* consumer) override {
    return true;
  }
  void DispatchGestureEvent(GestureConsumer* raw_input_consumer,
                            GestureEvent* event) override {}
  void DispatchSyntheticTouchEvent(TouchEvent* event) override {}

  DISALLOW_COPY_AND_ASSIGN(TestGestureEventHelper);
};

class GestureRecognizerImplTest : public testing::Test {
 public:
  GestureRecognizerImplTest() = default;
  ~GestureRecognizerImplTest() override = default;

  void SetUp() override {
    gesture_recognizer_.helpers().push_back(&helper_);
    observer_ =
        std::make_unique<TestGestureRecognizerObserver>(&gesture_recognizer_);
  }

  void TearDown() override { observer_.reset(); }

 protected:
  GestureRecognizer* gesture_recognizer() { return &gesture_recognizer_; }
  TestGestureRecognizerObserver* observer() { return observer_.get(); }

 private:
  GestureRecognizerImpl gesture_recognizer_;
  TestGestureEventHelper helper_;
  std::unique_ptr<TestGestureRecognizerObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerImplTest);
};

TEST_F(GestureRecognizerImplTest, CancelActiveTouchEvents) {
  GestureConsumer consumer;
  gesture_recognizer()->CancelActiveTouches(&consumer);

  EXPECT_EQ(1, observer()->active_touches_cancelled_call_count());
  EXPECT_EQ(&consumer, observer()->last_cancelled());
}

TEST_F(GestureRecognizerImplTest, CancelActiveTouchEventsExcept) {
  GestureConsumer consumer;
  gesture_recognizer()->CancelActiveTouchesExcept(&consumer);

  // OnActiveTouchesCancelled() shouldn't occur.
  EXPECT_EQ(0, observer()->active_touches_cancelled_call_count());
  EXPECT_EQ(1, observer()->active_touches_cancelled_except_call_count());
  EXPECT_EQ(&consumer, observer()->last_not_cancelled());
}

TEST_F(GestureRecognizerImplTest, CancelActiveTouchEventsExceptNullPtr) {
  gesture_recognizer()->CancelActiveTouchesExcept(nullptr);

  EXPECT_EQ(1, observer()->active_touches_cancelled_except_call_count());
  EXPECT_FALSE(observer()->last_not_cancelled());
}

TEST_F(GestureRecognizerImplTest, TransferEventsTo) {
  GestureConsumer consumer1;
  GestureConsumer consumer2;

  gesture_recognizer()->TransferEventsTo(&consumer1, &consumer2,
                                         TransferTouchesBehavior::kDontCancel);
  EXPECT_EQ(1, observer()->events_transfer_call_count());
  EXPECT_EQ(&consumer1, observer()->last_transferred_source());
  EXPECT_EQ(&consumer2, observer()->last_transferred_destination());
  EXPECT_EQ(TransferTouchesBehavior::kDontCancel,
            observer()->last_transfer_touches_behavior());
}

}  // namespace ui
