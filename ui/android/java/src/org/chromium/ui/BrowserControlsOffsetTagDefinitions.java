// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import androidx.annotation.NonNull;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    public @NonNull BrowserControlsOffsetTags getTags() {
        return mTags;
    }

    @CalledByNative
    public @NonNull BrowserControlsOffsetTagConstraints getConstraints() {
        return mConstraints;
    }

    @Override
    public boolean equals(@Nullable Object other) {
        return other instanceof BrowserControlsOffsetTagDefinitions that
                && mTags.equals(that.getTags())
                && mConstraints.equals(that.getConstraints());
    }

    @Override
    public String toString() {
        String topTag =
                mTags.getTopControlsOffsetTag() == null
                        ? "null"
                        : mTags.getTopControlsOffsetTag().toString();
        String contentTag =
                mTags.getContentOffsetTag() == null
                        ? "null"
                        : mTags.getContentOffsetTag().toString();
        String bottomTag =
                mTags.getBottomControlsOffsetTag() == null
                        ? "null"
                        : mTags.getBottomControlsOffsetTag().toString();
        String topConstraints =
                mConstraints.getTopControlsConstraints() == null
                        ? "null"
                        : mConstraints.getTopControlsConstraints().toString();
        String contentConstraints =
                mConstraints.getContentConstraints() == null
                        ? "null"
                        : mConstraints.getContentConstraints().toString();
        String bottomConstriants =
                mConstraints.getBottomControlsConstraints() == null
                        ? "null"
                        : mConstraints.getBottomControlsConstraints().toString();
        return "tags: "
                + topTag
                + " | "
                + contentTag
                + " | "
                + bottomTag
                + " | "
                + "constraints: "
                + topConstraints
                + " | "
                + contentConstraints
                + " | "
                + bottomConstriants;
    }
}
