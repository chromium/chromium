// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Build;
import android.view.View;

import androidx.annotation.RequiresApi;
import androidx.fragment.app.FragmentManager;

import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.IntentRequestTracker.Delegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/**
 * Implements intent sending for a fragment based window. This should be created when
 * onAttach() is called on the fragment, and destroyed when onDetach() is called.
 */
public class FragmentWindowAndroid extends WindowAndroid {
    private final BrowserFragmentImpl mFragment;
    private ModalDialogManager mModalDialogManager;

    /**
     * WebLayer's implementation of the delegate of a IntentRequestTracker.
     */
    private static class TrackerDelegateImpl implements Delegate {
        private final RemoteFragmentImpl mFragment;
        // This WeakReference is purely to avoid gc churn of creating a new WeakReference in
        // every getActivity call. It is not needed for correctness.
        private ImmutableWeakReference<Activity> mActivityWeakRefHolder;

        /**
         * Create an instance of delegate for the given fragment that will own the
         * IntentRequestTracker.
         * @param fragment The fragment that owns the IntentRequestTracker.
         */
        private TrackerDelegateImpl(RemoteFragmentImpl fragment) {
            mFragment = fragment;
        }

        @Override
        public boolean startActivityForResult(Intent intent, int requestCode) {
            return mFragment.startActivityForResult(intent, requestCode, null);
        }

        @Override
        public boolean startIntentSenderForResult(IntentSender intentSender, int requestCode) {
            return mFragment.startIntentSenderForResult(
                    intentSender, requestCode, new Intent(), 0, 0, 0, null);
        }

        @Override
        public void finishActivity(int requestCode) {
            Activity activity = getActivity().get();
            if (activity == null) return;
            activity.finishActivity(requestCode);
        }

        @Override
        public final WeakReference<Activity> getActivity() {
            if (mActivityWeakRefHolder == null
                    || mActivityWeakRefHolder.get() != mFragment.getActivity()) {
                mActivityWeakRefHolder = new ImmutableWeakReference<>(mFragment.getActivity());
            }
            return mActivityWeakRefHolder;
        }
    }

    /* package */ FragmentWindowAndroid(Context context, BrowserFragmentImpl fragment) {
        super(context, IntentRequestTracker.createFromDelegate(new TrackerDelegateImpl(fragment)));
        mFragment = fragment;

        setKeyboardDelegate(new ActivityKeyboardVisibilityDelegate(getActivity()));
        setAndroidPermissionDelegate(new FragmentAndroidPermissionDelegate(mFragment));
    }

    @Override
    public final WeakReference<Activity> getActivity() {
        return getIntentRequestTracker().getActivity();
    }

    @Override
    public final ModalDialogManager getModalDialogManager() {
        return mModalDialogManager;
    }

    @Override
    public View getReadbackView() {
        BrowserViewController viewController = mFragment.getPossiblyNullViewController();
        if (viewController == null) return null;
        return viewController.getViewForMagnifierReadback();
    }

    public void setModalDialogManager(ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
    }

    public BrowserFragmentImpl getFragment() {
        return mFragment;
    }

    public FragmentManager getFragmentManager() {
        return mFragment.getSupportFragmentManager();
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.O)
    public void setWideColorEnabled(boolean enabled) {
        // WebLayer should not change its behavior when the content contains wide color.
        // Rather, the app embedding the WebLayer gets to choose whether or not it is wide.
        // So we should do nothing in this override.
    }
}
