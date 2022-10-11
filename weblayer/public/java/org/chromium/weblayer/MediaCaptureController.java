// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Used to control Media Capture and Streams Web API operations.
 *
 * The Web API is used by sites to record video and audio from the user's device, e.g. for voice
 * recognition or video conferencing. There is one MediaCaptureController per {@link Tab}.
 */
class MediaCaptureController {
    private final ITab mTab;

    protected MediaCaptureController() {
        mTab = null;
    }

    MediaCaptureController(ITab tab) {
        mTab = tab;
    }

    /**
     * Sets the callback for the tab.
     * @param MediaCaptureCallback the callback to use, or null. If null, capture requests will be
     *         allowed (assuming the system and user granted permission).
     */
    public void setMediaCaptureCallback(@Nullable MediaCaptureCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            MediaCaptureCallbackClientImpl callbackClient = null;
            if (callback != null) {
                callbackClient = new MediaCaptureCallbackClientImpl(callback);
            }
            mTab.setMediaCaptureCallbackClient(callbackClient);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Called to stop the active media stream(s) in a tab.
     */
    public void stopMediaCapturing() {
        ThreadCheck.ensureOnUiThread();
        try {
            mTab.stopMediaCapturing();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private static final class MediaCaptureCallbackClientImpl
            extends IMediaCaptureCallbackClient.Stub {
        private final MediaCaptureCallback mCallback;

        MediaCaptureCallbackClientImpl(MediaCaptureCallback callback) {
            mCallback = callback;
        }

        public MediaCaptureCallback getCallback() {
            return mCallback;
        }

        @Override
        public void onMediaCaptureRequested(
                boolean audio, boolean video, IObjectWrapper callbackWrapper) {
            StrictModeWorkaround.apply();
            ValueCallback<Boolean> requestResultCallback =
                    (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                            callbackWrapper, ValueCallback.class);
            mCallback.onMediaCaptureRequested(audio, video, requestResultCallback);
        }

        @Override
        public void onMediaCaptureStateChanged(boolean audio, boolean video) {
            StrictModeWorkaround.apply();
            mCallback.onMediaCaptureStateChanged(audio, video);
        }
    }
}
