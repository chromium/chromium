// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.webkit.ValueCallback;

/**
 * Used along with {@link MediaCaptureController} to control and observe Media Capture and Streams
 * usage.
 */
abstract class MediaCaptureCallback {
    /**
     * Called when a site in a Tab requests to start capturing media from the user's device.
     *
     * This will be called only if the app already has the requisite permissions and the user has
     * granted permissions to the site that is making the request.
     *
     * At least one of the two boolean parameters will be true. Note that a single tab can have more
     * than one concurrent active stream, and these parameters only represent the state of the new
     * stream.
     *
     * @param audio if true, the new stream includes audio from a microphone.
     * @param video if true, the new stream includes video from a camera.
     * @param requestResult a callback to be run with true if and when the stream can start, or
     *         false if the stream should not start. Must be run on the UI thread.
     */
    public void onMediaCaptureRequested(
            boolean audio, boolean video, ValueCallback<Boolean> requestResult) {}

    /**
     * Called when media streaming state will change in the Tab.
     *
     * A site will receive audio/video from the device’s hardware. This
     * may be called with both parameters false to indicate all streaming
     * has stopped, or multiple times in a row with different parameter
     * values as streaming state changes to include or exclude hardware.
     *
     * @param audio if true, the stream includes audio from a microphone.
     * @param video if true, the stream includes video from a camera.
     */
    public void onMediaCaptureStateChanged(boolean audio, boolean video) {}
}
