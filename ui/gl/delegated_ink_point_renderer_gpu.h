// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_
#define UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_

#include <dcomp.h>
#include <wrl/client.h>

#include <memory>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "ui/gfx/delegated_ink_metadata.h"

// Required for SFINAE to check if these Win10 types exist.
// TODO(1171374): Remove this when the types are available in the Win10 SDK.
struct IDCompositionInkTrailDevice;
struct IDCompositionDelegatedInkTrail;

namespace gl {

// TODO(1171374): Remove this class and remove templates when the types are
// available in the Win10 SDK.
template <typename InkTrailDevice,
          typename DelegatedInkTrail,
          typename = void,
          typename = void>
class DelegatedInkPointRendererGpu {
 public:
  DelegatedInkPointRendererGpu() = default;
  bool Initialize(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device,
      const Microsoft::WRL::ComPtr<IDXGISwapChain1>& root_swap_chain) {
    NOTREACHED();
    return false;
  }

  bool HasBeenInitialized() const { return false; }

  IDCompositionVisual* GetInkVisual() const {
    NOTREACHED();
    return nullptr;
  }

  bool DelegatedInkIsSupported(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device) const {
    return false;
  }

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
    NOTREACHED();
  }
};

// This class will call the OS delegated ink trail APIs when they become
// available. Currently using SFINAE to land changes ahead of the SDK, so that
// the OS APIs can be used immediately.
// TODO(1171374): Remove the template SFINAE.
//
// On construction, this class will create a new visual for the visual tree with
// an IDCompositionDelegatedInk object as the contents. This will be added as
// a child of the root surface visual in the tree, and the trail will be drawn
// to it. It is a child of the root surface visual because this visual contains
// the swapchain, and there will be no transforms applied to the delegated ink
// visual this way.
// For more information about the design of this class and using the OS APIs,
// view the design doc here: https://aka.ms/GPUBackedDesignDoc
template <typename InkTrailDevice, typename DelegatedInkTrail>
class DelegatedInkPointRendererGpu<InkTrailDevice,
                                   DelegatedInkTrail,
                                   decltype(typeid(InkTrailDevice), void()),
                                   decltype(typeid(DelegatedInkTrail),
                                            void())> {
 public:
  DelegatedInkPointRendererGpu() = default;

  bool Initialize(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device2,
      const Microsoft::WRL::ComPtr<IDXGISwapChain1>& root_swap_chain) {
    if (dcomp_device_ == dcomp_device2.Get() &&
        swap_chain_ == root_swap_chain.Get() && HasBeenInitialized()) {
      return true;
    }

    dcomp_device_ = dcomp_device2.Get();
    swap_chain_ = root_swap_chain.Get();

    Microsoft::WRL::ComPtr<InkTrailDevice> ink_trail_device;
    TraceEventOnFailure(dcomp_device2.As(&ink_trail_device),
                        "DelegatedInkPointRendererGpu::Initialize - "
                        "DCompDevice2 as InkTrailDevice failed");
    if (!ink_trail_device)
      return false;

    TraceEventOnFailure(
        ink_trail_device->CreateDelegatedInkTrailForSwapChain(
            root_swap_chain.Get(), &delegated_ink_trail_),
        "DelegatedInkPointRendererGpu::Initialize - Failed to create "
        "delegated ink trail.");
    if (!delegated_ink_trail_)
      return false;

    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device;
    TraceEventOnFailure(dcomp_device2.As(&dcomp_device),
                        "DelegatedInkPointRendererGpu::Initialize - "
                        "DCompDevice2 as DCompDevice failed");
    if (!dcomp_device)
      return false;

    TraceEventOnFailure(dcomp_device->CreateVisual(&ink_visual_),
                        "DelegatedInkPointRendererGpu::Initialize - "
                        "Failed to create ink visual.");
    if (!ink_visual_)
      return false;

    if (TraceEventOnFailure(
            ink_visual_->SetContent(delegated_ink_trail_.Get()),
            "DelegatedInkPointRendererGpu::Initialize - SetContent failed")) {
      // Initialization has failed because SetContent failed. However, we must
      // reset the members so that HasBeenInitialized() does not return true
      // when queried.
      ink_visual_.Reset();
      delegated_ink_trail_.Reset();
      return false;
    }

    return true;
  }

  bool HasBeenInitialized() const {
    return ink_visual_ && delegated_ink_trail_;
  }

  IDCompositionVisual* GetInkVisual() const { return ink_visual_.Get(); }

  bool DelegatedInkIsSupported(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device) const {
    Microsoft::WRL::ComPtr<InkTrailDevice> ink_trail_device;
    HRESULT hr = dcomp_device.As(&ink_trail_device);
    return hr == S_OK;
  }

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
    TRACE_EVENT1("gpu",
                 "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint",
                 "metadata", metadata->ToString());
    metadata_ = std::move(metadata);
    // TODO(1052145): Start using OS APIs to inform of the starting point of the
    // ink trail.
  }

 private:
  // Note that this returns true if the HRESULT is anything other than S_OK,
  // meaning that it returns true when an event is traced (because of a
  // failure).
  static bool TraceEventOnFailure(HRESULT hr, const char* name) {
    if (!FAILED(hr))
      return false;

    TRACE_EVENT_INSTANT1("gpu", name, TRACE_EVENT_SCOPE_THREAD, "hr", hr);
    return true;
  }

  // The visual within the tree that will contain the delegated ink trail. It
  // should be a child of the root surface visual.
  Microsoft::WRL::ComPtr<IDCompositionVisual> ink_visual_;

  // The delegated ink trail object that the ink trail is drawn on. This is the
  // content of the ink visual.
  Microsoft::WRL::ComPtr<DelegatedInkTrail> delegated_ink_trail_;

  // The most recent metadata received. The metadata marks the last point of
  // the app rendered stroke, which corresponds to the first point of the
  // delegated ink trail that will be drawn.
  std::unique_ptr<gfx::DelegatedInkMetadata> metadata_;

  // Remember the dcomp device and swap chain used to create
  // |delegated_ink_trail_| and |ink_visual_| so that we can avoid recreating
  // them when it isn't necessary.
  IDCompositionDevice2* dcomp_device_ = nullptr;
  IDXGISwapChain1* swap_chain_ = nullptr;
};

}  // namespace gl

#endif  // UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_
