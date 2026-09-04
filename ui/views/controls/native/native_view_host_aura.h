// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {

class NativeViewHost;

// Aura implementation of NativeViewHostWrapper.
class NativeViewHostAura : public NativeViewHostWrapper,
                           public aura::WindowObserver {
 public:
  explicit NativeViewHostAura(NativeViewHost* host);

  NativeViewHostAura(const NativeViewHostAura&) = delete;
  NativeViewHostAura& operator=(const NativeViewHostAura&) = delete;

  ~NativeViewHostAura() override;

  // Overridden from NativeViewHostWrapper:
  void AttachNativeView() override;
  void NativeViewDetaching(bool destroyed) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool SetNativeViewCornerRadii(
      const gfx::RoundedCornersF& corner_radii) override;
  gfx::RoundedCornersF GetNativeViewCornerRadii() const override;
  gfx::Rect GetNativeViewClipRect() const override;

  void SetHitTestTopInset(int top_inset) override;
  int GetHitTestTopInset() const override;
  void InstallClip(int x, int y, int w, int h) override;
  bool HasInstalledClip() override;
  void UninstallClip() override;
  bool SetNativeViewClipRect(const gfx::Rect& clip_rect) override;
  void ShowWidget(int x, int y, int w, int h, int native_w, int native_h)
      override;
  void HideWidget() override;
  void SetFocus() override;
  gfx::NativeView GetNativeViewContainer() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::Cursor GetCursor(int x, int y) override;
  void SetVisible(bool visible) override;
  void SetParentAccessible(gfx::NativeViewAccessible) override;
  gfx::NativeViewAccessible GetParentAccessible() override;
  ui::Layer* GetUILayer() override;

 private:
  friend class NativeViewHostAuraTest;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;

  // Sets or updates the |corner_radii_| on the native view's layer.
  void ApplyRoundedCorners();

  // Updates the clip on the native view's layer.
  void UpdateLayerClip();

  // Returns the actual clip rect to be applied, combining layout clip and top
  // inset.
  gfx::Rect GetActualClipRect() const;

  // Our associated NativeViewHost.
  raw_ptr<NativeViewHost> host_;

  // If set, this is applied to the the layer to clip the content of attached
  // native view.
  std::optional<gfx::Rect> clip_rect_;

  // Holds the corner_radii to be applied.
  gfx::RoundedCornersF corner_radii_;

  // Set when AttachNativeView() is called. This is the original transform of
  // the NativeView's layer. The NativeView's layer may be modified to scale
  // when ShowWidget() is called with a native view size not equal to the
  // region's size. When NativeViewDetaching() is called, the NativeView's
  // transform is restored to this.
  gfx::Transform original_transform_;

  // True if a transform different from the original was set.
  bool original_transform_changed_ = false;

  // The external clip rect of the native view.
  std::optional<gfx::Rect> external_clip_rect_;

  // The top insets to exclude the underlying native view from the target.
  int top_inset_ = 0;

  // If attached, this contains the value of owned_by_parent of the
  // native view.
  std::optional<bool> owned_by_parent_;

  // Observation of the attached NativeView. Points at host_->native_view()
  // between AttachNativeView() and NativeViewDetaching()/window destruction.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      native_view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
