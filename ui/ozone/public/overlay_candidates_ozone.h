// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_
#define UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_

#include <vector>

#include "base/component_export.h"
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

  virtual ~OverlayCandidatesOzone();
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_
