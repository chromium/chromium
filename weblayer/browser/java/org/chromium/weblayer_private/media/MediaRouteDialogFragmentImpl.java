// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.media;

import android.content.Context;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.FragmentHostingRemoteFragmentImpl;
import org.chromium.weblayer_private.R;
import org.chromium.weblayer_private.interfaces.IMediaRouteDialogFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.lang.ref.WeakReference;

/**
 * WebLayer's implementation of the client library's MediaRouteDialogFragment.
 *
 * This class is the impl-side representation of a client fragment which is added to the browser
 * fragment, and is parent to MediaRouter-related {@link DialogFragment} instances. This class will
 * automatically clean up the client-side fragment when the child fragment is detached.
 */
public class MediaRouteDialogFragmentImpl extends FragmentHostingRemoteFragmentImpl {
    // The instance for the currently active dialog, if any. This is a WeakReference to get around
    // StaticFieldLeak warnings.
    private static WeakReference<MediaRouteDialogFragmentImpl> sInstanceForTest;

    private static class MediaRouteDialogContext
            extends FragmentHostingRemoteFragmentImpl.RemoteFragmentContext {
        public MediaRouteDialogContext(Context embedderContext) {
            super(ClassLoaderContextWrapperFactory.get(embedderContext));
            // TODO(estade): this is necessary because MediaRouter dialogs crash if the theme has an
            // action bar. It's unclear why this is necessary when it's not in Chrome, and why
            // ContextThemeWrapper doesn't work.
            getTheme().applyStyle(R.style.Theme_BrowserUI_DayNight, /*force=*/true);
        }
    }

    public MediaRouteDialogFragmentImpl() {
        super();
        sInstanceForTest = new WeakReference<MediaRouteDialogFragmentImpl>(this);
    }

    @Override
    public void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);

        // Remove the host fragment as soon as the media router dialog fragment is detached.
        getSupportFragmentManager().registerFragmentLifecycleCallbacks(
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentDetached(FragmentManager fm, Fragment f) {
                        MediaRouteDialogFragmentImpl.this.removeFragmentFromFragmentManager();
                    }
                },
                false);
    }

    @Override
    protected FragmentHostingRemoteFragmentImpl.RemoteFragmentContext createRemoteFragmentContext(
            Context embedderContext) {
        return new MediaRouteDialogContext(embedderContext);
    }

    public IMediaRouteDialogFragment asIMediaRouteDialogFragment() {
        return new IMediaRouteDialogFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return MediaRouteDialogFragmentImpl.this;
            }
        };
    }

    public static MediaRouteDialogFragmentImpl fromRemoteFragment(IRemoteFragment remoteFragment) {
        return (MediaRouteDialogFragmentImpl) remoteFragment;
    }

    public static MediaRouteDialogFragmentImpl getInstanceForTest() {
        return sInstanceForTest.get();
    }
}
