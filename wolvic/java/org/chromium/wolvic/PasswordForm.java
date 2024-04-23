// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

public class PasswordForm {
    private final @NonNull String mUsername;
    private final @NonNull String mPassword;
    private final @NonNull String mOrigin;
    private final @Nullable String mFormActionOrigin;
    private final @Nullable String mHttpRealm;
    // GUID in gecko storage will set keychain identifier in chromium.
    private @Nullable String mGuid;

    public PasswordForm(String username, String password, String origin,
                        String formActionOrigin, String httpRealm, String guid) {
        mUsername = username;
        mPassword = password;
        mOrigin = origin;
        mFormActionOrigin = formActionOrigin;
        mHttpRealm = httpRealm;
        mGuid = guid;
    }

    @CalledByNative
    public String getGuid() {
        return mGuid;
    }

    @CalledByNative
    public void setGuid(String guid) {
        mGuid = guid;
    }

    @CalledByNative
    public String getUsername() {
        return mUsername;
    }

    @CalledByNative
    public String getPassword() {
        return mPassword;
    }

    @CalledByNative
    public String getOrigin() {
        return mOrigin;
    }

    @CalledByNative
    public String getFormActionOrigin() {
        return mFormActionOrigin;
    }

    @CalledByNative
    public String getHttpRealm() {
        return mHttpRealm;
    }

    @CalledByNative
    private static PasswordForm createPasswordForm(
            String username, String password, String origin,
            String formActionOrigin, String httpRealm, String keychainId) {
        return new PasswordForm(username, password, origin, formActionOrigin, httpRealm, keychainId);
    }

    @CalledByNative
    private static PasswordForm[] createPasswordFormArray(int size) {
        return new PasswordForm[size];
    }
}
