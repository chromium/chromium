// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_PLATFORM_CURSOR_H_
#define UI_BASE_CURSOR_PLATFORM_CURSOR_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace ui {

// Ref-counted base class for platform-specific cursors.
//
// Sub-classes of PlatformCursor are expected to wrap platform-specific cursor
// resources (e.g. HCURSOR on Windows). Those resources also have
// platform-specific deletion methods that must only be called once the last
// cursor is destroyed, thus requiring a ref-counted type.
// While default cursors (all other than kCustom in
// ui/base/cursor/cursor_type.mojom) are used often during any Chrome session
// and could perhaps be kept alive for the duration of the program, custom
// cursors might incur in high memory usage. Because of this, all types of
// cursors are expected to be ref-counted.
class COMPONENT_EXPORT(UI_BASE_CURSOR) PlatformCursor
    : public base::RefCounted<PlatformCursor> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

 protected:
  friend class base::RefCounted<PlatformCursor>;
  virtual ~PlatformCursor() = default;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_PLATFORM_CURSOR_H_
