// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

oneway interface IBooleanCallback {
    void onResult(in boolean result) = 1;
}