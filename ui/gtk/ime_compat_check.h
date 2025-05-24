// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_IME_COMPAT_CHECK_H_
#define UI_GTK_IME_COMPAT_CHECK_H_

namespace gtk {

// Some distros have packaging issues where GTK3 IMEs may be installed but not
// GTK4 IMEs. This function checks for that case, and returns true if the GTK4
// IME is usable. This workaround may be removed when support for older
// distributions like Ubuntu 22.04 is dropped.
[[nodiscard]] bool CheckGtk4X11ImeCompatibility();

}  // namespace gtk

#endif  // UI_GTK_IME_COMPAT_CHECK_H_
