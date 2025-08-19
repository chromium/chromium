// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import org.chromium.build.annotations.NullMarked;

/** Interface that indicate the class is capable to handle incognito state updates. */
@NullMarked
public interface StatefulIncognitoAware extends IncognitoAware {

    /**
     * Called when incognito state is updated.
     *
     * @param source The source of the theme resource change.
     * @param isIncognito Whether the new state is incognito.
     */
    void onIncognitoStateChanged(ThemeResourceWrapper source, boolean isIncognito);
}
