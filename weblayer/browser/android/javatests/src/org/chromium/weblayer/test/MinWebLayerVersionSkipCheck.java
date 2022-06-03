// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.test.util.AnnotationProcessingUtils;
import org.chromium.base.test.util.SkipCheck;

/**
 * Checks the WebLayer version against any specified minimum requirement.
 */
public class MinWebLayerVersionSkipCheck extends SkipCheck {
    private static final String TAG = "MinWebLayerVersionSC";

    /**
     * If {@link MinWebLayerVersion} is present, checks its value
     * against the WebLayer version.
     *
     * @param testCase The test to check.
     * @return true if WebLayer's version is below the specified minimum.
     */
    @Override
    public boolean shouldSkip(FrameworkMethod frameworkMethod) {
        int minWebLayerVersion = 0;
        for (MinWebLayerVersion m : AnnotationProcessingUtils.getAnnotations(
                     frameworkMethod.getMethod(), MinWebLayerVersion.class)) {
            minWebLayerVersion = Math.max(minWebLayerVersion, m.value());
        }
        String stringVersion = CommandLine.getInstance().getSwitchValue("impl-version", "-1");
        int version = Integer.valueOf(stringVersion);
        if (version != -1 && version < minWebLayerVersion) {
            Log.i(TAG,
                    "Test " + frameworkMethod.getDeclaringClass().getName() + "#"
                            + frameworkMethod.getName() + " is not enabled at WebLayer version "
                            + version + ".");
            return true;
        }
        return false;
    }
}
