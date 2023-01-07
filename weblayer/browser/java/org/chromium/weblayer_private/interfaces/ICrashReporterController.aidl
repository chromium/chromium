// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.ICrashReporterControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface ICrashReporterController {
    void setClient(in ICrashReporterControllerClient client) = 0;
    void checkForPendingCrashReports() = 1;
    Bundle getCrashKeys(in String localId) = 2;
    void deleteCrash(in String localId) = 3;
    void uploadCrash(in String localId) = 4;
}
