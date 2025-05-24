// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

Clipboard* Clipboard::Create() {
  // TODO(crbug.com/391914246): Implement the Clipboard when tvOS actually
  // requires the functionality.
  NOTREACHED();
}

}  // namespace ui
