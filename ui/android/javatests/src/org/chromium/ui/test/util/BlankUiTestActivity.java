// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.LayoutRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/** Simplified activity to test UI components without Chrome browser initialization and natives. */
public class BlankUiTestActivity extends AppCompatActivity implements ModalDialogManagerHolder {
    private static int sTestTheme;
    private static int sTestLayout;

    private final ModalDialogManager mModalDialogManager =
            new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);

    private Callback<MotionEvent> mMotionEventCallback;
    private Callback<KeyEvent> mKeyEventCallback;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (sTestTheme != 0) {
            setTheme(sTestTheme);
        }
        if (sTestLayout != 0) {
            setContentView(sTestLayout);
        }
    }

    @Override
    public void onDestroy() {
        mModalDialogManager.destroy();
        super.onDestroy();
    }

    /** Required to make preference fragments use InMemorySharedPreferences in tests. */
    @Override
    public SharedPreferences getSharedPreferences(String name, int mode) {
        return ContextUtils.getApplicationContext().getSharedPreferences(name, mode);
    }

    /**
     * Set the base theme for the dummy activity. Note that you can also call mActivity.setTheme()
     * in test code later if you want to set theme after activity launched.
     * @param resid The style resource describing the theme.
     */
    public static void setTestTheme(@StyleRes int resid) {
        sTestTheme = resid;
    }

    /**
     * Set the activity content from a layout resource. Note that you can also call
     * mActivity.setContentView() in test code later if you want to set content view after activity
     * launched.
     * @param layoutResID Resource ID to be inflated.
     */
    public static void setTestLayout(@LayoutRes int layoutResID) {
        sTestLayout = layoutResID;
    }

    // Implementation of ModalDialogManagerHolder
    @Override
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManager;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (mMotionEventCallback != null) mMotionEventCallback.onResult(event);
        return super.dispatchTouchEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mKeyEventCallback != null) mKeyEventCallback.onResult(event);
        return super.dispatchKeyEvent(event);
    }

    /**
     * Registers a callback for getting a stream of touch events prior to being dispatched to the
     * view tree.
     */
    public void setTouchEventCallback(Callback<MotionEvent> callback) {
        mMotionEventCallback = callback;
    }

    /**
     * Registers a callback for getting a stream of key events prior to being dispatched to the
     * view tree.
     */
    public void setKeyEventCallback(Callback<KeyEvent> callback) {
        mKeyEventCallback = callback;
    }
}
