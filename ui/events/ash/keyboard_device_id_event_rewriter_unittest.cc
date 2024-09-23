// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_device_id_event_rewriter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ash/event_property.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_rewriter_continuation.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

namespace {

// Captures SendEvent calls.
class TestContinuation : public EventRewriterContinuation {
 public:
  TestContinuation() = default;
  TestContinuation(const TestContinuation&) = delete;
  TestContinuation& operator=(const TestContinuation&) = delete;
  ~TestContinuation() override = default;

  const std::vector<std::unique_ptr<Event>>& send_event_events() const {
    return send_event_events_;
  }

  base::WeakPtr<EventRewriterContinuation> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // EventRewriterContinuation:
  ui::EventDispatchDetails SendEvent(const ui::Event* event) override {
    send_event_events_.push_back(event->Clone());
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails SendEventFinally(const ui::Event* event) override {
    ADD_FAILURE();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DiscardEvent() override {
    ADD_FAILURE();
    return ui::EventDispatchDetails();
  }

 private:
  std::vector<std::unique_ptr<Event>> send_event_events_;
  base::WeakPtrFactory<TestContinuation> weak_ptr_factory_{this};
};

}  // namespace

class KeyboardDeviceIdEventRewriterTest : public testing::Test {
 public:
  KeyboardDeviceIdEventRewriterTest() = default;
  ~KeyboardDeviceIdEventRewriterTest() override = default;

  KeyboardDeviceIdEventRewriter& rewriter() { return *rewriter_; }
  KeyboardCapability& capability() { return *capability_; }

  void SetUp() override {
    DeviceDataManager::CreateInstance();
    capability_ = std::make_unique<KeyboardCapability>();
    rewriter_ =
        std::make_unique<KeyboardDeviceIdEventRewriter>(capability_.get());
  }

  void TearDown() override {
    rewriter_.reset();
    capability_.reset();
    DeviceDataManager::DeleteInstance();
  }

 private:
  std::unique_ptr<KeyboardCapability> capability_;
  std::unique_ptr<KeyboardDeviceIdEventRewriter> rewriter_;
};

TEST_F(KeyboardDeviceIdEventRewriterTest, KeyEventRewriting) {
  constexpr int kKeyboardDeviceId = 10;
  constexpr int kVirtualCoreKeyboardDeviceId = 20;

  KeyboardDevice device(kVirtualCoreKeyboardDeviceId,
                        /*type=*/InputDeviceType::INPUT_DEVICE_INTERNAL,
                        /*name=*/"virtual core keyboard");
  DeviceDataManagerTestApi().SetKeyboardDevices({device});
  KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard;
  capability().SetKeyboardInfoForTesting(device, std::move(keyboard_info));

  // Rewriting for non-virtual-core-keyboard has no effect.
  {
    auto key_event = std::make_unique<KeyEvent>(EventType::kKeyPressed, VKEY_A,
                                                /*flags=*/0);
    key_event->set_source_device_id(kKeyboardDeviceId);

    TestContinuation continuation;
    rewriter().RewriteEvent(*key_event, continuation.GetWeakPtr());
    ASSERT_EQ(1u, continuation.send_event_events().size());
    const Event& rewritten_event = *continuation.send_event_events()[0];
    EXPECT_EQ(kKeyboardDeviceId, GetKeyboardDeviceIdProperty(rewritten_event));
  }

  // Rewriting for virtual-core-keyboard annotates the device id
  // of the previous event.
  {
    auto key_event = std::make_unique<KeyEvent>(EventType::kKeyPressed, VKEY_A,
                                                /*flags=*/0);
    key_event->set_source_device_id(kVirtualCoreKeyboardDeviceId);

    TestContinuation continuation;
    rewriter().RewriteEvent(*key_event, continuation.GetWeakPtr());
    ASSERT_EQ(1u, continuation.send_event_events().size());
    const Event& rewritten_event = *continuation.send_event_events()[0];
    EXPECT_EQ(kKeyboardDeviceId, GetKeyboardDeviceIdProperty(rewritten_event));
  }
}

TEST_F(KeyboardDeviceIdEventRewriterTest, MotionEventRewriting) {
  constexpr int kKeyDeviceId = 10;
  constexpr int kMotionDeviceId = 20;

  // Rewriting for non-virtual-core-keyboard has no effect, but remembers it.
  {
    auto key_event = std::make_unique<KeyEvent>(EventType::kKeyPressed, VKEY_A,
                                                /*flags=*/0);
    key_event->set_source_device_id(kKeyDeviceId);

    TestContinuation continuation;
    rewriter().RewriteEvent(*key_event, continuation.GetWeakPtr());
    ASSERT_EQ(1u, continuation.send_event_events().size());
    const Event& rewritten_event = *continuation.send_event_events()[0];
    EXPECT_EQ(kKeyDeviceId, GetKeyboardDeviceIdProperty(rewritten_event));
  }

  // motion event is rewritten.
  {
    auto touch_event = std::make_unique<TouchEvent>(
        EventType::kTouchPressed, gfx::PointF(0.f, 0.f), gfx::PointF(0.f, 0.f),
        base::TimeTicks(), PointerDetails());
    touch_event->set_source_device_id(kMotionDeviceId);

    TestContinuation continuation;
    rewriter().RewriteEvent(*touch_event, continuation.GetWeakPtr());
    ASSERT_EQ(1u, continuation.send_event_events().size());
    const Event& rewritten_event = *continuation.send_event_events()[0];
    EXPECT_EQ(kKeyDeviceId, GetKeyboardDeviceIdProperty(rewritten_event));
  }
}

}  // namespace ui
