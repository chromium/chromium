// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.text.Layout;
import android.util.AttributeSet;

import androidx.appcompat.widget.AppCompatTextView;

/**
 * A TextView which truncates and displays a URL such that the origin is always visible.
 * The URL can be expanded by clicking on the it.
 */
public class ElidedUrlTextView extends AppCompatTextView {
    // The number of lines to display when the URL is truncated. This number
    // should still allow the origin to be displayed. NULL before
    // setUrlAfterLayout() is called.
    private Integer mTruncatedUrlLinesToDisplay;

    // The number of lines to display when the URL is expanded. This should be enough to display
    // at most two lines of the fragment if there is one in the URL.
    private Integer mFullLinesToDisplay;

    // If true, the text view will show the truncated text. If false, it
    // will show the full, expanded text.
    private boolean mIsShowingTruncatedText = true;

    // The length of the URL's origin in number of characters.
    private int mOriginLength = -1;

    // The maximum number of lines currently shown in the view
    private int mCurrentMaxLines = Integer.MAX_VALUE;

    /** Constructor for inflating from XML. */
    public ElidedUrlTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setMaxLines(int maxlines) {
        super.setMaxLines(maxlines);
        mCurrentMaxLines = maxlines;
    }

    /**
     * Find the number of lines of text which must be shown in order to display the character at
     * a given index.
     */
    private int getLineForIndex(int index) {
        Layout layout = getLayout();
        int endLine = 0;
        while (endLine < layout.getLineCount() && layout.getLineEnd(endLine) < index) {
            endLine++;
        }
        // Since endLine is an index, add 1 to get the number of lines.
        return endLine + 1;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMaxLines(Integer.MAX_VALUE);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        assert mOriginLength >= 0 : "setUrl() must be called before layout.";
        String urlText = getText().toString();

        // Find the range of lines containing the origin.
        int originEndLine = getLineForIndex(mOriginLength);

        // Display an extra line so we don't accidentally hide the origin with
        // ellipses
        mTruncatedUrlLinesToDisplay = originEndLine + 1;

        // Find the line where the fragment starts. Since # is a reserved character, it is safe
        // to just search for the first # to appear in the url.
        int fragmentStartIndex = urlText.indexOf('#');
        if (fragmentStartIndex == -1) fragmentStartIndex = urlText.length();

        int fragmentStartLine = getLineForIndex(fragmentStartIndex);
        mFullLinesToDisplay = fragmentStartLine + 1;

        // If there is no origin (according to OmniboxUrlEmphasizer), make sure the fragment is
        // still hidden correctly.
        if (mFullLinesToDisplay < mTruncatedUrlLinesToDisplay) {
            mTruncatedUrlLinesToDisplay = mFullLinesToDisplay;
        }

        if (updateMaxLines()) super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Sets the URL and the length of the URL's origin.
     * Must be called before layout.
     *
     * @param url The URL.
     * @param originLength The length of the URL's origin in number of characters.
     */
    public void setUrl(CharSequence url, int originLength) {
        assert originLength >= 0 && originLength <= url.length();
        setText(url);
        mOriginLength = originLength;
    }

    /**
     * Toggles truncating/expanding the URL text. If the URL text is not
     * truncated, has no effect.
     */
    public void toggleTruncation() {
        mIsShowingTruncatedText = !mIsShowingTruncatedText;
        if (mFullLinesToDisplay != null) {
            if (updateMaxLines()) {
                announceForAccessibilityOnToggleTruncation(mIsShowingTruncatedText);
            }
        }
    }

    private void announceForAccessibilityOnToggleTruncation(boolean isUrlTruncated) {
        announceForAccessibility(
                getResources()
                        .getString(
                                isUrlTruncated
                                        ? R.string.elided_url_text_view_url_truncated
                                        : R.string.elided_url_text_view_url_expanded));
    }

    private boolean updateMaxLines() {
        int maxLines = mFullLinesToDisplay;
        if (mIsShowingTruncatedText) {
            maxLines = mTruncatedUrlLinesToDisplay;
        }
        if (maxLines != mCurrentMaxLines) {
            setMaxLines(maxLines);
            return true;
        }
        return false;
    }
}
