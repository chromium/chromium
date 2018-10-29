// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/input_handler_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/trees/swap_promise_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/compositor_thread_event_queue.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/event_with_callback.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"
#include "ui/events/blink/scroll_predictor.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/latency/latency_info.h"

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using testing::Field;

namespace ui {
namespace test {

namespace {

const char* kCoalescedCountHistogram =
    "Event.CompositorThreadEventQueue.CoalescedCount";
const char* kContinuousHeadQueueingTimeHistogram =
    "Event.CompositorThreadEventQueue.Continuous.HeadQueueingTime";
const char* kContinuousTailQueueingTimeHistogram =
    "Event.CompositorThreadEventQueue.Continuous.TailQueueingTime";
const char* kNonContinuousQueueingTimeHistogram =
    "Event.CompositorThreadEventQueue.NonContinuous.QueueingTime";

enum InputHandlerProxyTestType {
  ROOT_SCROLL_NORMAL_HANDLER,
  ROOT_SCROLL_SYNCHRONOUS_HANDLER,
  CHILD_SCROLL_NORMAL_HANDLER,
  CHILD_SCROLL_SYNCHRONOUS_HANDLER,
};
static const InputHandlerProxyTestType test_types[] = {
    ROOT_SCROLL_NORMAL_HANDLER, ROOT_SCROLL_SYNCHRONOUS_HANDLER,
    CHILD_SCROLL_NORMAL_HANDLER, CHILD_SCROLL_SYNCHRONOUS_HANDLER};

MATCHER_P(WheelEventsMatch, expected, "") {
  return WheelEventsMatch(arg, expected);
}

WebScopedInputEvent CreateGestureScrollPinch(WebInputEvent::Type type,
                                             WebGestureDevice source_device,
                                             float delta_y_or_scale = 0,
                                             int x = 0,
                                             int y = 0) {
  WebGestureEvent gesture(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          source_device);
  if (type == WebInputEvent::kGestureScrollUpdate) {
    gesture.data.scroll_update.delta_y = delta_y_or_scale;
  } else if (type == WebInputEvent::kGesturePinchUpdate) {
    gesture.data.pinch_update.scale = delta_y_or_scale;
    gesture.SetPositionInWidget(gfx::PointF(x, y));
  }
  return WebInputEventTraits::Clone(gesture);
}

class MockInputHandler : public cc::InputHandler {
 public:
  MockInputHandler() {}
  ~MockInputHandler() override {}

  MOCK_METHOD0(PinchGestureBegin, void());
  MOCK_METHOD2(PinchGestureUpdate,
               void(float magnify_delta, const gfx::Point& anchor));
  MOCK_METHOD2(PinchGestureEnd, void(const gfx::Point& anchor, bool snap));

  MOCK_METHOD0(SetNeedsAnimateInput, void());

  MOCK_METHOD2(ScrollBegin,
               ScrollStatus(cc::ScrollState*,
                            cc::InputHandler::ScrollInputType type));
  MOCK_METHOD2(RootScrollBegin,
               ScrollStatus(cc::ScrollState*,
                            cc::InputHandler::ScrollInputType type));
  MOCK_METHOD1(ScrollAnimatedBegin, ScrollStatus(cc::ScrollState*));
  MOCK_METHOD3(ScrollAnimated,
               ScrollStatus(const gfx::Point& viewport_point,
                            const gfx::Vector2dF& scroll_delta,
                            base::TimeDelta));
  MOCK_METHOD1(ScrollBy, cc::InputHandlerScrollResult(cc::ScrollState*));
  MOCK_METHOD2(ScrollEnd, void(cc::ScrollState*, bool));
  MOCK_METHOD0(ScrollingShouldSwitchtoMainThread, bool());

  std::unique_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override {
    return nullptr;
  }

  cc::ScrollElasticityHelper* CreateScrollElasticityHelper() override {
    return NULL;
  }
  bool GetScrollOffsetForLayer(cc::ElementId element_id,
                               gfx::ScrollOffset* offset) override {
    return false;
  }
  bool ScrollLayerTo(cc::ElementId element_id,
                     const gfx::ScrollOffset& offset) override {
    return false;
  }

  void BindToClient(cc::InputHandlerClient* client) override {}

  void MouseDown() override {}
  void MouseUp() override {}
  void MouseLeave() override {}

  void MouseMoveAt(const gfx::Point& mouse_position) override {}

  MOCK_CONST_METHOD2(IsCurrentlyScrollingLayerAt,
                     bool(const gfx::Point& point,
                          cc::InputHandler::ScrollInputType type));

  MOCK_CONST_METHOD1(
      GetEventListenerProperties,
      cc::EventListenerProperties(cc::EventListenerClass event_class));
  MOCK_METHOD2(EventListenerTypeForTouchStartOrMoveAt,
               cc::InputHandler::TouchStartOrMoveEventListenerType(
                   const gfx::Point& point,
                   cc::TouchAction* touch_action));
  MOCK_CONST_METHOD1(HasBlockingWheelEventHandlerAt, bool(const gfx::Point&));

  MOCK_METHOD0(RequestUpdateForSynchronousInputHandler, void());
  MOCK_METHOD1(SetSynchronousInputHandlerRootScrollOffset,
               void(const gfx::ScrollOffset& root_offset));

  bool IsCurrentlyScrollingViewport() const override {
    return is_scrolling_root_;
  }
  void set_is_scrolling_root(bool is) { is_scrolling_root_ = is; }

  MOCK_CONST_METHOD3(GetSnapFlingInfo,
                     bool(const gfx::Vector2dF& natural_displacement,
                          gfx::Vector2dF* initial_offset,
                          gfx::Vector2dF* target_offset));

 private:
  bool is_scrolling_root_ = true;
  DISALLOW_COPY_AND_ASSIGN(MockInputHandler);
};

class MockSynchronousInputHandler : public SynchronousInputHandler {
 public:
  MOCK_METHOD0(SetNeedsSynchronousAnimateInput, void());
  MOCK_METHOD6(UpdateRootLayerState,
               void(const gfx::ScrollOffset& total_scroll_offset,
                    const gfx::ScrollOffset& max_scroll_offset,
                    const gfx::SizeF& scrollable_size,
                    float page_scale_factor,
                    float min_page_scale_factor,
                    float max_page_scale_factor));
};

class MockInputHandlerProxyClient
    : public InputHandlerProxyClient {
 public:
  MockInputHandlerProxyClient() {}
  ~MockInputHandlerProxyClient() override {}

  void WillShutdown() override {}

  MOCK_METHOD1(DispatchNonBlockingEventToMainThread_,
               void(const WebInputEvent&));

  MOCK_METHOD1(GenerateScrollBeginAndSendToMainThread,
               void(const blink::WebGestureEvent& update_event));

  void DispatchNonBlockingEventToMainThread(
      WebScopedInputEvent event,
      const ui::LatencyInfo& latency_info) override {
    CHECK(event.get());
    DispatchNonBlockingEventToMainThread_(*event.get());
  }

  MOCK_METHOD5(DidOverscroll,
               void(const gfx::Vector2dF& accumulated_overscroll,
                    const gfx::Vector2dF& latest_overscroll_delta,
                    const gfx::Vector2dF& current_fling_velocity,
                    const gfx::PointF& causal_event_viewport_point,
                    const cc::OverscrollBehavior& overscroll_behavior));
  void DidAnimateForInput() override {}
  void DidStartScrollingViewport() override {}
  MOCK_METHOD3(SetWhiteListedTouchAction,
               void(cc::TouchAction touch_action,
                    uint32_t unique_touch_event_id,
                    InputHandlerProxy::EventDisposition event_disposition));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputHandlerProxyClient);
};

class MockInputHandlerProxyClientWithDidAnimateForInput
    : public MockInputHandlerProxyClient {
 public:
  MockInputHandlerProxyClientWithDidAnimateForInput() {}
  ~MockInputHandlerProxyClientWithDidAnimateForInput() override {}

  MOCK_METHOD0(DidAnimateForInput, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputHandlerProxyClientWithDidAnimateForInput);
};

WebTouchPoint CreateWebTouchPoint(WebTouchPoint::State state, float x,
                                  float y) {
  WebTouchPoint point;
  point.state = state;
  point.SetPositionInScreen(x, y);
  point.SetPositionInWidget(x, y);
  return point;
}

const cc::InputHandler::ScrollStatus kImplThreadScrollState(
    cc::InputHandler::SCROLL_ON_IMPL_THREAD,
    cc::MainThreadScrollingReason::kNotScrollingOnMain);

const cc::InputHandler::ScrollStatus kMainThreadScrollState(
    cc::InputHandler::SCROLL_ON_MAIN_THREAD,
    cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

const cc::InputHandler::ScrollStatus kScrollIgnoredScrollState(
    cc::InputHandler::SCROLL_IGNORED,
    cc::MainThreadScrollingReason::kNotScrollable);

}  // namespace

class TestInputHandlerProxy : public InputHandlerProxy {
 public:
  TestInputHandlerProxy(cc::InputHandler* input_handler,
                        InputHandlerProxyClient* client)
      : InputHandlerProxy(input_handler, client) {}
  void RecordMainThreadScrollingReasonsForTest(blink::WebGestureDevice device,
                                               uint32_t reasons) {
    RecordMainThreadScrollingReasons(device, reasons);
  }

  EventDisposition HitTestTouchEventForTest(
      const blink::WebTouchEvent& touch_event,
      bool* is_touching_scrolling_layer,
      cc::TouchAction* white_listed_touch_action) {
    return HitTestTouchEvent(touch_event, is_touching_scrolling_layer,
                             white_listed_touch_action);
  }

  EventDisposition HandleMouseWheelForTest(
      const blink::WebMouseWheelEvent& wheel_event) {
    return HandleMouseWheel(wheel_event);
  }
};

class InputHandlerProxyTest
    : public testing::Test,
      public testing::WithParamInterface<InputHandlerProxyTestType> {
 public:
  InputHandlerProxyTest()
      : synchronous_root_scroll_(GetParam() == ROOT_SCROLL_SYNCHRONOUS_HANDLER),
        install_synchronous_handler_(
            GetParam() == ROOT_SCROLL_SYNCHRONOUS_HANDLER ||
            GetParam() == CHILD_SCROLL_SYNCHRONOUS_HANDLER),
        expected_disposition_(InputHandlerProxy::DID_HANDLE) {
    input_handler_.reset(
        new TestInputHandlerProxy(&mock_input_handler_, &mock_client_));
    scroll_result_did_scroll_.did_scroll = true;
    scroll_result_did_not_scroll_.did_scroll = false;

    if (install_synchronous_handler_) {
      EXPECT_CALL(mock_input_handler_,
                  RequestUpdateForSynchronousInputHandler())
          .Times(1);
      input_handler_->SetOnlySynchronouslyAnimateRootFlings(
          &mock_synchronous_input_handler_);
    }

    mock_input_handler_.set_is_scrolling_root(synchronous_root_scroll_);

    // Set a default device so tests don't always have to set this.
    gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchpad);
  }

  virtual ~InputHandlerProxyTest() { input_handler_.reset(); }

// This is defined as a macro so the line numbers can be traced back to the
// correct spot when it fails.
#define EXPECT_SET_NEEDS_ANIMATE_INPUT(times)                                \
  do {                                                                       \
    if (synchronous_root_scroll_) {                                          \
      EXPECT_CALL(mock_synchronous_input_handler_,                           \
                  SetNeedsSynchronousAnimateInput())                         \
          .Times(times);                                                     \
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(0);     \
    } else {                                                                 \
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(times); \
      EXPECT_CALL(mock_synchronous_input_handler_,                           \
                  SetNeedsSynchronousAnimateInput())                         \
          .Times(0);                                                         \
    }                                                                        \
  } while (false)

// This is defined as a macro because when an expectation is not satisfied the
// only output you get out of gmock is the line number that set the expectation.
#define VERIFY_AND_RESET_MOCKS()                                     \
  do {                                                               \
    testing::Mock::VerifyAndClearExpectations(&mock_input_handler_); \
    testing::Mock::VerifyAndClearExpectations(                       \
        &mock_synchronous_input_handler_);                           \
    testing::Mock::VerifyAndClearExpectations(&mock_client_);        \
  } while (false)

  void Animate(base::TimeTicks time) {
    if (synchronous_root_scroll_) {
      input_handler_->SynchronouslyAnimate(time);
    } else {
      input_handler_->Animate(time);
    }
  }

  void SetSmoothScrollEnabled(bool value) {
    input_handler_->smooth_scroll_enabled_ = value;
  }

  base::HistogramTester& histogram_tester() {
    return histogram_tester_;
  }

 protected:
  void GestureScrollStarted();
  void ScrollHandlingSwitchedToMainThread();
  void GestureScrollIgnored();
  void FlingAndSnap();

  const bool synchronous_root_scroll_;
  const bool install_synchronous_handler_;
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler_;
  std::unique_ptr<TestInputHandlerProxy> input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  WebGestureEvent gesture_;
  InputHandlerProxy::EventDisposition expected_disposition_;
  base::HistogramTester histogram_tester_;
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
};

class InputHandlerProxyEventQueueTest : public testing::Test {
 public:
  InputHandlerProxyEventQueueTest() : weak_ptr_factory_(this) {
    feature_list_.InitAndEnableFeature(features::kVsyncAlignedInputEvents);
  }

  ~InputHandlerProxyEventQueueTest() { input_handler_proxy_.reset(); }

  void SetUp() override {
    event_disposition_recorder_.clear();
    latency_info_recorder_.clear();
    input_handler_proxy_ = std::make_unique<TestInputHandlerProxy>(
        &mock_input_handler_, &mock_client_);
    if (input_handler_proxy_->compositor_event_queue_)
      input_handler_proxy_->compositor_event_queue_ =
          std::make_unique<CompositorThreadEventQueue>();
    input_handler_proxy_->scroll_predictor_ =
        std::make_unique<ScrollPredictor>(true /* enable_resampling */);
  }

  void HandleGestureEvent(WebInputEvent::Type type,
                          float delta_y_or_scale = 0,
                          int x = 0,
                          int y = 0) {
    HandleGestureEventWithSourceDevice(
        type, blink::kWebGestureDeviceTouchscreen, delta_y_or_scale, x, y);
  }

  void HandleGestureEventWithSourceDevice(WebInputEvent::Type type,
                                          WebGestureDevice source_device,
                                          float delta_y_or_scale = 0,
                                          int x = 0,
                                          int y = 0) {
    LatencyInfo latency;
    input_handler_proxy_->HandleInputEventWithLatencyInfo(
        CreateGestureScrollPinch(type, source_device, delta_y_or_scale, x, y),
        latency,
        base::BindOnce(
            &InputHandlerProxyEventQueueTest::DidHandleInputEventAndOverscroll,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DidHandleInputEventAndOverscroll(
      InputHandlerProxy::EventDisposition event_disposition,
      WebScopedInputEvent input_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params) {
    event_disposition_recorder_.push_back(event_disposition);
    latency_info_recorder_.push_back(latency_info);
  }

  base::circular_deque<std::unique_ptr<EventWithCallback>>& event_queue() {
    return input_handler_proxy_->compositor_event_queue_->queue_;
  }

  void SetInputHandlerProxyTickClockForTesting(
      const base::TickClock* tick_clock) {
    input_handler_proxy_->SetTickClockForTesting(tick_clock);
  }

  bool GestureScrollEventPredictionAvailable(
      ui::InputPredictor::InputData* result) {
    return input_handler_proxy_->scroll_predictor_->predictor_
        ->GeneratePrediction(WebInputEvent::GetStaticTimeStampForTests(),
                             result);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  std::unique_ptr<TestInputHandlerProxy> input_handler_proxy_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  std::vector<InputHandlerProxy::EventDisposition> event_disposition_recorder_;
  std::vector<ui::LatencyInfo> latency_info_recorder_;

  base::MessageLoop loop_;
  base::WeakPtrFactory<InputHandlerProxyEventQueueTest> weak_ptr_factory_;
};

TEST_P(InputHandlerProxyTest, MouseWheelNoListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(true));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingAndPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(true));
  // We will not call GetEventListenerProperties because we early out when we
  // hit blocking region.
  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelEventOutsideBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Gt(10))))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Le(10))))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kBlocking));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  wheel.SetPositionInScreen(0, 5);
  wheel.SetPositionInWidget(0, 5);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       MouseWheelEventOutsideBlockingListenerWithPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Gt(10))))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Le(10))))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(
          testing::Return(cc::EventListenerProperties::kBlockingAndPassive));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  wheel.SetPositionInScreen(0, 5);
  wheel.SetPositionInWidget(0, 5);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::GestureScrollStarted() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,input_handler_->HandleInputEvent(gesture_));

  // The event should not be marked as handled if scrolling is not possible.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // Mark the event as handled if scroll happens.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, GestureScrollStarted) {
  GestureScrollStarted();
}

TEST_P(InputHandlerProxyTest, GestureScrollOnMainThread) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 40;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollIgnored) {
  // We shouldn't handle the GestureScrollBegin.
  // Instead, we should get a DROP_EVENT result, indicating
  // that we could determine that there's nothing that could scroll or otherwise
  // react to this gesture sequence and thus we should drop the whole gesture
  // sequence on the floor, except for the ScrollEnd.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kScrollIgnoredScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // GSB is dropped and not sent to the main thread, GSE shouldn't get sent to
  // the main thread, either.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollByPage) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.data.scroll_begin.delta_hint_units =
      WebGestureEvent::ScrollUnits::kPage;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 1;
  gesture_.data.scroll_update.delta_units = WebGestureEvent::ScrollUnits::kPage;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

// Mac does not smooth scroll wheel events (crbug.com/574283).
#if !defined(OS_MACOSX)
TEST_P(InputHandlerProxyTest, GestureScrollByCoarsePixels) {
#else
TEST_P(InputHandlerProxyTest, DISABLED_GestureScrollByCoarsePixels) {
#endif
  SetSmoothScrollEnabled(true);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.data.scroll_begin.delta_hint_units =
      WebGestureEvent::ScrollUnits::kPixels;
  EXPECT_CALL(mock_input_handler_, ScrollAnimatedBegin(::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_units =
      WebGestureEvent::ScrollUnits::kPixels;

  EXPECT_CALL(mock_input_handler_,
              ScrollAnimated(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollBeginThatTargetViewport) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, RootScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.data.scroll_begin.target_viewport = true;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::FlingAndSnap() {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // The event should be dropped if InputHandler decides to snap.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  gesture_.data.scroll_update.inertial_phase =
      blink::WebGestureEvent::kMomentumPhase;
  EXPECT_CALL(mock_input_handler_,
              GetSnapFlingInfo(testing::_, testing::_, testing::_))
      .WillOnce(DoAll(testing::SetArgPointee<1>(gfx::Vector2dF(0, 0)),
                      testing::SetArgPointee<2>(gfx::Vector2dF(0, 100)),
                      testing::Return(true)));
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_)).Times(1);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, SnapFlingIgnoresFollowingGSUAndGSE) {
  FlingAndSnap();
  // The next GestureScrollUpdate should also be ignored, and will not ask for
  // snap position.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;

  EXPECT_CALL(mock_input_handler_,
              GetSnapFlingInfo(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_)).Times(0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The GestureScrollEnd should also be ignored.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_end.inertial_phase =
      blink::WebGestureEvent::kMomentumPhase;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, testing::_)).Times(0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinch) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6), true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinchAfterScrollOnMainThread) {
  // Scrolls will start by being sent to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 40;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // However, after the pinch gesture starts, they should go to the impl
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6), true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After the pinch gesture ends, they should go to back to the main
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::ScrollHandlingSwitchedToMainThread() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  // HandleGestureScrollBegin will set gesture_scroll_on_impl_thread_.
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = -40;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // The scroll handling switches to the main thread, a GSB is sent to the main
  // thread to initiate the hit testing and compute the scroll chain.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client_,
              GenerateScrollBeginAndSendToMainThread(::testing::_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, WheelScrollHandlingSwitchedToMainThread) {
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchpad);
  ScrollHandlingSwitchedToMainThread();
}
TEST_P(InputHandlerProxyTest, TouchScrollHandlingSwitchedToMainThread) {
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  ScrollHandlingSwitchedToMainThread();
}

TEST_P(InputHandlerProxyTest,
       GestureScrollOnImplThreadFlagClearedAfterScrollEnd) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should be false once a GestureScrollEnd
  // gets handled.
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       BeginScrollWhenGestureScrollOnImplThreadFlagIsSet) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNonNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Eq(0)), testing::_))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionMax;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Invoke([](const gfx::Point&, cc::TouchAction* touch_action) {
            *touch_action = cc::kTouchActionPanUp;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                HANDLER_ON_SCROLLING_LAYER;
          }));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction white_listed_touch_action = cc::kTouchActionAuto;
  EXPECT_EQ(expected_disposition_, input_handler_->HitTestTouchEventForTest(
                                       touch, &is_touching_scrolling_layer,
                                       &white_listed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_EQ(white_listed_touch_action, cc::kTouchActionPanUp);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Eq(0)), testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::kTouchMove, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction* white_listed_touch_action = nullptr;
  EXPECT_EQ(expected_disposition_, input_handler_->HitTestTouchEventForTest(
                                       touch, &is_touching_scrolling_layer,
                                       white_listed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_TRUE(!white_listed_touch_action);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestNegative) {
  // None of the three touch points fall in the touch region. So the event
  // should be dropped.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke([](const gfx::Point&,
                                         cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionPanUp;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(cc::kTouchActionPanUp, 1,
                                        InputHandlerProxy::DROP_EVENT))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStateStationary, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPositive) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Eq(0)), testing::_))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionAuto;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Invoke([](const gfx::Point&, cc::TouchAction* touch_action) {
            *touch_action = cc::kTouchActionPanY;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                HANDLER_ON_SCROLLING_LAYER;
          }));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(cc::kTouchActionPanY, 1,
                                        InputHandlerProxy::DID_NOT_HANDLE))
      .WillOnce(testing::Return());
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPassivePositive) {
  // One of the touch points is not on a touch-region. So the event should be
  // sent to the impl thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .Times(3)
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionPanRight;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }))
      .WillRepeatedly(testing::Invoke([](const gfx::Point&,
                                         cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionPanX;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(
                                cc::kTouchActionPanRight, 1,
                                InputHandlerProxy::DID_HANDLE_NON_BLOCKING))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchStartPassiveAndTouchEndBlocking) {
  // The touch start is not in a touch-region but there is a touch end handler
  // so to maintain targeting we need to dispatch the touch start as
  // non-blocking but drop all touch moves.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionNone;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(
                                cc::kTouchActionNone, 1,
                                InputHandlerProxy::DID_HANDLE_NON_BLOCKING))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.unique_touch_event_id = 1;
  touch.touches_length = 1;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touch_start_or_first_touch_move = true;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  touch.SetType(WebInputEvent::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = false;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchMoveBlockingAddedAfterPassiveTouchStart) {
  // The touch start is not in a touch-region but there is a touch end handler
  // so to maintain targeting we need to dispatch the touch start as
  // non-blocking but drop all touch moves.
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE_NON_BLOCKING,
            input_handler_->HandleInputEvent(touch));

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::HANDLER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  touch.SetType(WebInputEvent::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStateMoved, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            input_handler_->HandleInputEvent(touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST(SynchronousInputHandlerProxyTest, StartupShutdown) {
  testing::StrictMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  ui::InputHandlerProxy proxy(&mock_input_handler, &mock_client);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(1);
  proxy.SetOnlySynchronouslyAnimateRootFlings(&mock_synchronous_input_handler);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(0);
  proxy.SetOnlySynchronouslyAnimateRootFlings(nullptr);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, UpdateRootLayerState) {
  testing::NiceMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  ui::InputHandlerProxy proxy(&mock_input_handler, &mock_client);

  proxy.SetOnlySynchronouslyAnimateRootFlings(&mock_synchronous_input_handler);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(
      mock_synchronous_input_handler,
      UpdateRootLayerState(gfx::ScrollOffset(1, 2), gfx::ScrollOffset(3, 4),
                           gfx::SizeF(5, 6), 7, 8, 9))
      .Times(1);
  proxy.UpdateRootLayerStateForSynchronousInputHandler(
      gfx::ScrollOffset(1, 2), gfx::ScrollOffset(3, 4), gfx::SizeF(5, 6), 7, 8,
      9);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, SetOffset) {
  testing::NiceMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  ui::InputHandlerProxy proxy(&mock_input_handler, &mock_client);

  proxy.SetOnlySynchronouslyAnimateRootFlings(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, SetSynchronousInputHandlerRootScrollOffset(
                                      gfx::ScrollOffset(5, 6)));
  proxy.SynchronouslySetRootScrollOffset(gfx::ScrollOffset(5, 6));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedGestureScroll) {
  base::HistogramTester histogram_tester;

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);

  // GestureScrollBegin will be processed immediately.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[0]);

  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);

  // GestureScrollUpdate will be queued.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(-20, static_cast<const blink::WebGestureEvent&>(
                     event_queue().front()->event())
                     .data.scroll_update.delta_y);
  EXPECT_EQ(1ul, event_queue().front()->coalesced_count());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);

  // GestureScrollUpdate will be coalesced.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(-60, static_cast<const blink::WebGestureEvent&>(
                     event_queue().front()->event())
                     .data.scroll_update.delta_y);
  EXPECT_EQ(2ul, event_queue().front()->coalesced_count());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // GestureScrollEnd will be queued.
  EXPECT_EQ(2ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));

  // Dispatch all queued events.
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[1]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[2]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[3]);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  histogram_tester.ExpectUniqueSample(kCoalescedCountHistogram, 2, 1);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedGestureScrollPinchScroll) {
  base::HistogramTester histogram_tester;

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);

  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Continue scroll in the second frame, pinch, then start another scroll.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true)).Times(2);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  // Two |GesturePinchUpdate| will be coalesced.
  EXPECT_CALL(mock_input_handler_,
              PinchGestureUpdate(0.7f, gfx::Point(13, 17)));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(), false));

  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 1.4f, 13, 17);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 0.5f, 13, 17);
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -70);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -5);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  EXPECT_EQ(8ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(12ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  histogram_tester.ExpectBucketCount(kCoalescedCountHistogram, 1, 2);
  histogram_tester.ExpectBucketCount(kCoalescedCountHistogram, 2, 2);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedQueueingTime) {
  base::HistogramTester histogram_tester;
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(10));
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(40));
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(20));
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(10));
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Dispatch all queued events.
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(70));
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  histogram_tester.ExpectUniqueSample(kContinuousHeadQueueingTimeHistogram, 140,
                                      1);
  histogram_tester.ExpectUniqueSample(kContinuousTailQueueingTimeHistogram, 80,
                                      1);
  histogram_tester.ExpectBucketCount(kNonContinuousQueueingTimeHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(kNonContinuousQueueingTimeHistogram, 70,
                                     1);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedCoalesceScrollAndPinch) {
  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  // GSUs and GPUs in one sequence should be coalesced into 1 GSU and 1 GPU.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -7);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 2.0f, 13, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -6);
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 0.2f, 2, 20);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 0.25f, 3, 30);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Only the first GSB was dispatched.
  EXPECT_EQ(11ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  EXPECT_EQ(WebInputEvent::kGesturePinchBegin,
            event_queue()[0]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            event_queue()[1]->event().GetType());
  EXPECT_EQ(
      -35,
      ToWebGestureEvent(event_queue()[1]->event()).data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            event_queue()[2]->event().GetType());
  EXPECT_EQ(
      2.0f,
      ToWebGestureEvent(event_queue()[2]->event()).data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            event_queue()[3]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            event_queue()[4]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin,
            event_queue()[5]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGesturePinchBegin,
            event_queue()[6]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            event_queue()[7]->event().GetType());
  EXPECT_EQ(
      -85,
      ToWebGestureEvent(event_queue()[7]->event()).data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            event_queue()[8]->event().GetType());
  EXPECT_EQ(
      0.5f,
      ToWebGestureEvent(event_queue()[8]->event()).data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            event_queue()[9]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            event_queue()[10]->event().GetType());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, OriginalEventsTracing) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .Times(::testing::AtLeast(1));

  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(testing::_, testing::_));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(testing::_, testing::_));

  trace_analyzer::Start("*");
  // Simulate scroll.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Simulate scroll and pinch.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 2.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Dispatch all events.
  input_handler_proxy_->DeliverInputForBeginFrame();

  // Retrieve tracing data.
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector begin_events;
  trace_analyzer::Query begin_query = trace_analyzer::Query::EventPhaseIs(
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  analyzer->FindEvents(begin_query, &begin_events);

  trace_analyzer::TraceEventVector end_events;
  trace_analyzer::Query end_query =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END);
  analyzer->FindEvents(end_query, &end_events);

  EXPECT_EQ(7ul, begin_events.size());
  EXPECT_EQ(7ul, end_events.size());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            end_events[0]->GetKnownArgAsInt("type"));
  EXPECT_EQ(3, end_events[0]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            end_events[1]->GetKnownArgAsInt("type"));

  EXPECT_EQ(WebInputEvent::kGestureScrollBegin,
            end_events[2]->GetKnownArgAsInt("type"));
  EXPECT_EQ(WebInputEvent::kGesturePinchBegin,
            end_events[3]->GetKnownArgAsInt("type"));
  // Original scroll and pinch updates will be stored in the coalesced
  // PinchUpdate of the <ScrollUpdate, PinchUpdate> pair.
  // The ScrollUpdate of the pair doesn't carry original events and won't be
  // traced.
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            end_events[4]->GetKnownArgAsInt("type"));
  EXPECT_EQ(4, end_events[4]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            end_events[5]->GetKnownArgAsInt("type"));
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            end_events[6]->GetKnownArgAsInt("type"));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, TouchpadGestureScrollEndFlushQueue) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .Times(::testing::AtLeast(1));

  // Simulate scroll.
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollBegin,
                                     blink::kWebGestureDeviceTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollUpdate,
                                     blink::kWebGestureDeviceTouchpad, -20);

  // Both GSB and the first GSU will be dispatched immediately since the first
  // GSU has blocking wheel event source.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // The rest of the GSU events will get queued since they have non-blocking
  // wheel event source.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollUpdate,
                                     blink::kWebGestureDeviceTouchpad, -20);
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // Touchpad GSE will flush the queue.
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollEnd,
                                     blink::kWebGestureDeviceTouchpad);

  EXPECT_EQ(0ul, event_queue().size());
  // GSB, GSU(with blocking wheel source), GSU(with non-blocking wheel
  // source), and GSE are the sent events.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());

  EXPECT_FALSE(
      input_handler_proxy_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_F(InputHandlerProxyEventQueueTest, CoalescedLatencyInfo) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);
  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  EXPECT_EQ(5ul, latency_info_recorder_.size());
  EXPECT_EQ(false, latency_info_recorder_[1].coalesced());
  // Coalesced events should have latency set to coalesced.
  EXPECT_EQ(true, latency_info_recorder_[2].coalesced());
  EXPECT_EQ(true, latency_info_recorder_[3].coalesced());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, CoalescedEventSwitchToMainThread) {
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  scroll_result_did_not_scroll_.did_scroll = false;

  // scroll begin on main thread
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(3ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            event_disposition_recorder_.back());
  // GSU should not be coalesced
  EXPECT_EQ(false, latency_info_recorder_[1].coalesced());
  EXPECT_EQ(false, latency_info_recorder_[2].coalesced());

  // pinch start, handle scroll and pinch on compositor.
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(testing::_, testing::_));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(testing::_, testing::_));

  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  EXPECT_EQ(2ul, event_queue().size());
  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(7ul, event_disposition_recorder_.size());
  EXPECT_EQ(false, latency_info_recorder_[4].coalesced());
  // Coalesced events should have latency set to coalesced.
  EXPECT_EQ(true, latency_info_recorder_[5].coalesced());
  EXPECT_EQ(true, latency_info_recorder_[6].coalesced());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_.back());

  // Pinch end, handle scroll on main thread.
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(10ul, event_disposition_recorder_.size());
  EXPECT_EQ(10ul, latency_info_recorder_.size());
  // Latency should not be set to coalesced when send to main thread
  EXPECT_EQ(false, latency_info_recorder_[8].coalesced());
  EXPECT_EQ(false, latency_info_recorder_[9].coalesced());
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            event_disposition_recorder_.back());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, ScrollPredictorTest) {
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));

  // No prediction when start with a GSB
  ui::InputPredictor::InputData result;
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_FALSE(GestureScrollEventPredictionAvailable(&result));

  // Test predictor returns last GSU delta.
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -15);
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_TRUE(GestureScrollEventPredictionAvailable(&result));
  EXPECT_EQ(-35, result.pos.y());

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Predictor has been reset after a new GSB.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_FALSE(GestureScrollEventPredictionAvailable(&result));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

class InputHandlerProxyMainThreadScrollingReasonTest
    : public InputHandlerProxyTest {
 public:
  enum TestEventType {
    Touch,
    MouseWheel,
  };

  InputHandlerProxyMainThreadScrollingReasonTest() : InputHandlerProxyTest() {}
  ~InputHandlerProxyMainThreadScrollingReasonTest() { input_handler_.reset(); }

  void SetupEvents(TestEventType type) {
    touch_start_ =
        WebTouchEvent(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
    touch_end_ =
        WebTouchEvent(WebInputEvent::kTouchEnd, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
    wheel_event_ = WebMouseWheelEvent(
        WebInputEvent::kMouseWheel, WebInputEvent::kControlKey,
        WebInputEvent::GetStaticTimeStampForTests());
    gesture_scroll_begin_ = WebGestureEvent(
        WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        type == TestEventType::MouseWheel
            ? blink::kWebGestureDeviceTouchpad
            : blink::kWebGestureDeviceTouchscreen);
    gesture_scroll_end_ = WebGestureEvent(
        WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        type == TestEventType::MouseWheel
            ? blink::kWebGestureDeviceTouchpad
            : blink::kWebGestureDeviceTouchscreen);
    touch_start_.touches_length = 1;
    touch_start_.touch_start_or_first_touch_move = true;
    touch_start_.touches[0] =
        CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);

    touch_end_.touches_length = 1;
  }

  base::HistogramBase::Sample GetBucketSample(uint32_t reason) {
    if (reason == cc::MainThreadScrollingReason::kNotScrollingOnMain)
      return 0;

    uint32_t bucket = 1;
    while ((reason = reason >> 1))
      bucket++;
    return bucket;
  }

 protected:
  WebTouchEvent touch_start_;
  WebTouchEvent touch_end_;
  WebMouseWheelEvent wheel_event_;
  WebGestureEvent gesture_scroll_begin_;
  WebGestureEvent gesture_scroll_end_;
};

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollNotScrollOnMain) {
  // Touch start with passive event listener.
  SetupEvents(TestEventType::Touch);

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(cc::MainThreadScrollingReason::kNotScrollingOnMain),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollTouchEventHandlerRegion) {
  // The touch event hits a touch event handler and should block on main thread.
  // Since ScrollBegin allows the gesture to scroll on impl. We collect
  // TouchEventHandler reason but not HandlingScrollFromMainThread.
  SetupEvents(TestEventType::Touch);

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Renderer4.MainThreadGestureScrollReason"),
              testing::ElementsAre(base::Bucket(
                  GetBucketSample(
                      cc::MainThreadScrollingReason::kTouchEventHandlerRegion),
                  1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollTouchEventHandlerRegionAndHandlingScrollFromMainThread) {
  // The touch event hits a touch event handler and should block on main thread.
  // Since ScrollBegin doesn't allow the gesture to scroll on impl. We report
  // TouchEventHandler reason as well as HandlingScrollFromMainThread. Since we
  // do not collect HandlingScrollFromMainThread when there are other reasons
  // present, TouchEventHandler is the only reason being collected in the
  // histogram.
  SetupEvents(TestEventType::Touch);

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Renderer4.MainThreadGestureScrollReason"),
              testing::ElementsAre(base::Bucket(
                  GetBucketSample(
                      cc::MainThreadScrollingReason::kTouchEventHandlerRegion),
                  1)));

  // Handle touch end event so that input handler proxy is out of the state of
  // DID_NOT_HANDLE.
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));

  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollHandlingScrollFromMainThread) {
  // Gesture scrolling on main thread. We only record
  // HandlingScrollFromMainThread when it's the only available reason.
  SetupEvents(TestEventType::Touch);
  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, GetEventListenerProperties(testing::_))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kPassive));

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest, WheelScrollHistogram) {
  // Firstly check if input handler can correctly record main thread scrolling
  // reasons.
  input_handler_->RecordMainThreadScrollingReasonsForTest(
      blink::kWebGestureDeviceTouchpad,
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects |
          cc::MainThreadScrollingReason::kThreadedScrollingDisabled |
          cc::MainThreadScrollingReason::kPageOverlay |
          cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::
                                  kHasBackgroundAttachmentFixedObjects),
              1),
          base::Bucket(
              GetBucketSample(
                  cc::MainThreadScrollingReason::kThreadedScrollingDisabled),
              1),
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::kPageOverlay),
              1)));

  // We only want to record "Handling scroll from main thread" reason if it's
  // the only reason. If it's not the only reason, the "real" reason for
  // scrolling on main is something else, and we only want to pay attention to
  // that reason. So we should only include this reason in the histogram when
  // its on its own.
  input_handler_->RecordMainThreadScrollingReasonsForTest(
      blink::kWebGestureDeviceTouchpad,
      cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::
                                  kHasBackgroundAttachmentFixedObjects),
              1),
          base::Bucket(
              GetBucketSample(
                  cc::MainThreadScrollingReason::kThreadedScrollingDisabled),
              1),
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::kPageOverlay), 1),
          base::Bucket(
              GetBucketSample(
                  cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
              1)));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollNotScrollingOnMain) {
  // Even if a scroller is composited, we still need to record its main thread
  // scrolling reason if it is blocked on a main thread event handler.
  SetupEvents(TestEventType::MouseWheel);

  // We can scroll on impl for an wheel event with passive event listener.
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(cc::MainThreadScrollingReason::kNotScrollingOnMain),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollWheelEventHandlerRegion) {
  // Wheel event with blocking event listener. If there is a wheel event handler
  // at the point, we do not need to call GetEventListenerProperties since it
  // indicates kBlocking.
  SetupEvents(TestEventType::MouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kWheelEventHandlerRegion),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollWheelEventHandlerRegionAndHandlingScrollFromMainThread) {
  // Wheel scrolling on main thread. Because we also block scrolling with wheel
  // event handler, we should record that reason as well.
  SetupEvents(TestEventType::MouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kWheelEventHandlerRegion),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollHandlingScrollFromMainThread) {
  // Gesture scrolling on main thread. We only record
  // HandlingScrollFromMainThread when it's the only available reason.
  SetupEvents(TestEventType::MouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end_));
}

INSTANTIATE_TEST_CASE_P(AnimateInput,
                        InputHandlerProxyTest,
                        testing::ValuesIn(test_types));

INSTANTIATE_TEST_CASE_P(AnimateInput,
                        InputHandlerProxyMainThreadScrollingReasonTest,
                        testing::ValuesIn(test_types));
}  // namespace test
}  // namespace ui
