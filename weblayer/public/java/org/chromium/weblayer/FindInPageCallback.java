// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Informed of find in page results.
 */
abstract class FindInPageCallback {
    /**
     * Called when incremental results from a find operation are ready.
     *
     * @param numberOfMatches The total number of matches found thus far.
     * @param activeMatchIndex The index of the currently highlighted match.
     * @param finalUpdate Whether this is the last find result that can be expected for the current
     *         find operation.
     */
    public void onFindResult(int numberOfMatches, int activeMatchIndex, boolean finalUpdate) {}

    /**
     * Called when WebLayer has ended the find session, for example due to the Tab losing active
     * status. This will not be invoked when the client ends a find session via {@link
     * FindInPageController#setFindInPageCallback} with a {@code null} value.
     */
    public void onFindEnded() {}
}
