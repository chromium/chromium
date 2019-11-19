// Copyright 2017 The Chromium Authors. All rights reserved.
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
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (ca_layer_params.io_surface_mach_port) {
    DCHECK(!ca_layer_params.ca_context_id);
    return gfx::mojom::CALayerContent::NewIoSurfaceMachPort(
        mojo::WrapMachPort(ca_layer_params.io_surface_mach_port.get()));
  }
#endif
  return gfx::mojom::CALayerContent::NewCaContextId(
      ca_layer_params.ca_context_id);
}

bool StructTraits<gfx::mojom::CALayerParamsDataView, gfx::CALayerParams>::Read(
    gfx::mojom::CALayerParamsDataView data,
    gfx::CALayerParams* out) {
  out->is_empty = data.is_empty();

  gfx::mojom::CALayerContentDataView content_data;
  data.GetContentDataView(&content_data);
  switch (content_data.tag()) {
    case gfx::mojom::CALayerContentDataView::Tag::CA_CONTEXT_ID:
      out->ca_context_id = content_data.ca_context_id();
      break;
    case gfx::mojom::CALayerContentDataView::Tag::IO_SURFACE_MACH_PORT:
#if defined(OS_MACOSX) && !defined(OS_IOS)
      mach_port_t io_surface_mach_port;
      MojoResult unwrap_result = mojo::UnwrapMachPort(
          content_data.TakeIoSurfaceMachPort(), &io_surface_mach_port);
      if (unwrap_result != MOJO_RESULT_OK)
        return false;
      out->io_surface_mach_port.reset(io_surface_mach_port);
#else
      return false;
#endif
      break;
  }

  if (!data.ReadPixelSize(&out->pixel_size))
    return false;

  out->scale_factor = data.scale_factor();
  return true;
}

}  // namespace mojo
