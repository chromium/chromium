// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Initiates find-in-page operations.
 *
 * There is one FindInPageController per {@link Tab}, and only the active tab may have an active
 * find session.
 */
class FindInPageController {
    private final ITab mTab;

    protected FindInPageController() {
        mTab = null;
    }

    FindInPageController(ITab tab) {
        mTab = tab;
    }

    /**
     * Starts or ends a find session.
     *
     * When called with a non-null parameter, this starts a find session and displays a find result
     * bar over the affected web page. When called with a null parameter, the find session will end
     * and the result bar will be removed.
     *
     * @param callback The object that will be notified of find results, or null to end the find
     *         session.
     * @return True if the operation succeeded. Ending a find session will always succeed, but
     *         starting one may fail, for example if the tab is not active or a find session is
     *         already started.
     */
    public boolean setFindInPageCallback(@Nullable FindInPageCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            FindInPageCallbackClientImpl callbackClient = null;
            if (callback != null) {
                callbackClient = new FindInPageCallbackClientImpl(callback);
            }
            return mTab.setFindInPageCallbackClient(callbackClient);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Called to initiate an in-page text search for the given string.
     *
     * Results will be highlighted in context, with additional attention drawn to the "active"
     * result. The position of results will also be displayed on the find result bar.
     * This has no effect if there is no active find session.
     *
     * @param searchText The {@link String} to search for. Any pre-existing search results will be
     *         cleared.
     * @param forward The direction to move the "active" result. This only applies when the search
     *         text matches that of the last search.
     */
    public void find(@NonNull String searchText, boolean forward) {
        ThreadCheck.ensureOnUiThread();
        try {
            mTab.findInPage(searchText, forward);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private static final class FindInPageCallbackClientImpl extends IFindInPageCallbackClient.Stub {
        private final FindInPageCallback mCallback;

        FindInPageCallbackClientImpl(FindInPageCallback callback) {
            mCallback = callback;
        }

        public FindInPageCallback getCallback() {
            return mCallback;
        }

        @Override
        public void onFindResult(int numberOfMatches, int activeMatchOrdinal, boolean finalUpdate) {
            StrictModeWorkaround.apply();
            mCallback.onFindResult(numberOfMatches, activeMatchOrdinal, finalUpdate);
        }

        @Override
        public void onFindEnded() {
            StrictModeWorkaround.apply();
            mCallback.onFindEnded();
        }
    }
}
