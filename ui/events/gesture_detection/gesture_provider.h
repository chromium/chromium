// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_

#include <memory>

#include "ui/display/display.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/gesture_touch_uma_histogram.h"
#include "ui/events/gesture_detection/scale_gesture_detector.h"
#include "ui/events/gesture_detection/snap_scroll_controller.h"

namespace ui {

class GESTURE_DETECTION_EXPORT GestureProviderClient {
 public:
  virtual ~GestureProviderClient() {}
  virtual void OnGestureEvent(const GestureEventData& gesture) = 0;

  // Supporting double tap events requires adding some extra delay before
  // sending single-tap events in order to determine whether its a potential
  // double tap or not. This delay may be undesirable in many UI components and
  // should be avoided if not needed.
  // Returns true if the client wants to receive double tap gesture events.
  virtual bool RequiresDoubleTapGestureEvents() const;
};

// Given a stream of |MotionEvent|'s, provides gesture detection and gesture
// event dispatch.
class GESTURE_DETECTION_EXPORT GestureProvider {
 public:
  struct GESTURE_DETECTION_EXPORT Config {
    Config();
    Config(const Config& other);
    ~Config();
    display::Display display;
    GestureDetector::Config gesture_detector_config;
    ScaleGestureDetector::Config scale_gesture_detector_config;

    // Whether double-tap detection is supported by the platform. If disabled,
    // there will be no delay before tap events. Defaults to true.
    bool double_tap_support_for_platform_enabled;

    // If |gesture_begin_end_types_enabled| is true, fire an
    // EventType::kGestureBegin event for every added touch point, and an
    // EventType::kGestureEnd event for every removed touch point. This requires
    // one ACTION_CANCEL event to be sent per touch point, which only occurs on
    // Aura. Defaults to false.
    bool gesture_begin_end_types_enabled;

    // The min and max size (both length and width, in dips) of the generated
    // bounding box for all gesture types. This is useful for touch streams
    // that may report zero or unreasonably small or large touch sizes.
    // Note that these bounds are only applied for touch or unknown tool types;
    // mouse and stylus-derived gestures will not be affected.
    // Both values default to 0 (disabled).
    float min_gesture_bounds_length;
    float max_gesture_bounds_length;
  };

  GestureProvider(const Config& config, GestureProviderClient* client);
  ~GestureProvider();

  // Handle the incoming MotionEvent, returning false if the event could not
  // be handled.
  bool OnTouchEvent(const MotionEvent& event);

  // Reset any active gesture detection, including detection of timeout-based
  // events (e.g., double-tap or delayed tap) for which the pointer has already
  // been released.
  void ResetDetection();

  // Update whether multi-touch pinch zoom is supported by the platform.
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  // Update whether double-tap gestures are supported by the platform.
  void SetDoubleTapSupportForPlatformEnabled(bool enabled);

  // Update whether double-tap gesture detection should be suppressed, e.g.,
  // if the page scale is fixed or the page has a mobile viewport. This disables
  // the tap delay, allowing rapid and responsive single-tap gestures.
  void SetDoubleTapSupportForPageEnabled(bool enabled);

  // Whether a scroll gesture is in-progress.
  bool IsScrollInProgress() const;

  // Whether a pinch gesture is in-progress (i.e. a pinch update has been
  // forwarded and detection is still active).
  bool IsPinchInProgress() const;

  // Whether a double-tap gesture is in-progress (either double-tap or
  // double-tap drag zoom).
  bool IsDoubleTapInProgress() const;

  // Synthesizes and propagates gesture end events.
  void SendSynthesizedEndEvents();

  // May be NULL if there is no currently active touch sequence.
  const ui::MotionEvent* current_down_event() const {
    return current_down_event_.get();
  }

 private:
  bool CanHandle(const MotionEvent& event) const;
  void OnTouchEventHandlingBegin(const MotionEvent& event);
  void OnTouchEventHandlingEnd(const MotionEvent& event);
  void UpdateDoubleTapDetectionSupport();

  class GestureListenerImpl;
  std::unique_ptr<GestureListenerImpl> gesture_listener_;

  std::unique_ptr<MotionEvent> current_down_event_;

  // Logs information on touch and gesture events.
  GestureTouchUMAHistogram uma_histogram_;

  // Whether double-tap gesture detection is currently supported.
  bool double_tap_support_for_page_;
  bool double_tap_support_for_platform_;

  const bool gesture_begin_end_types_enabled_;
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_
