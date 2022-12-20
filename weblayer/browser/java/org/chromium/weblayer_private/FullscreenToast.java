// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.Gravity;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.ui.widget.Toast;

/**
 * FullscreenToast is responsible for showing toast when fullscreen mode is entered. As the embedder
 * is responsible for entering fullscreen mode, there is no guarantee when or if fullscreen mode
 * will be entered. This waits for the system to enter fullscreen mode and then show the toast. If
 * fullscreen isn't entered after a short delay this assumes the embedder won't enter fullscreen
 * and the toast is never shown.
 */
public final class FullscreenToast {
    // The tab the toast is showing from.
    private TabImpl mTab;

    // View used to register for system ui change notification.
    private ContentView mView;

    private View.OnSystemUiVisibilityChangeListener mSystemUiVisibilityChangeListener;

    // Set to true once toast is shown.
    private boolean mDidShowToast;

    // The toast.
    private Toast mToast;

    FullscreenToast(TabImpl tab) {
        mTab = tab;
        // TODO(https://crbug.com/1130096): This should really be handled lower down in the stack.
        if (tab.getBrowser().getActiveTab() != tab) return;
        addSystemUiChangedObserver();
    }

    @VisibleForTesting
    public boolean didShowFullscreenToast() {
        return mDidShowToast;
    }

    public void destroy() {
        // This may be called more than once.
        if (mTab == null) return;

        if (mSystemUiVisibilityChangeListener != null) {
            // mSystemUiVisibilityChangeListener is only installed if mView is non-null.
            assert mView != null;
            mView.removeOnSystemUiVisibilityChangeListener(mSystemUiVisibilityChangeListener);
            mSystemUiVisibilityChangeListener = null;
        }
        mTab = null;
        mView = null;
        if (mToast != null) {
            mToast.cancel();
            mToast = null;
        }
    }

    private void addSystemUiChangedObserver() {
        if (mTab.getBrowser().getBrowserFragment().getViewAndroidDelegateContainerView() == null) {
            return;
        }
        mView = mTab.getBrowser().getBrowserFragment().getViewAndroidDelegateContainerView();
        mSystemUiVisibilityChangeListener = new View.OnSystemUiVisibilityChangeListener() {
            @Override
            public void onSystemUiVisibilityChange(int visibility) {
                // The listener should have been removed if destroy() was called.
                assert mTab != null;
                if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                    // No longer in fullscreen. Destroy.
                    destroy();
                } else if ((visibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) != 0
                        && !mDidShowToast) {
                    // Only show the toast when navigation is hidden and toast wasn't already shown.
                    showToast();
                    mDidShowToast = true;
                }
            }
        };
        mView.addOnSystemUiVisibilityChangeListener(mSystemUiVisibilityChangeListener);
        // See class description for details on why a timeout is used.
        mView.postDelayed(() -> {
            if (!mDidShowToast) destroy();
        }, 1000);
    }

    private void showToast() {
        assert mToast == null;
        mDidShowToast = true;
        int resId = R.string.immersive_fullscreen_api_notification;
        mToast = Toast.makeText(mView.getContext(), resId, Toast.LENGTH_LONG);
        mToast.setGravity(Gravity.TOP | Gravity.CENTER, 0, 0);
        mToast.show();
    }
}
