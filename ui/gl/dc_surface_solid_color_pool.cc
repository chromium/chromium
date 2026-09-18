// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_surface_solid_color_pool.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"

namespace gl {

namespace {

base::expected<void, CommitError> FillSurface(
    ID3D11Device* d3d11_device,
    IDCompositionSurface* dcomp_surface,
    SkColor4f color) {
  CHECK(dcomp_surface);
  HRESULT hr = S_OK;

  RECT update_rect = {0, 0, kSolidColorSurfaceSize.width(),
                      kSolidColorSurfaceSize.height()};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture;
  POINT update_offset;
  hr = dcomp_surface->BeginDraw(&update_rect, IID_PPV_ARGS(&draw_texture),
                                &update_offset);
  if (FAILED(hr)) {
    return base::unexpected(
        CommitError{CommitError::Reason::kSolidColorSurfaceBeginDraw, hr});
  }

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  hr = d3d11_device->CreateRenderTargetView(draw_texture.Get(), nullptr, &rtv);
  if (FAILED(hr)) {
    return base::unexpected(CommitError{
        CommitError::Reason::kSolidColorSurfaceCreateRenderTargetView, hr});
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  d3d11_device->GetImmediateContext(&immediate_context);
  immediate_context->ClearRenderTargetView(rtv.Get(), color.makeOpaque().vec());

  hr = dcomp_surface->EndDraw();
  if (FAILED(hr)) {
    return base::unexpected(
        CommitError{CommitError::Reason::kSolidColorSurfaceEndDraw, hr});
  }

  return base::ok();
}

base::expected<void, CommitError> CreateSurface(
    IDCompositionDevice3* dcomp_device,
    Microsoft::WRL::ComPtr<IDCompositionSurface>& out_dcomp_surface) {
  HRESULT hr = dcomp_device->CreateSurface(
      kSolidColorSurfaceSize.width(), kSolidColorSurfaceSize.height(),
      gfx::ColorSpaceWin::GetDXGIFormat(gfx::ColorSpace::CreateSRGB()),
      DXGI_ALPHA_MODE_IGNORE, &out_dcomp_surface);
  if (FAILED(hr)) {
    return base::unexpected(CommitError{
        CommitError::Reason::kSolidColorSurfacePoolCreateSurface, hr});
  }
  return base::ok();
}

}  // namespace

DCSurfaceSolidColorPool::Surface::Surface() = default;

DCSurfaceSolidColorPool::Surface::~Surface() = default;

IUnknown* DCSurfaceSolidColorPool::Surface::GetContent() const {
  return dcomp_surface.Get();
}

DCSurfaceSolidColorPool::DCSurfaceSolidColorPool(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device)
    : d3d11_device_(std::move(d3d11_device)),
      dcomp_device_(std::move(dcomp_device)) {
  CHECK(d3d11_device_);
  CHECK(dcomp_device_);
}

DCSurfaceSolidColorPool::~DCSurfaceSolidColorPool() = default;

base::expected<void, CommitError> DCSurfaceSolidColorPool::FillEntry(
    std::unique_ptr<Entry>& base_entry,
    const SkColor4f& color) {
  // Move the surface out of any existing entry and only install it back
  // (allocating a fresh `Surface` if needed) after the fill succeeds, so
  // a failure mid-fill never leaves the entry in a partially-updated state.
  Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
  if (auto* existing = static_cast<Surface*>(base_entry.get())) {
    dcomp_surface = std::move(existing->dcomp_surface);
  }
  if (!dcomp_surface) {
    RETURN_IF_ERROR(CreateSurface(dcomp_device_.Get(), dcomp_surface));
  }
  RETURN_IF_ERROR(FillSurface(d3d11_device_.Get(), dcomp_surface.Get(), color));

  if (!base_entry) {
    base_entry = std::make_unique<Surface>();
  }
  auto* surface = static_cast<Surface*>(base_entry.get());
  surface->dcomp_surface = std::move(dcomp_surface);
  return base::ok();
}

SolidColorPoolFactory CreateDCSurfaceSolidColorPoolFactory(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  return base::BindRepeating(
      [](Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
         IDCompositionDevice3* dcomp_device)
          -> std::unique_ptr<SolidColorPoolBase> {
        CHECK(dcomp_device);
        return std::make_unique<DCSurfaceSolidColorPool>(
            std::move(d3d11_device),
            Microsoft::WRL::ComPtr<IDCompositionDevice3>(dcomp_device));
      },
      std::move(d3d11_device));
}

}  // namespace gl
