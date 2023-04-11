// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_H_

#include <fidl/fuchsia.accessibility.semantics/cpp/fidl.h>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace ui {

// Manages the connection with the Fuchsia Semantics API.
//
// Semantic nodes can be added or deleted. When a batch of nodes would leave the
// Fuchsia semantic tree in a valid state, they are committed. Please see
// |fuchsia.accessibility.semantics| API for more documentation on valid
// semantic trees.
class COMPONENT_EXPORT(AX_PLATFORM) AXFuchsiaSemanticProvider {
 public:
  // Fuchsia root node id.
  static constexpr uint32_t kFuchsiaRootNodeId = 0u;

  // A delegate that can be registered by clients of this library to be notified
  // about Semantic changes.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    using HitTestCallback = base::OnceCallback<void(
        const fidl::Response<
            fuchsia_accessibility_semantics::SemanticListener::HitTest>&)>;
    // Called when the FIDL channel to the Semantics Manager is closed. If this
    // callback returns true, an attempt to reconnect will be made.
    virtual bool OnSemanticsManagerConnectionClosed(zx_status_t status) = 0;

    // Processes an incoming accessibility action from Fuchsia. It
    // receives the Fuchsia node ID and the action requested. If this
    // method returns true, this means that the action will be handled.
    virtual bool OnAccessibilityAction(
        uint32_t node_id,
        fuchsia_accessibility_semantics::Action action) = 0;

    // Processes an incoming hit test request from Fuchsia. It
    // receives a point in Scenic View pixel coordinates and a callback to
    // return the result when the hit test is done. Please see
    // |fuchsia.accessibility.semantics.SemanticListener| for documentation on
    // hit tests.
    virtual void OnHitTest(fuchsia_math::PointF point,
                           HitTestCallback callback) = 0;

    // Called whenever Fuchsia enables / disables semantic updates.
    virtual void OnSemanticsEnabled(bool enabled) = 0;
  };

  AXFuchsiaSemanticProvider() = default;
  virtual ~AXFuchsiaSemanticProvider() = default;

  // Adds a semantic node to be updated. It is mandatory that the node has at
  // least an unique ID.
  virtual bool Update(fuchsia_accessibility_semantics::Node node) = 0;

  // Marks a semantic node to be deleted. Returns false if the node is not
  // present in the list of semantic nodes known by this provider.
  virtual bool Delete(uint32_t node_id) = 0;

  // Clears the semantic tree.
  virtual bool Clear() = 0;

  // Sends an accessibility event to Fuchsia. Please consult
  // https://cs.opensource.google/fuchsia/fuchsia/+/main:sdk/fidl/fuchsia.accessibility.semantics/semantics_manager.fidl
  // for documentation on events.
  virtual void SendEvent(
      fuchsia_accessibility_semantics::SemanticEvent event) = 0;

  // Returns true if there are pending updates or deletions to be made.
  virtual bool HasPendingUpdates() const = 0;

  // Returns the pixel scale.
  virtual float GetPixelScale() const = 0;

  // Sets the pixel scale.
  virtual void SetPixelScale(float pixel_scale) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_H_
