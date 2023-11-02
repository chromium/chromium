// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * ErrorPage contains the html to show when an error is encountered.
 */
class ErrorPage {
    public final String htmlContent;

    /**
     * Creates an ErrorPage.
     *
     * @param htmlContent The html to show.
     *
     */
    public ErrorPage(@NonNull String htmlContent) {
        this.htmlContent = htmlContent;
    }
}
