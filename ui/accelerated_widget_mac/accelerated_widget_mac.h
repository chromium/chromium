// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_

#include "base/memory/raw_ptr.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// A class through which an AcceleratedWidget may be bound to draw the contents
// of an NSView. An AcceleratedWidget may be bound to multiple different views
// throughout its lifetime (one at a time, though).
class AcceleratedWidgetMacNSView {
 public:
  // Called when the AcceleratedWidgetMac's CALayerFrameSink interface's
  // UpdateCALayerTree method is called. This is used to update background
  // colors and to suppressing drawing of blank windows until content is
  // available.
  virtual void AcceleratedWidgetCALayerParamsUpdated() = 0;
};

// AcceleratedWidgetMac owns a tree of CALayers. The widget may be passed
// to a ui::Compositor, which will, through its output surface, call the
// CALayerFrameSink interface. The CALayers may be installed in an NSView
// by setting the AcceleratedWidgetMacNSView for the helper.
class ACCELERATED_WIDGET_MAC_EXPORT AcceleratedWidgetMac
    : public CALayerFrameSink {
 public:
  AcceleratedWidgetMac();

  AcceleratedWidgetMac(const AcceleratedWidgetMac&) = delete;
  AcceleratedWidgetMac& operator=(const AcceleratedWidgetMac&) = delete;

  ~AcceleratedWidgetMac() override;

  gfx::AcceleratedWidget accelerated_widget() { return native_widget_; }

  void SetNSView(AcceleratedWidgetMacNSView* view);
  void ResetNSView();

  // Returns the CALayer parameters most recently sent to the CALayerFrameSink
  // interface, or nullptr if none are available.
  const gfx::CALayerParams* GetCALayerParams() const;

  // Return true if the last frame swapped has a size in DIP of |dip_size|.
  bool HasFrameOfSize(const gfx::Size& dip_size) const;

  // If |suspended| is true, then ignore all new frames that come in until
  // a call is made with |suspended| as false.
  void SetSuspended(bool suspended);

 private:
  // For CALayerFrameSink::FromAcceleratedWidget to access Get.
  friend class CALayerFrameSink;

  // Translate from a gfx::AcceleratedWidget handle to the underlying
  // AcceleratedWidgetMac (used by other gfx::AcceleratedWidget translation
  // functions).
  static AcceleratedWidgetMac* Get(gfx::AcceleratedWidget widget);

  // gfx::CALayerFrameSink implementation:
  void UpdateCALayerTree(const gfx::CALayerParams& ca_layer_params) override;

  // The AcceleratedWidgetMacNSView that is using this as its internals.
  raw_ptr<AcceleratedWidgetMacNSView> view_ = nullptr;

  // A phony NSView handle used to identify this.
  gfx::AcceleratedWidget native_widget_ = gfx::kNullAcceleratedWidget;

  // If the output surface is suspended, then updates to the layer parameters
  // will be ignored.
  bool is_suspended_ = false;

  // The last CALayer parameter update from the CALayerFrameSink interface.
  bool last_ca_layer_params_valid_ = false;
  gfx::CALayerParams last_ca_layer_params_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_
