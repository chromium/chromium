// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WINDOW_OPEN_DISPOSITION_H_
#define UI_BASE_WINDOW_OPEN_DISPOSITION_H_

#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

// Alias until all clients of `WindowOpenDisposition` have been migrated to
// `ui::mojom::WindowOpenDisposition`. Will be removed once that is complete.
// TODO(crbug.com/373551504): Remove this alias.
using WindowOpenDisposition = ::ui::mojom::WindowOpenDisposition;

#endif  // UI_BASE_WINDOW_OPEN_DISPOSITION_H_
