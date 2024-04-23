// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("wolvic")
public class SessionSettings {
    public enum UserAgentMode {
        // values have to be synchronized with session_settings.h
        MOBILE(0),
        DESKTOP(1),
        MOBILE_VR(2);

        private static final UserAgentMode[] modes = UserAgentMode.values();
        private final int value;

        private UserAgentMode(int value) {
            this.value = value;
        }

        private int getValue() {
            return value;
        }

        private static UserAgentMode fromValue(int value) {
            return modes[value];
        }
    };

    public SessionSettings() {}

    public void setUserAgentMode(UserAgentMode mode) {
        SessionSettingsJni.get().setUserAgentMode(mode.getValue());
    }

    public UserAgentMode getUserAgentMode() {
        return UserAgentMode.fromValue(SessionSettingsJni.get().getUserAgentMode());
    }

    public void setUserAgentOverride(@Nullable String value) {
        SessionSettingsJni.get().setUserAgentOverride(value);
    }

    @Nullable
    public String getUserAgentOverride() {
        return SessionSettingsJni.get().getUserAgentOverride();
    }

    public String getDefaultUserAgent(UserAgentMode mode) {
        return SessionSettingsJni.get().getDefaultUserAgent(mode.getValue());
    }

    @NativeMethods
    public interface Natives {
        void setUserAgentMode(int value);
        int getUserAgentMode();
        void setUserAgentOverride(@Nullable String value);
        @Nullable
        String getUserAgentOverride();
        String getDefaultUserAgent(int value);
    }
}
