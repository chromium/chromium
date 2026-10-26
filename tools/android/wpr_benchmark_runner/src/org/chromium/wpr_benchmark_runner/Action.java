// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

@NullMarked
interface Action {
    void run() throws Exception;
}
