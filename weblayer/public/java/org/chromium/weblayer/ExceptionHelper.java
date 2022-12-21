// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.weblayer_private.interfaces.ExceptionType;

class ExceptionHelper {
    static @ExceptionType int convertType(@ExceptionType int type) {
        switch (type) {
            case ExceptionType.RESTRICTED_API:
                return org.chromium.webengine.interfaces.ExceptionType.RESTRICTED_API;
            case ExceptionType.UNKNOWN:
                return org.chromium.webengine.interfaces.ExceptionType.UNKNOWN;
        }
        assert false : "Unexpected ExceptionType: " + String.valueOf(type);
        return org.chromium.webengine.interfaces.ExceptionType.UNKNOWN;
    }
}
