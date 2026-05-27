// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.color;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for resolving Material3 theme dynamic colors. Implemented in chrome/android where
 * R.attr values are compiled constants.
 */
@NullMarked
public interface ColorProviderBridge {
    long[] getThemeColors(Context context);
}
