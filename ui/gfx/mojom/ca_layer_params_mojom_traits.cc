// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/ca_layer_params_mojom_traits.h"

#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

gfx::mojom::CALayerContentPtr
StructTraits<gfx::mojom::CALayerParamsDataView, gfx::CALayerParams>::content(
    const gfx::CALayerParams& ca_layer_params) {
#if BUILDFLAG(IS_APPLE)
  if (ca_layer_params.io_surface_mach_port) {
    DCHECK(!ca_layer_params.ca_context_id);
    DCHECK(!ca_layer_params.ca_context_fence_mach_port.get());
    return gfx::mojom::CALayerContent::NewIoSurfaceMachPort(
        mojo::PlatformHandle(base::apple::RetainMachSendRight(
            ca_layer_params.io_surface_mach_port.get())));
  }
  if (ca_layer_params.ca_context_id) {
    auto ca_context = gfx::mojom::CAContext::New(
        ca_layer_params.ca_context_id,
        mojo::PlatformHandle(base::apple::RetainMachSendRight(
            ca_layer_params.ca_context_fence_mach_port.get())));
    return gfx::mojom::CALayerContent::NewCaContext(std::move(ca_context));
  }
#endif
  return nullptr;
}

bool StructTraits<gfx::mojom::CALayerParamsDataView, gfx::CALayerParams>::Read(
    gfx::mojom::CALayerParamsDataView data,
    gfx::CALayerParams* out) {
#if BUILDFLAG(IS_APPLE)
  gfx::mojom::CALayerContentDataView content_data;
  data.GetContentDataView(&content_data);
  if (!content_data.is_null()) {
    switch (content_data.tag()) {
      case gfx::mojom::CALayerContentDataView::Tag::kCaContext: {
        gfx::mojom::CAContextDataView ca_context;
        content_data.GetCaContextDataView(&ca_context);
        out->ca_context_id = ca_context.id();
        mojo::PlatformHandle platform_handle = ca_context.TakeFenceMachPort();
        if (platform_handle.is_mach_send()) {
          out->ca_context_fence_mach_port.reset(
              platform_handle.ReleaseMachSendRight());
        }
        break;
      }
      case gfx::mojom::CALayerContentDataView::Tag::kIoSurfaceMachPort: {
        mojo::PlatformHandle platform_handle =
            content_data.TakeIoSurfaceMachPort();
        if (!platform_handle.is_mach_send()) {
          return false;
        }
        out->io_surface_mach_port.reset(platform_handle.ReleaseMachSendRight());
        break;
      }
    }
  }
#endif

  if (!data.ReadPixelSize(&out->pixel_size))
    return false;

  out->scale_factor = data.scale_factor();
  return true;
}

}  // namespace mojo
