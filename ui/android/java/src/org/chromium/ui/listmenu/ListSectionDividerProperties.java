// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for customizing the list section divider. */
public class ListSectionDividerProperties {
    public static final WritableIntPropertyKey LEFT_PADDING_DIMEN_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey RIGHT_PADDING_DIMEN_ID =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {LEFT_PADDING_DIMEN_ID, RIGHT_PADDING_DIMEN_ID};
}
