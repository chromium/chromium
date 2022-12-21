// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

oneway interface IStringCallback {
    void onResult(in String result) = 1;

    // TODO(swestphal): Replace parameters with actual Exception when supported to also propagate stacktrace.
    void onException(in int type, in String msg) = 2;
}