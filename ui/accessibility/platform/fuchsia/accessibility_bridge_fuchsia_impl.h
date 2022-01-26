// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_IMPL_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_IMPL_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider.h"
#include "ui/aura/window.h"

namespace ui {

class AX_EXPORT AccessibilityBridgeFuchsiaImpl final
    : public ui::AccessibilityBridgeFuchsia,
      public ui::AXFuchsiaSemanticProvider::Delegate {
 public:
  // Constructor args:
  //
  // |root_window|: Refers to the root aura::Window for which this accessibility
  // bridge instance is responsible. The pointer is NOT owned by this class.
  // The accessibility bridge instance's lifespan is not explicitly tied to the
  // window's, but they are expected to be deleted together.
  //
  // |view_ref|: The fuchsia ViewRef for the fuchsia view that corresponds to
  // |root_window|.
  //
  // |get_pixel_scale|: Callback used to retrieve the pixel scale for this
  // device. We use a callback here, because the correct value may not be
  // available at the time of construction.
  //
  // |on_semantics_enabled|: Callback invoked when fuchsia's accessibility
  // platform component requests to enable/disable semantics (e.g. when the
  // screen reader is toggled on/off). The boolean argument to the callback
  // indicates whether semantics should be enabled or disabled (true => enable
  // semantics, false => disable semantics).
  //
  // |on_connection_closed|: Callback invoked when the FIDL connection to the
  // fuchsia accessibility platform component closes. The return value indicates
  // whether an attempt to reconnect should be made (true => reconnect, false =>
  // do not reconnect).
  AccessibilityBridgeFuchsiaImpl(
      aura::Window* root_window,
      fuchsia::ui::views::ViewRef view_ref,
      base::RepeatingCallback<float()> get_pixel_scale,
      base::RepeatingCallback<void(bool)> on_semantics_enabled,
      base::RepeatingCallback<bool()> on_connection_closed);
  ~AccessibilityBridgeFuchsiaImpl() override;

  // AccessibilityBridgeFuchsia overrides.
  void UpdateNode(fuchsia::accessibility::semantics::Node node) override;
  void DeleteNode(uint32_t node_id) override;
  void OnAccessibilityHitTestResult(int hit_test_request_id,
                                    absl::optional<uint32_t> result) override;
  float GetDeviceScaleFactor() override;
  void SetRootID(uint32_t root_node_id) override;

  // SemanticProvider::Delegate overrides.
  bool OnSemanticsManagerConnectionClosed() override;
  bool OnAccessibilityAction(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action) override;
  void OnHitTest(
      fuchsia::math::PointF point,
      fuchsia::accessibility::semantics::SemanticListener::HitTestCallback
          callback) override;
  void OnSemanticsEnabled(bool enabled) override;

  // Test-only method to set |semantic_provider_|.
  void set_semantic_provider_for_test(
      std::unique_ptr<AXFuchsiaSemanticProvider> semantic_provider);

 private:
  // Returns kFuchsiaRootNodeId if node_id == *root_node_id_. Otherwise, returns
  // node_id.
  uint32_t MaybeToFuchsiaRootID(uint32_t node_id);

  // Root window for the fuchsia view for which this accessibility bridge
  // instance is responsible.
  aura::Window* root_window_;

  // Manages connections with the fuchsia semantics APIs.
  std::unique_ptr<ui::AXFuchsiaSemanticProvider> semantic_provider_;

  // Fuchsia semantic trees require that the root node ID == 0. The
  // AXUniqueId of the chrome node corresponding to the fuchsia root will NOT be
  // 0, so we need to store it here in order to map between the two.
  absl::optional<uint32_t> root_node_id_;

  // Holds callbacks for hit tests that have not yet completed, keyed by a
  // request ID that this class generates.
  base::flat_map<
      int /* request_id */,
      fuchsia::accessibility::semantics::SemanticListener::HitTestCallback>
      pending_hit_test_callbacks_;

  // Next hit test request ID to use.
  int next_hittest_request_id_ = 1;

  // Callback invoked whenever the semantics enabled state changes.
  base::RepeatingCallback<void(bool)> on_semantics_enabled_;

  // Callback invoked whenever the semantics manager connection is closed.
  base::RepeatingCallback<bool()> on_connection_closed_;
};

}  // namespace ui
#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_ACCESSIBILITY_BRIDGE_FUCHSIA_IMPL_H_
