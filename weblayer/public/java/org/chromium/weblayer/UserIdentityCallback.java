// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.graphics.Bitmap;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;

/**
 * Used to provide details about the current user's identity.
 *
 * If this callback is implemented and set on {@link Profile}, the information is used to better
 * organize contact details in the navigator.contacts UI as well as by Autofill Assistant.
 */
abstract class UserIdentityCallback {
    /**
     * The current user's email address. If no user is signed in or the email is currently
     * unavailable, this should return an empty string.
     */
    public @NonNull String getEmail() {
        return new String();
    }

    /**
     * Returns the full name of the current user, or empty if the user is signed out. This can
     * be provided on a best effort basis if the name is not available immediately.
     */
    public @NonNull String getFullName() {
        return new String();
    }

    /**
     * Called to retrieve the signed-in user's avatar.
     * @param desiredSize the size the avatar will be displayed at, in raw pixels. If a different
     *         size avatar is returned, WebLayer will scale the returned image.
     * @param avatarLoadedCallback to be called with the avatar when it is available (synchronously
     *         or asynchronously). Until such time that it's called, WebLayer will fall back to a
     *         monogram based on {@link getFullName()}, e.g. encircled "JD" for "Jill Doe". This
     *         will no-op if the associated {@link Profile} object is destroyed before this is
     *         called.
     */
    public void getAvatar(int desiredSize, @NonNull ValueCallback<Bitmap> avatarLoadedCallback) {}
}
