// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IPrerenderController;
import org.chromium.weblayer_private.interfaces.IProfile;

/**
 * PrerenderController enables prerendering of urls.
 *
 * Prerendering has the same effect as adding a link rel="prerender" resource hint to a web page. It
 * is implemented using NoStatePrefetch and fetches resources needed for a url in advance, but does
 * not execute Javascript or render any part of the page in advance. For more information on
 * NoStatePrefetch, see https://developers.google.com/web/updates/2018/07/nostate-prefetch.
 */
class PrerenderController {
    private final IPrerenderController mImpl;

    static PrerenderController create(IProfile profile) {
        try {
            return new PrerenderController(profile.getPrerenderController());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    // Constructor for test mocking.
    protected PrerenderController() {
        mImpl = null;
    }

    PrerenderController(IPrerenderController prerenderController) {
        mImpl = prerenderController;
    }

    /*
     * Creates a prerender for the url provided.
     * Prerendering here is implemented using NoStatePrefetch. We fetch resources and save them
     * to the HTTP cache. All resources are cached according to their cache headers. For details,
     * see https://developers.google.com/web/updates/2018/07/nostate-prefetch#implementation.
     *
     * On low end devices or when the device has too many renderers running and prerender is
     * considered expensive, we do preconnect instead. Preconnect involves creating connections with
     * the server without actually fetching any resources. For more information on preconnect, see
     * https://www.chromium.org/developers/design-documents/network-stack/preconnect.
     * @param uri The uri to prerender.
     */
    public void schedulePrerender(@NonNull Uri uri) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.prerender(uri.toString());
        } catch (RemoteException exception) {
            throw new APICallException(exception);
        }
    }
}
