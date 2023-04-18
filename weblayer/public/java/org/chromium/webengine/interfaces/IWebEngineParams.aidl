// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

parcelable IWebEngineParams {
    String profileName;
    String persistenceId;
    boolean isIncognito;
    boolean isExternalIntentsEnabled;
    @nullable List<String> allowedOrigins;
}
