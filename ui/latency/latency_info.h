// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_LATENCY_INFO_H_
#define UI_LATENCY_LATENCY_INFO_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"

#if !BUILDFLAG(IS_IOS)
#include "ipc/ipc_param_traits.h"  // nogncheck
#include "mojo/public/cpp/bindings/struct_traits.h"  // nogncheck
#endif

namespace ui {

#if !BUILDFLAG(IS_IOS)
namespace mojom {
class LatencyInfoDataView;
}
#endif

// When adding new components, or new metrics based on LatencyInfo,
// please update latency_info.dot.
//
// When adding new components, please update
// //third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.proto
// so both this and the internal versions can be kept up to date. Or reach out
// to tracing@chromium.org so we can assist.
enum LatencyComponentType {
  // ---------------------------BEGIN COMPONENT-------------------------------
  // BEGIN COMPONENT is when we show the latency begin in chrome://tracing.
  // Timestamp when the input event is sent from RenderWidgetHost to renderer.
  INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
  // ---------------------------NORMAL COMPONENT-------------------------------
  // The original timestamp of the touch event which converts to scroll update.
  INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
  // The original timestamp of the touch event which converts to the *first*
  // scroll update in a scroll gesture sequence.
  INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
  // Original timestamp for input event (e.g. timestamp from kernel).
  INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
  // Timestamp when the UI event is created.
  INPUT_EVENT_LATENCY_UI_COMPONENT,
  // Timestamp when the event is dispatched on the main thread of the renderer.
  INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
  // This is special component indicating there is rendering scheduled for
  // the event associated with this LatencyInfo on main thread.
  INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT,
  // This is special component indicating there is rendering scheduled for
  // the event associated with this LatencyInfo on impl thread.
  INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT,
  // Timestamp when the frame is swapped in renderer.
  INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
  // Timestamp of when the display compositor receives a compositor frame from
  // the renderer.
  // Display compositor can be either in the browser process or in Mus.
  DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT,
  // Timestamp of when the gpu service began swap buffers, unlike
  // INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT which measures after.
  INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT,
  // Timestamp when the frame is swapped (i.e. when the rendering caused by
  // input event actually takes effect).
  INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,

  LATENCY_COMPONENT_TYPE_LAST = INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
};

enum class SourceEventType {
  UNKNOWN,
  WHEEL,
  MOUSE,
  TOUCH,
  INERTIAL,
  KEY_PRESS,
  // TODO(crbug.com/868056) Touchpad scrolling latency report as WHEEL.
  TOUCHPAD,
  SCROLLBAR,
  OTHER,
  LAST = OTHER,
};

class LatencyInfo {
 public:
  // Map a Latency Component (with a component-specific int64_t id) to a
  // timestamp.
  using LatencyMap = base::flat_map<LatencyComponentType, base::TimeTicks>;

  LatencyInfo();
  LatencyInfo(const LatencyInfo& other);
  LatencyInfo(LatencyInfo&& other);
  LatencyInfo(SourceEventType type);
  ~LatencyInfo();

  // For test only.
  LatencyInfo(int64_t trace_id, bool terminated);

  LatencyInfo& operator=(const LatencyInfo& other);

  // Returns true if the vector |latency_info| is valid. Returns false
  // if it is not valid and log the |referring_msg|.
  // This function is mainly used to check the latency_info vector that
  // is passed between processes using IPC message has reasonable size
  // so that we are confident the IPC message is not corrupted/compromised.
  // This check will go away once the IPC system has better built-in scheme
  // for corruption/compromise detection.
  static bool Verify(const std::vector<LatencyInfo>& latency_info,
                     const char* referring_msg);

  // Adds trace flow events only to LatencyInfos that are being traced.
  static void TraceIntermediateFlowEvents(
      const std::vector<LatencyInfo>& latency_info,
      perfetto::protos::pbzero::ChromeLatencyInfo::Step step);

  // Add timestamps for components that are in |other| but not in |this|.
  void AddNewLatencyFrom(const LatencyInfo& other);

  // Modifies the current sequence number for a component, and adds a new
  // sequence number with the current timestamp.
  void AddLatencyNumber(LatencyComponentType component);

  // Similar to |AddLatencyNumber|, and also appends |trace_name_str| to
  // the trace event's name.
  // This function should only be called when adding a BEGIN component.
  void AddLatencyNumberWithTraceName(LatencyComponentType component,
                                     const char* trace_name_str);

  // Modifies the current sequence number and adds a certain number of events
  // for a specific component.
  void AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                     base::TimeTicks time);

  // Returns true if a component with |type| is found in the latency component.
  // The first such component (when iterating over latency_components_) is
  // stored to |output| if |output| is not NULL. Returns false if no such
  // component is found.
  bool FindLatency(LatencyComponentType type, base::TimeTicks* output) const;

  void Terminate();

  const LatencyMap& latency_components() const { return latency_components_; }

  const SourceEventType& source_event_type() const {
    return source_event_type_;
  }
  void set_source_event_type(SourceEventType type) {
    source_event_type_ = type;
  }

  bool began() const { return began_; }
  bool terminated() const { return terminated_; }
  void set_coalesced() { coalesced_ = true; }
  bool coalesced() const { return coalesced_; }
  int64_t trace_id() const { return trace_id_; }
  void set_trace_id(int64_t trace_id) { trace_id_ = trace_id; }
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }
  void set_ukm_source_id(ukm::SourceId id) { ukm_source_id_ = id; }
  int64_t gesture_scroll_id() const { return gesture_scroll_id_; }
  void set_gesture_scroll_id(int64_t id) { gesture_scroll_id_ = id; }
  int64_t touch_trace_id() const { return touch_trace_id_; }
  void set_touch_trace_id(int64_t id) { touch_trace_id_ = id; }

 private:
  void AddLatencyNumberWithTimestampImpl(LatencyComponentType component,
                                         base::TimeTicks time,
                                         const char* trace_name_str);

  LatencyMap latency_components_;

  // The unique id for matching the ASYNC_BEGIN/END trace event.
  int64_t trace_id_ = -1;
  // UKM Source id to be used for recording UKM metrics associated with this
  // event.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;
  // Whether this event has been coalesced into another event.
  bool coalesced_ = false;
  // Whether a begin component has been added.
  bool began_ = false;
  // Whether a terminal component has been added.
  bool terminated_ = false;
  // Stores the type of the first source event.
  SourceEventType source_event_type_ = SourceEventType::UNKNOWN;

  // The unique id for denoting a scroll gesture. This is only set for
  // GestureScrollBegin, GestureScrollUpdate, and GestureScrollEnd events, and
  // allows easy grouping of these global async events into a single logical
  // scroll in the sql interface of TBMv3 (Trace Based Metrics v3). As a current
  // implementation detail this unique id comes from the |trace_id| of the
  // associated GestureScrollBegin (-1 if there was none or it wasn't valid).
  int64_t gesture_scroll_id_ = 0;
  // The unique id for denoting a touch, tracking from TouchStart through
  // TouchMoves to TouchEnd. Used for TBMv3 metrics as in the same way as
  // gesture_scroll_id_.
  int64_t touch_trace_id_ = 0;

#if !BUILDFLAG(IS_IOS)
  friend struct IPC::ParamTraits<ui::LatencyInfo>;
  friend struct mojo::StructTraits<ui::mojom::LatencyInfoDataView,
                                   ui::LatencyInfo>;
#endif
};

// This is declared here for use in gtest-based unit tests, but is defined in
// //ui/latency:test_support target.
// Without this the default PrintTo template in gtest tries to pass LatencyInfo
// by value, which leads to an alignment compile error on Windows.
void PrintTo(const LatencyInfo& latency, ::std::ostream* os);

}  // namespace ui

#endif  // UI_LATENCY_LATENCY_INFO_H_
