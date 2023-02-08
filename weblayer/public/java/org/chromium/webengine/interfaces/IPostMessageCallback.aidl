// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

oneway interface IPostMessageCallback {
    void onPostMessage(in String result, in String origin) = 1;
}