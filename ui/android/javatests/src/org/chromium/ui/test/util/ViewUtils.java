// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewFinder;
import org.chromium.base.test.transit.ViewPresence;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;

/** Collection of utilities helping to clarify expectations on views in tests. */
public class ViewUtils {
    private ViewUtils() {}

    /**
     * Waits until a visible view matches the given matcher. Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * <p>By default, also waits for the view to be displayed >= 51% and enabled.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static void waitForVisibleView(Matcher<View> viewMatcher) {
        ViewFinder.waitForView(viewMatcher);
    }

    /**
     * Waits until a visible view matches the given matcher. Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static void waitForView(ViewGroup root, Matcher<View> viewMatcher) {
        ViewFinder.waitForView(allOf(viewMatcher, isDescendantOfA(is(root))));
    }

    /**
     * Waits until a visible view matches the given matcher. Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * <p>By default, also waits for the view to be displayed >= 51% and enabled.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param options The options to override expectations for the View (e.g. displayed %).
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(
            Matcher<View> viewMatcher, ViewElement.Options options) {
        ViewPresence<View> viewPresence = ViewFinder.waitForView(viewMatcher, options);
        return viewPresence.onView();
    }

    /**
     * Waits until a visible view matches the given matcher. Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * <p>By default, also waits for the view to be displayed >= 51% and enabled.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(
            Matcher<View> viewMatcher, boolean checkRootDialog) {
        ViewElement.Options.Builder optionsBuilder = ViewElement.newOptions();
        if (checkRootDialog) {
            optionsBuilder = optionsBuilder.inDialog();
        }
        ViewPresence<View> viewPresence =
                ViewFinder.waitForView(viewMatcher, optionsBuilder.build());
        return viewPresence.onView();
    }

    /**
     * Waits until a visible view matches the given matcher. Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * <p>By default, also waits for the view to be displayed >= 51% and enabled.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(Matcher<View> viewMatcher) {
        ViewPresence<View> viewPresence = ViewFinder.waitForView(viewMatcher);
        return viewPresence.onView();
    }

    /**
     * Wait until the specified view has finished layout updates.
     *
     * @param view The specified view.
     */
    public static void waitForStableView(final View view) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat("The view is dirty.", view.isDirty(), is(false));
                    Criteria.checkThat(
                            "The view has layout requested.", view.isLayoutRequested(), is(false));
                });
    }

    public static MotionEvent createMotionEvent(float x, float y) {
        return MotionEvent.obtain(0, 0, 0, x, y, 0);
    }

    /**
     * Creates a view matcher for the given color resource id.
     *
     * @param colorResId The color resource id to be tested against the view.for the search engine
     *         icon.
     * @return Returns the view matcher.
     */
    public static Matcher<View> hasBackgroundColor(final int colorResId) {
        return new BoundedMatcher<View, View>(View.class) {
            private Context mContext;

            @Override
            protected boolean matchesSafely(View imageView) {
                this.mContext = imageView.getContext();
                Drawable background = imageView.getBackground();
                if (!(background instanceof ColorDrawable)) return false;
                int expectedColor =
                        AppCompatResources.getColorStateList(mContext, colorResId)
                                .getDefaultColor();
                return ((ColorDrawable) background).getColor() == expectedColor;
            }

            @Override
            public void describeTo(Description description) {
                String colorId = String.valueOf(colorResId);
                if (this.mContext != null) {
                    colorId = this.mContext.getResources().getResourceName(colorResId);
                }

                description.appendText("has background color with ID " + colorId);
            }
        };
    }

    /**
     * Creates a {@link ViewAction} to click on a text view with one or more clickable spans.
     *
     * @param spanIndex Index of clickable span to click on.
     * @return A {@link ViewAction} on the matching view.
     */
    public static ViewAction clickOnClickableSpan(int spanIndex) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on a specified link in a text view with clickable spans";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length == 0) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                Assert.assertTrue("Span index out of bounds", spans.length > spanIndex);
                spans[spanIndex].onClick(view);
            }
        };
    }
}
