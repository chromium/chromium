// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/fuchsia/view_ref_pair.h"

namespace ui {

ViewRefPair ViewRefPair::New() {
  ViewRefPair ref_pair;
  zx::eventpair::create(/*options*/ 0u, &ref_pair.control_ref.reference,
                        &ref_pair.view_ref.reference);
  ref_pair.control_ref.reference.replace(
      ZX_DEFAULT_EVENTPAIR_RIGHTS & (~ZX_RIGHT_DUPLICATE),
      &ref_pair.control_ref.reference);
  ref_pair.view_ref.reference.replace(ZX_RIGHTS_BASIC,
                                      &ref_pair.view_ref.reference);
  return ref_pair;
}

}  // namespace ui
