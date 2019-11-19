// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;

import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.IntentWindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Implements intent sending for a fragment based window. This should be created when
 * onAttach() is called on the fragment, and destroyed when onDetach() is called.
 */
public class FragmentWindowAndroid extends IntentWindowAndroid {
    private BrowserFragmentImpl mFragment;

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
        return new WeakReference<>(mFragment.getActivity());
    }
}
