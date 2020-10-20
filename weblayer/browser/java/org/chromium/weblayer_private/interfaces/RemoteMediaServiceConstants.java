// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/** Keys for remote media service intent extras. */
public interface RemoteMediaServiceConstants {
    // Used as a key in the client's AndroidManifest.xml to enable remote media playback, i.e.
    // Presentation API, Remote Playback API, and Media Fling (automatic casting of html5 videos).
    String FEATURE_ENABLED_KEY = "org.chromium.weblayer.ENABLE_REMOTE_MEDIA";

    // Used internally by WebLayer as a key to the various values of remote media service
    // notification IDs.
    String NOTIFICATION_ID_KEY = "REMOTE_MEDIA_SERVICE_NOTIFICATION_ID_KEY";
}
