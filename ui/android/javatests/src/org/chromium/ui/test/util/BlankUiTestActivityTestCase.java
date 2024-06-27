// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import androidx.test.runner.lifecycle.Stage;

import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;

/**
 * Test case to instrument BlankUiTestActivity for UI testing scenarios.
 * Recommend to use setUpTest() and tearDownTest() to setup and tear down instead of @Before and
 * @After.
 */
public class BlankUiTestActivityTestCase {
    private BlankUiTestActivity mActivity;

    private final BaseActivityTestRule<? extends BlankUiTestActivity> mActivityTestRule;

    // Disable animations to reduce flakiness.
    @Rule public final TestRule ruleChain;

    /** Default constructor that creates a {@link BlankUiTestActivity} as expected. */
    public BlankUiTestActivityTestCase() {
        this(new BaseActivityTestRule<BlankUiTestActivity>(BlankUiTestActivity.class));
    }

    /**
     * Constructor to allow subclasses to inject activity and rule subclasses.
     * @param activityTestRule Injected rule to use for activity interactions.
     */
    protected BlankUiTestActivityTestCase(
            BaseActivityTestRule<? extends BlankUiTestActivity> activityTestRule) {
        mActivityTestRule = activityTestRule;
        ruleChain = RuleChain.outerRule(mActivityTestRule).around(new TestDriverRule());
    }

    /** TestRule to setup and tear down for each test. */
    public final class TestDriverRule implements TestRule {
        @Override
        public Statement apply(final Statement base, Description description) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    beforeActivityLaunch();
                    mActivityTestRule.launchActivity(null);
                    ApplicationTestUtils.waitForActivityState(
                            mActivityTestRule.getActivity(), Stage.RESUMED);
                    setUpTest();
                    try {
                        base.evaluate();
                    } finally {
                        tearDownTest();
                    }
                }
            };
        }
    }

    public void beforeActivityLaunch() throws Exception {}

    // Override this to setup before test.
    public void setUpTest() throws Exception {
        mActivity = mActivityTestRule.getActivity();
    }

    // Override this to tear down after test.
    public void tearDownTest() throws Exception {}

    public BlankUiTestActivity getActivity() {
        return mActivity;
    }
}
