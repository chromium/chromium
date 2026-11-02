// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.BrowserControlsOffsetTags;

/** Java counterpart to the native ui::BrowserControlsOffsetTagDefinitions. */
@DoNotMock("This is a simple value object.")
@NullMarked
public final class BrowserControlsOffsetTagDefinitions {
    private final BrowserControlsOffsetTags mTags;
    private final BrowserControlsOffsetTagConstraints mConstraints;

    public BrowserControlsOffsetTagDefinitions() {
        mTags = new BrowserControlsOffsetTags(null, null, null);
        mConstraints = new BrowserControlsOffsetTagConstraints(null, null, null);
    }

    public BrowserControlsOffsetTagDefinitions(
            BrowserControlsOffsetTags tags, BrowserControlsOffsetTagConstraints constraints) {
        mTags = tags;
        mConstraints = constraints;
    }

    @CalledByNative
    public BrowserControlsOffsetTags getTags() {
        return mTags;
    }

    @CalledByNative
    public BrowserControlsOffsetTagConstraints getConstraints() {
        return mConstraints;
    }
}
