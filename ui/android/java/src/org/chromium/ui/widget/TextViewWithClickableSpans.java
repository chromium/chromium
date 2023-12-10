// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.text.Layout;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.PopupMenu;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.accessibility.AccessibilityState;

/**
 * ClickableSpan isn't accessible by default, so we create a subclass of TextView that tries to
 * handle the case where a user clicks on a view and not directly on one of the clickable spans. We
 * do nothing if it's a touch event directly on a ClickableSpan. Otherwise if there's only one
 * ClickableSpan, we activate it. If there's more than one, we pop up a PopupMenu to disambiguate.
 */
public class TextViewWithClickableSpans extends TextViewWithLeading
        implements View.OnLongClickListener {
    private PopupMenu mDisambiguationMenu;

    public TextViewWithClickableSpans(Context context) {
        super(context);
        init();
    }

    public TextViewWithClickableSpans(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        // This disables the saving/restoring since the saved text may be in the wrong language
        // (if the user just changed system language), and restoring spans doesn't work anyway.
        // See crbug.com/533362
        setSaveEnabled(false);
        setOnLongClickListener(this);
    }

    @Override
    public boolean onLongClick(View v) {
        assert v == this;
        if (!AccessibilityState.isTouchExplorationEnabled()) {
            // If no accessibility services that requested touch exploration are enabled, then this
            // view should not consume the long click action.
            return false;
        }
        openDisambiguationMenu();
        return true;
    }

    @Override
    public final void setOnLongClickListener(View.OnLongClickListener listener) {
        // Ensure that no one changes the long click listener to anything but this view.
        assert listener == this || listener == null;
        super.setOnLongClickListener(listener);
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        // BrailleBack will generate an accessibility click event directly
        // on this view, make sure we handle that correctly.
        if (action == AccessibilityNodeInfo.ACTION_CLICK) {
            handleAccessibilityClick();
            return true;
        }
        return super.performAccessibilityAction(action, arguments);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        boolean superResult = super.onTouchEvent(event);

        if (event.getAction() != MotionEvent.ACTION_UP
                && AccessibilityState.isTouchExplorationEnabled()
                && !touchIntersectsAnyClickableSpans(event)) {
            handleAccessibilityClick();
            return true;
        }

        return superResult;
    }

    /**
     * Determines whether the motion event intersects with any of the ClickableSpan(s) within the
     * text.
     *
     * @param event The motion event to compare the spans against.
     * @return Whether the motion event intersected any clickable spans.
     */
    protected boolean touchIntersectsAnyClickableSpans(MotionEvent event) {
        // This logic is borrowed from android.text.method.LinkMovementMethod.
        //
        // ClickableSpan doesn't stop propagation of the event in its click handler,
        // so we should only try to simplify clicking on a clickable span if the touch event
        // isn't already over a clickable span.

        CharSequence text = getText();
        if (!(text instanceof SpannableString)) return false;
        SpannableString spannable = (SpannableString) text;

        int x = (int) event.getX();
        int y = (int) event.getY();

        x -= getTotalPaddingLeft();
        y -= getTotalPaddingTop();

        x += getScrollX();
        y += getScrollY();

        Layout layout = getLayout();
        int line = layout.getLineForVertical(y);
        int off = layout.getOffsetForHorizontal(line, x);

        ClickableSpan[] clickableSpans = spannable.getSpans(off, off, ClickableSpan.class);
        return clickableSpans.length > 0;
    }

    /** Returns the ClickableSpans in this TextView's text. */
    @VisibleForTesting
    public ClickableSpan[] getClickableSpans() {
        CharSequence text = getText();
        if (!(text instanceof SpannableString)) return null;

        SpannableString spannable = (SpannableString) text;
        return spannable.getSpans(0, spannable.length(), ClickableSpan.class);
    }

    private void handleAccessibilityClick() {
        ClickableSpan[] clickableSpans = getClickableSpans();
        if (clickableSpans == null || clickableSpans.length == 0) {
            return;
        } else if (clickableSpans.length == 1) {
            clickableSpans[0].onClick(this);
        } else {
            openDisambiguationMenu();
        }
    }

    private void openDisambiguationMenu() {
        ClickableSpan[] clickableSpans = getClickableSpans();
        if (clickableSpans == null || clickableSpans.length == 0 || mDisambiguationMenu != null) {
            return;
        }

        SpannableString spannable = (SpannableString) getText();
        mDisambiguationMenu = new PopupMenu(getContext(), this);
        Menu menu = mDisambiguationMenu.getMenu();
        for (final ClickableSpan clickableSpan : clickableSpans) {
            CharSequence itemText =
                    spannable.subSequence(
                            spannable.getSpanStart(clickableSpan),
                            spannable.getSpanEnd(clickableSpan));
            MenuItem menuItem = menu.add(itemText);
            menuItem.setOnMenuItemClickListener(
                    new MenuItem.OnMenuItemClickListener() {
                        @Override
                        public boolean onMenuItemClick(MenuItem menuItem) {
                            clickableSpan.onClick(TextViewWithClickableSpans.this);
                            return true;
                        }
                    });
        }

        mDisambiguationMenu.setOnDismissListener(
                new PopupMenu.OnDismissListener() {
                    @Override
                    public void onDismiss(PopupMenu menu) {
                        mDisambiguationMenu = null;
                    }
                });
        mDisambiguationMenu.show();
    }
}
