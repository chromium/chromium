// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import org.chromium.webengine.interfaces.ExceptionType;

class ExceptionHelper {
    static Exception createException(@ExceptionType int type, String msg) {
        switch (type) {
            case ExceptionType.RESTRICTED_API:
                return new RestrictedAPIException(msg);
            case ExceptionType.INVALID_ARGUMENT:
                return new IllegalArgumentException(msg);
            case ExceptionType.UNKNOWN:
                return new RuntimeException(msg);
        }
        // Should not happen.
        return new RuntimeException(msg);
    }
}
