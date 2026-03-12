// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PorterDuff.Mode;
import android.util.AttributeSet;
import android.view.InputDevice;
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

    // Used to keep track of button state so that things like middle click can work.
    private int mLastEventButtonState;

    private @Nullable ClickWithMetaStateCallback mClickCallback;

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
        mLastEventButtonState = 0;
        return super.onKeyUp(keyCode, event);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        // NOTE: Update `mLastEventMetaState` and `mLastEventButtonState` in anticipation of a
        // potential click. This handles button activations with touch/mouse. The way it works is as
        // follows:
        // 1) User clicks/touches button.
        // 2) This method is triggered.
        // 3) The current meta state and button state are saved.
        // 4) The button is clicked because user clicked button in (1).
        // 5) The OnClickListener is called, which calls the callback set in setClickCallback and
        // provides the meta state and button state saved in (3).
        mLastEventMetaState = event.getMetaState();

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_BUTTON_PRESS) {
            mLastEventButtonState = event.getButtonState();
        }

        // Consume middle-clicks to avoid triggering the OnClickListener. Without this, a
        // middle-click (e.g. from a mouse) would trigger both the middle-click specific action
        // (handled in onGenericMotionEvent) and the standard click action (triggered by
        // super.onTouchEvent), leading to duplicate or conflicting behaviors.
        if ((mLastEventButtonState & MotionEvent.BUTTON_TERTIARY) != 0) {
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                mLastEventButtonState = 0;
            }
            return true;
        }

        return super.onTouchEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                && event.getActionMasked() == MotionEvent.ACTION_BUTTON_PRESS
                && event.getActionButton() == MotionEvent.BUTTON_TERTIARY) {
            if (mClickCallback != null) {
                mClickCallback.onClickWithMeta(event.getMetaState(), MotionEvent.BUTTON_TERTIARY);
            }
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    /**
     * Sets the callback to notify of button click events. The callback is provided the meta state
     * and button state of the most recent key/touch event.
     *
     * @param callback the callback to notify.
     */
    public void setClickCallback(@Nullable ClickWithMetaStateCallback callback) {
        mClickCallback = callback;
        setOnClickListener(
                callback != null
                        ? (v) ->
                                callback.onClickWithMeta(mLastEventMetaState, mLastEventButtonState)
                        : null);
    }
}
