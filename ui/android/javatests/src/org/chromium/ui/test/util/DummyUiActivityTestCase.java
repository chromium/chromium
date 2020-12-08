// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.support.test.runner.lifecycle.Stage;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;

/**
 * Test case to instrument DummyUiActivity for UI testing scenarios.
 * Recommend to use setUpTest() and tearDownTest() to setup and tear down instead of @Before and
 * @After.
 */
public class DummyUiActivityTestCase {
    private DummyUiActivity mActivity;

    private BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public TestRule ruleChain = RuleChain.outerRule(mActivityTestRule)
                                        .around(new TestDriverRule());

    /**
     * TestRule to setup and tear down for each test.
     */
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

    public DummyUiActivity getActivity() {
        return mActivity;
    }

    public BaseActivityTestRule<DummyUiActivity> getActivityTestRule() {
        return mActivityTestRule;
    }
}
