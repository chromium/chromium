// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_OVERSCROLL_REFRESH_H_
#define UI_ANDROID_OVERSCROLL_REFRESH_H_

#include <optional>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "ui/android/ui_android_export.h"
#include "ui/events/back_gesture_event.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

// Java counterparts will be generated for these enums.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui
enum class OverscrollAction {
  kNone = 0,
  kPullToRefresh = 1,
  kHistoryNavigation = 2,
  kPullFromBottomEdge = 3
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui
// kDisallowActivation: Prevents activation.
// kAllowActivation: Allows activation, but the final decision depends on
//                     Java-side logic (e.g. drag distance threshold).
// kForceActivation: Forces the activation.
// kReset: This is for NavigationHandler.java to reset the state
enum class OverscrollActivationStatus {
  kDisallowActivation = 0,
  kAllowActivation = 1,
  kForceActivation = 2,
  kReset = 3
};

namespace cc {
struct OverscrollBehavior;
}

namespace gfx {
class PointF;
}

namespace ui {

class OverscrollRefreshHandler;

// Simple pull-to-refresh styled effect. Listens to scroll events, conditionally
// activating when:
//   1) The scroll begins when the page's root layer 1) has no vertical scroll
//      offset and 2) lacks the overflow-y:hidden property.
//   2) The page doesn't consume the initial scroll events.
//   3) The initial scroll direction is upward.
// The actuall pull response, animation and action are delegated to the
// provided refresh handler.
class UI_ANDROID_EXPORT OverscrollRefresh {
 public:
  // The default distance in dp from a side of the device to start a navigation
  // from.
  // LINT.IfChange
  static constexpr int kDefaultNavigationEdgeWidth = 24;
  // LINT.ThenChange(//ui/android/java/src/org/chromium/ui/OverscrollRefreshHandler.java:kDefaultNavigationEdgeWidth)

  OverscrollRefresh(OverscrollRefreshHandler* handler, float edge_width);

  OverscrollRefresh(const OverscrollRefresh&) = delete;
  OverscrollRefresh& operator=(const OverscrollRefresh&) = delete;

  virtual ~OverscrollRefresh();

  // Scroll event stream listening methods.
  void OnScrollBegin(const gfx::PointF& pos);
  // Returns whether the refresh was activated.
  void OnScrollEnd(const gfx::Vector2dF& velocity);

  // Scroll ack listener. The effect will only be activated if |can_navigate|
  // is true which happens when the scroll update is not consumed and the
  // overscroll_behavior on y axis is 'auto'.
  // This method is made virtual for mocking.
  virtual void OnOverscrolled(const cc::OverscrollBehavior& behavior,
                              gfx::Vector2dF accumulated_overscroll,
                              blink::WebGestureDevice source_device);

  // Disables scroll consumption if the activation shouldn't have happened.
  void MaybeDisableScrollConsumption(const gfx::Vector2dF& scroll_delta);

  // Returns true if the effect has consumed the |scroll_delta|.
  bool WillHandleScrollUpdate(const gfx::Vector2dF& scroll_delta);

  // Release the effect (if active), preventing any associated refresh action.
  void ReleaseWithoutActivation();

  // Notify the effect of the latest scroll offset and overflow properties.
  // The effect will be disabled when the offset is non-zero or overflow is
  // hidden. Note: All dimensions are in device pixels. `view_width` is the
  // width of the embedding native view and is used for edge-swipe gating;
  // `scrollable_viewport_height` reflects the page's scrollable viewport.
  void OnFrameUpdated(float view_width,
                      float scrollable_viewport_height,
                      const gfx::PointF& content_scroll_offset,
                      const gfx::SizeF& content_size,
                      bool root_overflow_y_hidden);

  // Reset the effect to its inactive state, immediately detaching and
  // disabling any active effects.
  // This method is made virtual for mocking.
  virtual void Reset();

  // Returns true if the refresh effect is either being manipulated or animated.
  // This method is made virtual for mocking.
  virtual bool IsActive() const;

  // Returns true if the effect is waiting for an unconsumed scroll to start.
  // This method is made virtual for mocking.
  virtual bool IsAwaitingScrollUpdateAck() const;

  void SetTouchpadOverscrollHistoryNavigation(bool enabled);
  void SetIsGestureNavigationMode(bool is_gesture_navigation_mode);

 protected:
  // This constructor is for mocking only.
  OverscrollRefresh();

 private:
  void Release(OverscrollActivationStatus status);

  // Returns velocity in the active action direction.
  float GetVelocityInActiveActionDirection(const gfx::Vector2dF& velocity);

  // Returns the activation status based on velocity in the active action
  // direction.
  OverscrollActivationStatus GetActivationStatus(
      const gfx::Vector2dF& velocity);

  bool scrolled_to_top_;
  bool scrolled_to_bottom_;

  // True if the content y offset was zero before scroll began. Overscroll
  // should not be triggered for the scroll that started from non-zero offset.
  bool top_at_scroll_start_;
  // True if the scroll is from the bottom of the screen. Overscroll
  // should not be triggered for the scroll that started from non-zero offset.
  bool bottom_at_scroll_start_;
  bool overflow_y_hidden_;

  struct Disabled {};
  struct AwaitingScrollUpdateAck {};
  struct ActiveAction {
    OverscrollAction action = OverscrollAction::kNone;
    std::optional<BackGestureEventSwipeEdge> edge;
    std::optional<blink::WebGestureDevice> device;
  };

  float viewport_width_ = 0.f;
  float scroll_begin_x_ = 0.f;
  float scroll_begin_y_ = 0.f;
  const float edge_width_;  // in px
  const raw_ptr<OverscrollRefreshHandler, DanglingUntriaged> handler_;
  bool touchpad_overscroll_history_navigation_enabled_ = false;
  bool is_gesture_navigation_mode_ = false;

  class ScrollState {
   public:
    void Reset() { state_ = Disabled{}; }
    void StartAwaitingAck() { state_ = AwaitingScrollUpdateAck{}; }
    void SetEnabled(const ActiveAction& action) {
      CHECK(!IsEnabled());
      state_ = action;
    }

    bool IsDisabled() const { return std::holds_alternative<Disabled>(state_); }
    bool IsAwaitingAck() const {
      return std::holds_alternative<AwaitingScrollUpdateAck>(state_);
    }
    bool IsEnabled() const {
      return std::holds_alternative<ActiveAction>(state_);
    }

    const ActiveAction& GetAction() const {
      CHECK(IsEnabled());
      return std::get<ActiveAction>(state_);
    }

   private:
    std::variant<Disabled, AwaitingScrollUpdateAck, ActiveAction> state_ =
        Disabled{};
  } scroll_state_;
};

}  // namespace ui

#endif  // UI_ANDROID_OVERSCROLL_REFRESH_H_
