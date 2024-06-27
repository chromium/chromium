// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_
#define UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_

#include <vector>

#include "base/component_export.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/public/hardware_capabilities.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

// This class can be used to answer questions about possible overlay
// configurations for a particular output device. We get an instance of this
// class from SurfaceFactoryOzone given an AcceleratedWidget.
class COMPONENT_EXPORT(OZONE_BASE) OverlayCandidatesOzone {
 public:
  using OverlaySurfaceCandidateList = std::vector<OverlaySurfaceCandidate>;

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. When setting |overlay_handled| to true,
  // the implementation must also snap |display_rect| to integer coordinates
  // if necessary.
  virtual void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces);

  // Register `receive_callback` to be called with the latest
  // HardwareCapbalitites, whenever displays are configured.
  // `receive_callback` may be called once after OverlayCandidatesOzone is
  // destroyed if there is an in-flight callback, so it should be bound with a
  // WeakPtr.
  virtual void ObserveHardwareCapabilities(
      ui::HardwareCapabilitiesCallback receive_callback);

  // This should be invoked during overlay processing to indicate if there are
  // any candidates for this processor that have an overlay requirement.
  virtual void RegisterOverlayRequirement(bool requires_overlay) {}

  // Invoked on each swap completion. |swap_result| is the result of the last
  // swap.
  virtual void OnSwapBuffersComplete(gfx::SwapResult swap_result) {}

  // Invoked once the overlay processor receives hardware capabilities. This
  // allows to set supported buffer formats in the overlay manager, which can
  // make a decision whether an overlay candidate is overlay-capable as early as
  // possible.
  virtual void SetSupportedBufferFormats(
      base::flat_set<gfx::BufferFormat> supported_buffer_formats) {}

  // Notifies what overlay candidates were actually promoted as overlays.
  // Can be empty.
  virtual void NotifyOverlayPromotion(
      std::vector<gfx::OverlayType> promoted_overlay_types) {}

  virtual ~OverlayCandidatesOzone();
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_
