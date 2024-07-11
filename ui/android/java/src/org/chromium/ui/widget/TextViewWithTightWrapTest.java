// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.R;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for {@link TextViewWithTightWrap}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TextViewWithTightWrapTest extends BlankUiTestActivityTestCase {
    private static final int RENDER_TEST_REVISION = 2;
    private static final String RENDER_TEST_REVISION_DESCRIPTION = "Update the text style.";

    private TextViewWithTightWrap mTextView;
    private View mView;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_REVISION_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .build();

    @Before
    public void setup() {
        // Setup the UI.
        Activity activity = getActivity();
        mView = LayoutInflater.from(activity).inflate(R.layout.textbubble_text, null, false);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mTextView = mView.findViewById(R.id.message);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setBackgroundColor(activity.getColor(R.color.filled_button_bg));
                    mTextView.setText("First line\nVery very very very long second line");
                    getActivity().setContentView(mView, params);
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testBasicTextView() throws Exception {
        // Render UI Elements.
        mRenderTestRule.render(mView, "TextViewWithTightWrap_Basic");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testWrapContentTextView() throws Exception {
        LayoutParams params = mTextView.getLayoutParams();
        params.height = LayoutParams.WRAP_CONTENT;
        params.width = LayoutParams.WRAP_CONTENT;
        // Render UI Elements.
        mRenderTestRule.render(mView, "TextViewWithTightWrap_WrapContent");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testMatchParentTextView() throws Exception {
        LayoutParams params = mTextView.getLayoutParams();
        params.height = LayoutParams.MATCH_PARENT;
        params.width = LayoutParams.MATCH_PARENT;
        // Render UI Elements.
        mRenderTestRule.render(mView, "TextViewWithTightWrap_MatchParent");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testTextViewWithSnooze() throws Exception {
        Button snoozeButton = (Button) mView.findViewById(R.id.button_snooze);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    snoozeButton.setVisibility(View.VISIBLE);
                });
        // Render UI Elements.
        mRenderTestRule.render(mView, "TextViewWithTightWrap_MatchParent_WithSnooze");
    }
}
