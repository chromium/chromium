// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.chromium.base.SysUtils;
import org.chromium.ui.R;

/**
 * Toast wrapper, makes sure toasts are not HW accelerated on low-end devices and presented
 * correctly (i.e. use VrToast while in virtual reality).
 *
 * Can (and should) also be used for Chromium-related additions and extensions.
 */
public class Toast {

    public static final int LENGTH_SHORT = android.widget.Toast.LENGTH_SHORT;
    public static final int LENGTH_LONG = android.widget.Toast.LENGTH_LONG;

    private static int sExtraYOffset;

    private android.widget.Toast mToast;
    private ViewGroup mSWLayout;

    public Toast(Context context, View toastView) {
        if (SysUtils.isLowEndDevice()) {
            // Don't HW accelerate Toasts. Unfortunately the only way to do that is to make
            // toast.getView().getContext().getApplicationInfo() return lies to prevent
            // WindowManagerGlobal.addView() from adding LayoutParams.FLAG_HARDWARE_ACCELERATED.
            mSWLayout = new FrameLayout(new ContextWrapper(context) {
                @Override
                public ApplicationInfo getApplicationInfo() {
                    ApplicationInfo info = new ApplicationInfo(super.getApplicationInfo());
                    // On Lollipop the condition we need to fail is
                    // "targetSdkVersion >= Build.VERSION_CODES.LOLLIPOP"
                    // (and for Chrome targetSdkVersion is always the latest)
                    info.targetSdkVersion = Build.VERSION_CODES.KITKAT;

                    // On M+ the condition we need to fail is
                    // "flags & ApplicationInfo.FLAG_HARDWARE_ACCELERATED"
                    info.flags &= ~ApplicationInfo.FLAG_HARDWARE_ACCELERATED;
                    return info;
                }
            });
        }

        mToast = UiWidgetFactory.getInstance().createToast(context);
        setView(toastView);
        mToast.setGravity(
                mToast.getGravity(), mToast.getXOffset(), mToast.getYOffset() + sExtraYOffset);
    }

    public void show() {
        mToast.show();
    }

    public void cancel() {
        mToast.cancel();
    }

    public void setView(View view) {
        if (mSWLayout != null) {
            mSWLayout.removeAllViews();
            if (view != null) {
                mSWLayout.addView(view, WRAP_CONTENT, WRAP_CONTENT);
                mToast.setView(mSWLayout);
            } else {
                // When null view is set we propagate it to the toast to trigger appropriate
                // handling (for example show() throws an exception when view is null).
                mToast.setView(null);
            }
        } else {
            mToast.setView(view);
        }
    }

    public View getView() {
        if (mToast.getView() == null) {
            return null;
        }
        if (mSWLayout != null) {
            return mSWLayout.getChildAt(0);
        } else {
            return mToast.getView();
        }
    }

    public void setDuration(int duration) {
        mToast.setDuration(duration);
    }

    public int getDuration() {
        return mToast.getDuration();
    }

    @SuppressLint("RtlHardcoded")
    private void anchor(Context context, View anchoredView) {
        final int screenWidth = context.getResources().getDisplayMetrics().widthPixels;
        final int screenHeight = context.getResources().getDisplayMetrics().heightPixels;
        final int[] screenPos = new int[2];
        anchoredView.getLocationOnScreen(screenPos);
        final int width = anchoredView.getWidth();
        final int height = anchoredView.getHeight();

        final int horizontalGravity =
                (screenPos[0] < screenWidth / 2) ? Gravity.LEFT : Gravity.RIGHT;
        final int xOffset = (screenPos[0] < screenWidth / 2)
                ? screenPos[0] + width / 2
                : screenWidth - screenPos[0] - width / 2;
        final int yOffset = (screenPos[1] < screenHeight / 2) ? screenPos[1] + height / 2
                                                              : screenPos[1] - height * 3 / 2;
        setGravity(Gravity.TOP | horizontalGravity, xOffset, yOffset);
    }

    public void setMargin(float horizontalMargin, float verticalMargin) {
        mToast.setMargin(horizontalMargin, verticalMargin);
    }

    public float getHorizontalMargin() {
        return mToast.getHorizontalMargin();
    }

    public float getVerticalMargin() {
        return mToast.getVerticalMargin();
    }

    public void setGravity(int gravity, int xOffset, int yOffset) {
        mToast.setGravity(gravity, xOffset, yOffset);
    }

    public int getGravity() {
        return mToast.getGravity();
    }

    public int getXOffset() {
        return mToast.getXOffset();
    }

    public int getYOffset() {
        return mToast.getYOffset();
    }

    @SuppressLint("ShowToast")
    public static Toast makeText(Context context, CharSequence text, int duration) {
        return new Builder(context).withText(text).withDuration(duration).build();
    }

    public static Toast makeText(Context context, int resId, int duration) {
        return new Builder(context).withTextStringRes(resId).withDuration(duration).build();
    }

    /**
     * Shows a toast anchored on a view.
     * @param context The context to use for the toast.
     * @param anchoredView The view to anchor the toast.
     * @param description The string shown in the toast.
     * @return Whether a toast has been shown successfully.
     */
    public static boolean showAnchoredToast(
            Context context, View anchoredView, CharSequence description) {
        return new Builder(context)
                .withAnchoredView(anchoredView)
                .withText(description)
                .buildAndShow();
    }

    /** Builder pattern class to construct {@link Toast} with various arguments. */
    public static class Builder {
        private final Context mContext;
        private CharSequence mText;
        private View mAnchoredView;
        private Integer mBackgroundColor;
        private Integer mTextAppearance;
        private int mDuration = LENGTH_SHORT;

        public Builder(Context context) {
            mContext = context;
        }

        public Builder withText(CharSequence text) {
            mText = text;
            return this;
        }

        public Builder withTextStringRes(@StringRes int textResId) {
            mText = mContext.getResources().getText(textResId);
            return this;
        }

        public Builder withAnchoredView(View anchoredView) {
            mAnchoredView = anchoredView;
            return this;
        }

        public Builder withBackgroundColor(@ColorInt int backgroundColor) {
            mBackgroundColor = backgroundColor;
            return this;
        }

        public Builder withTextAppearance(@StyleRes int textAppearance) {
            mTextAppearance = textAppearance;
            return this;
        }

        public Builder withDuration(int duration) {
            mDuration = duration;
            return this;
        }

        private TextView inflateTextView() {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            TextView textView = (TextView) inflater.inflate(R.layout.custom_toast_layout, null);
            if (mText != null) {
                textView.setText(mText);
                textView.announceForAccessibility(mText);
            }
            if (mBackgroundColor != null) {
                textView.getBackground().setTint(mBackgroundColor);
            }
            if (mTextAppearance != null) {
                textView.setTextAppearance(mTextAppearance);
            }
            return textView;
        }

        /** Builds and returns a toast, but does not show it. */
        public Toast build() {
            TextView textView = inflateTextView();
            Toast toast = new Toast(mContext, textView);
            if (mAnchoredView != null) {
                toast.anchor(mContext, mAnchoredView);
            }
            toast.setDuration(mDuration);
            return toast;
        }

        /** Builds and shows a toast, returning if it was successful. */
        public boolean buildAndShow() {
            if (mText == null) return false;
            build().show();
            return true;
        }
    }

    /**
     * Set extra Y offset for toasts all toasts created with this class. This can be overridden by
     * calling {@link Toast#setGravity(int, int, int)} on an individual toast.
     * @param yOffsetPx The Y offset from the normal toast position in px.
     */
    public static void setGlobalExtraYOffset(int yOffsetPx) {
        sExtraYOffset = yOffsetPx;
    }
}
