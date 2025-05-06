// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PorterDuff.Mode;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.appcompat.widget.AppCompatImageButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.ClickWithMetaStateCallback;

// TODO(crbug.com/40883889): See if we still need this class.
/**
 * A subclass of AppCompatImageButton to add workarounds for bugs in Android Framework and Support
 * Library.
 */
@NullMarked
public class ChromeImageButton extends AppCompatImageButton {

    // Used to keep track of meta state so that things like shift+click and ctrl+click can work.
    private int mLastEventMetaState;

    public ChromeImageButton(Context context) {
        super(context);
    }

    public ChromeImageButton(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public ChromeImageButton(Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // ImageView defaults to SRC_ATOP when there's a tint. This interacts poorly with tints that
        // contain alpha, so adjust the default to SRC_IN when this case is found. This will cause
        // the drawable to be mutated, but the tint should already be causing that anyway.
        if (getImageTintList() != null && getImageTintMode() == Mode.SRC_ATOP) {
            setImageTintMode(Mode.SRC_IN);
        }
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        // NOTE: Update `mLastEventMetaState` in anticipation of a potential click.
        // This handles button activations with keyboard. The way it works is as follows:
        // 1) User has keyboard focus on button and lifts finger from activation key (space/enter).
        // 2) This method is triggered.
        // 3) The current meta state is saved.
        // 4) The button is clicked because user lifted finger from activation key in (1).
        // 5) The OnClickListener is called, which calls the callback set in setClickCallback and
        // provides the meta state saved in (3).
        mLastEventMetaState = event.getMetaState();
        return super.onKeyUp(keyCode, event);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        // NOTE: Update `mLastEventMetaState` in anticipation of a potential click.
        // This handles button activations with touch/mouse. The way it works is as follows:
        // 1) User clicks/touches button.
        // 2) This method is triggered.
        // 3) The current meta state is saved.
        // 4) The button is clicked because user clicked button in (1).
        // 5) The OnClickListener is called, which calls the callback set in setClickCallback and
        // provides the meta state saved in (3).
        mLastEventMetaState = event.getMetaState();
        return super.onTouchEvent(event);
    }

    /**
     * Sets the callback to notify of button click events. The callback is provided the meta state
     * of the most recent key/touch event.
     *
     * @param callback the callback to notify.
     */
    public void setClickCallback(@Nullable ClickWithMetaStateCallback callback) {
        setOnClickListener(
                callback != null ? (v) -> callback.onClickWithMeta(mLastEventMetaState) : null);
    }
}
