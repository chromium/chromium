// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;

/** Unit tests for {@link DragAndDropPermissionsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DragAndDropPermissionsUtilsUnitTest {
    /** Activity that returns canned results from requestDragAndDropPermissions(). */
    public static class TestActivity extends Activity {
        @Nullable DragAndDropPermissions mPermissions;
        boolean mThrowSecurityException;

        @Override
        public @Nullable DragAndDropPermissions requestDragAndDropPermissions(DragEvent event) {
            if (mThrowSecurityException) throw new SecurityException("Source cannot grant");
            return mPermissions;
        }
    }

    private TestActivity mActivity;
    private DragEvent mEvent;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mEvent = mock(DragEvent.class);
    }

    @Test
    public void testRequestPermissions_granted() {
        mActivity.mPermissions = mock(DragAndDropPermissions.class);
        assertEquals(
                mActivity.mPermissions,
                DragAndDropPermissionsUtils.requestPermissions(mActivity, mEvent));
    }

    @Test
    public void testRequestPermissions_notGranted() {
        assertNull(DragAndDropPermissionsUtils.requestPermissions(mActivity, mEvent));
    }

    @Test
    public void testRequestPermissions_securityException() {
        mActivity.mThrowSecurityException = true;
        assertNull(DragAndDropPermissionsUtils.requestPermissions(mActivity, mEvent));
    }
}
