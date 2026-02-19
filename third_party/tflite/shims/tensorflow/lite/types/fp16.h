// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Prefer the third_party fp16 library over the fp16.h that's bundled with
// tflite. The "shims" folder should be before "src" in the include path so
// that this file will override tflite's copy.

#include "third_party/fp16/src/include/fp16.h"
