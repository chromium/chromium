// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Build;
import android.view.View;

import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.IntentWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/**
 * Implements intent sending for a fragment based window. This should be created when
 * onAttach() is called on the fragment, and destroyed when onDetach() is called.
 */
public class FragmentWindowAndroid extends IntentWindowAndroid {
    private BrowserFragmentImpl mFragment;
    private ModalDialogManager mModalDialogManager;

    // This WeakReference is purely to avoid gc churn of creating a new WeakReference in
    // every getActivity call. It is not needed for correctness.
    private ImmutableWeakReference<Activity> mActivityWeakRefHolder;

    FragmentWindowAndroid(Context context, BrowserFragmentImpl fragment) {
        super(context);
        mFragment = fragment;

        setKeyboardDelegate(new ActivityKeyboardVisibilityDelegate(getActivity()));
        setAndroidPermissionDelegate(new FragmentAndroidPermissionDelegate(mFragment));
    }

    @Override
    protected final boolean startIntentSenderForResult(IntentSender intentSender, int requestCode) {
        return mFragment.startIntentSenderForResult(
                intentSender, requestCode, new Intent(), 0, 0, 0, null);
    }

    @Override
    protected final boolean startActivityForResult(Intent intent, int requestCode) {
        return mFragment.startActivityForResult(intent, requestCode, null);
    }

    @Override
    public final WeakReference<Activity> getActivity() {
        if (mActivityWeakRefHolder == null
                || mActivityWeakRefHolder.get() != mFragment.getActivity()) {
            mActivityWeakRefHolder = new ImmutableWeakReference<>(mFragment.getActivity());
        }
        return mActivityWeakRefHolder;
    }

    @Override
    public final ModalDialogManager getModalDialogManager() {
        return mModalDialogManager;
    }

    @Override
    public View getReadbackView() {
        BrowserViewController viewController = getBrowser().getPossiblyNullViewController();
        if (viewController == null) return null;
        return viewController.getViewForMagnifierReadback();
    }

    public void setModalDialogManager(ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
    }

    public BrowserImpl getBrowser() {
        return mFragment.getBrowser();
    }

    @Override
    @TargetApi(Build.VERSION_CODES.O)
    public void setWideColorEnabled(boolean enabled) {
        // WebLayer should not change its behavior when the content contains wide color.
        // Rather, the app embedding the WebLayer gets to choose whether or not it is wide.
        // So we should do nothing in this override.
    }
}
