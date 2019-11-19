// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * An interface that allows clients to handle error page interactions.
 */
public abstract class ErrorPageCallback {
    /**
     * The user has attempted to back out of an error page, such as one warning of an SSL error.
     *
     * @return true if the action was overridden and WebLayer should skip default handling.
     */
    public abstract boolean onBackToSafety();
}
