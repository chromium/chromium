// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.weblayer_private.interfaces.RestrictedAPIException;

class ExceptionHelper {
    /**
     * Children of RuntimeExceptions lose their specific type when passed through AIDL-interfaces.
     * This function identifies the exception type and reraises the according child exception.
     */
    static void reraise(RuntimeException e) {
        if (RestrictedAPIException.isInstance(e)) {
            throw new RestrictedAPIException();
        }
        throw e;
    }
}
