// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;

import java.lang.annotation.Retention;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.CheckReturnValue;

/** Collection of utilities helping to clarify expectations on views in tests. */
public class ViewUtils {
    @Retention(SOURCE)
    @IntDef(
            flag = true,
            value = {VIEW_VISIBLE, VIEW_INVISIBLE, VIEW_GONE, VIEW_NULL})
    public @interface ExpectedViewState {}

    public static final int VIEW_VISIBLE = 1;
    public static final int VIEW_INVISIBLE = 1 << 1;
    public static final int VIEW_GONE = 1 << 2;
    public static final int VIEW_NULL = 1 << 3;

    private static class ExpectedViewCriteria implements Runnable {
        private final Matcher<View> mViewMatcher;
        private final @ExpectedViewState int mViewState;
        private final ViewGroup mRootView;

        ExpectedViewCriteria(
                Matcher<View> viewMatcher, @ExpectedViewState int viewState, ViewGroup rootView) {
            mViewMatcher = viewMatcher;
            mViewState = viewState;
            mRootView = rootView;
        }

        @Override
        public void run() {
            List<View> matchedViews = new ArrayList<>();
            findMatchingChildren(mRootView, matchedViews);
            Assert.assertThat(matchedViews, not(hasSize(greaterThan(1))));
            assertViewExpectedState(matchedViews.size() == 0 ? null : matchedViews.get(0));
        }

        private void findMatchingChildren(ViewGroup root, List<View> matchedViews) {
            for (int i = 0; i < root.getChildCount(); i++) {
                View view = root.getChildAt(i);
                if (mViewMatcher.matches(view)) matchedViews.add(view);
                if (view instanceof ViewGroup) {
                    findMatchingChildren((ViewGroup) view, matchedViews);
                }
            }
        }

        private void assertViewExpectedState(View view) {
            if (view == null) {
                Criteria.checkThat(
                        "No view found to match: " + mViewMatcher.toString(),
                        (mViewState & VIEW_NULL) != 0,
                        is(true));
                return;
            }

            switch (view.getVisibility()) {
                case View.VISIBLE:
                    Criteria.checkThat(
                            "View matching '"
                                    + mViewMatcher.toString()
                                    + "' is unexpectedly visible!",
                            (mViewState & VIEW_VISIBLE) != 0,
                            is(true));
                    break;
                case View.INVISIBLE:
                    Criteria.checkThat(
                            "View matching '"
                                    + mViewMatcher.toString()
                                    + "' is unexpectedly invisible!",
                            (mViewState & VIEW_INVISIBLE) != 0,
                            is(true));
                    break;
                case View.GONE:
                    Criteria.checkThat(
                            "View matching '" + mViewMatcher.toString() + "' is unexpectedly gone!",
                            (mViewState & VIEW_GONE) != 0,
                            is(true));
                    break;
            }
        }
    }

    private ViewUtils() {}

    /**
     * Waits until a view matching the given matches any of the given {@link ExpectedViewState}s.
     * Fails if the matcher applies to multiple views. Times out after
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    public static void waitForView(
            ViewGroup root, Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        CriteriaHelper.pollUiThread(new ExpectedViewCriteria(viewMatcher, viewState, root));
    }

    /**
     * Waits until a view in a dialog root view that matches the given matcher and any of the given
     * {@link ExpectedViewState}s. waitForViewCheckingState is flaky with dialogs after api 29.
     * Fails if the matcher applies to multiple views. Times out after {@link
     * CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *     the waiting will stop if at least one applies.
     */
    public static void waitForDialogViewCheckingState(
            Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        onView(isRoot())
                .inRoot(isDialog())
                .check(withEventualExpectedViewState(viewMatcher, viewState));
    }

    /**
     * Waits until a view matches the given matcher and any of the given {@link ExpectedViewState}s.
     * Fails if the matcher applies to multiple views. Times out after
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    public static void waitForViewCheckingState(
            Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        onView(isRoot()).check(withEventualExpectedViewState(viewMatcher, viewState));
    }

    /**
     * Waits until a visible view matches the given matcher. Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static void waitForVisibleView(Matcher<View> viewMatcher) {
        waitForViewCheckingState(viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Returns a ViewAssertion that, upon being called, polls until either a view matches both the
     * given |viewMatcher| and any of the given {@link ExpectedViewState}s - or the polling times
     * out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * Calling this and ignoring the return value does not do anything. This should be used on
     * {@link ViewInteraction#check} with a {@link ViewGroup}. For example,
     * the following usage assumes the root view is a {@link ViewGroup}.
     * <pre>
     *   onView(isRoot()).check(withEventualExpectedViewState(withId(R.id.example_id), VIEW_GONE));
     * </pre>
     *
     * A convenience method is provided equivalent to the exact code above:
     * <pre>
     *   waitForViewCheckingState(withId(R.id.example_id), VIEW_GONE);
     * </pre>
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    @CheckReturnValue
    public static ViewAssertion withEventualExpectedViewState(
            Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        return (View view, NoMatchingViewException noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            CriteriaHelper.pollUiThreadNested(
                    new ExpectedViewCriteria(viewMatcher, viewState, (ViewGroup) view));
        };
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL}
     * milliseconds.
     *
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static void waitForView(ViewGroup root, Matcher<View> viewMatcher) {
        waitForView(root, viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL}
     * milliseconds.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    @CheckReturnValue
    public static ViewAssertion isEventuallyVisible(Matcher<View> viewMatcher) {
        return withEventualExpectedViewState(viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Waits until a visible view matching the given matcher Fails if the matcher applies to
     * multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     *
     * <p>Android API 30+ tests are flakey with espresso 3.2 without the inRoot(isDialog()) check.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(
            Matcher<View> viewMatcher, boolean checkRootDialog) {
        Runnable r =
                () -> {
                    onView(isRoot())
                            .check(
                                    (View view, NoMatchingViewException noMatchException) -> {
                                        if (noMatchException != null) throw noMatchException;
                                        new ExpectedViewCriteria(
                                                        viewMatcher, VIEW_VISIBLE, (ViewGroup) view)
                                                .run();
                                    });
                };
        // Needed to prevent flakiness with espresso 3.2 after API 29.
        if (checkRootDialog) {
            r =
                    () -> {
                        onView(isRoot())
                                .inRoot(isDialog())
                                .check(
                                        (View view, NoMatchingViewException noMatchException) -> {
                                            if (noMatchException != null) throw noMatchException;
                                            new ExpectedViewCriteria(
                                                            viewMatcher,
                                                            VIEW_VISIBLE,
                                                            (ViewGroup) view)
                                                    .run();
                                        });
                    };
        }

        CriteriaHelper.pollInstrumentationThread(r);
        return onView(viewMatcher);
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views. Times out after {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL}
     * milliseconds.
     *
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(Matcher<View> viewMatcher) {
        return onViewWaiting(viewMatcher, false);
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
