// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.DragEvent;
import android.view.View.DragShadowBuilder;
import java.util.concurrent.TimeUnit;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.ui.dragdrop.DragDropGlobalState.TrackerToken;

@RunWith(org.chromium.base.test.BaseRobolectricTestRunner.class)
public final class DragDropGlobalStateTest {
    private static final int INSTANCE_ID = 1;
    private static final int INVALID_INSTANCE_ID = -1;
    private final String mText = "text";
    @Rule public MockitoRule mMockitoProcessorRule = MockitoJUnit.rule();
    private DropDataAndroid mDropData;
    private TrackerToken mToken;

    @Before
    public void setup() {
        mDropData = DropDataAndroid.create(mText, null, null, null, null);
    }

    @Test
    public void testInstance() {
        mToken = DragDropGlobalState.store(INSTANCE_ID, mDropData, null);

        // Assert instance through build token.
        assertTrue("Instance should exist", DragDropGlobalState.hasValue());
        DragDropGlobalState instance = DragDropGlobalState.getState(mToken);
        assertNotNull("Instance get should not be null", instance);
        assertTrue("SourceInstanceId should match.", instance.isDragSourceInstance(INSTANCE_ID));
        assertEquals("Text being dragged should match.", mText, mDropData.text);

        // Assert release token.
        DragDropGlobalState.clear(mToken);
        assertFalse("Instance should not exist", DragDropGlobalState.hasValue());
    }

    @Test
    public void testGetState() {
        mToken = DragDropGlobalState.store(INSTANCE_ID, mDropData, null);

        // Assert instance through acquired token.
        assertTrue("Instance should exist", DragDropGlobalState.hasValue());
        DragEvent dropEvent = Mockito.mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);

        DragDropGlobalState instance = DragDropGlobalState.getState(dropEvent);
        assertNotNull("Instance get should not be null", instance);
        assertTrue("SourceInstanceId should match.", instance.isDragSourceInstance(INSTANCE_ID));
        assertEquals("Text being dragged should match.", mText, mDropData.text);
        assertTrue("Instance should still exists.", DragDropGlobalState.hasValue());

        // Assert release token - no-op.
        DragDropGlobalState.clear(mToken);
        assertFalse("Instance should not exist.", DragDropGlobalState.hasValue());
    }

    @Test
    public void clearWithInvalidToken() {
        mToken = DragDropGlobalState.store(INVALID_INSTANCE_ID, null, null);

        ShadowSystemClock.advanceBy(100, TimeUnit.SECONDS);
        TrackerToken newToken = DragDropGlobalState.store(INSTANCE_ID, mDropData, null);
        try {
            DragDropGlobalState.clear(mToken);
        } catch (AssertionError error) {
            DragDropGlobalState.clear(newToken);
            return;
        }
        Assert.fail("Clear with invalid token should throughs assertion error.");
    }

    @Test
    public void getDrawShadowBuilderAfterClear() {
        DragShadowBuilder builder = mock(DragShadowBuilder.class);
        mToken = DragDropGlobalState.store(INSTANCE_ID, mDropData, builder);
        assertEquals("Builder mismatch.", builder, DragDropGlobalState.getDragShadowBuilder());

        // Get state by token.
        DragDropGlobalState.getState(mToken);
        assertEquals(
                "Builder is not impact by getState.",
                builder,
                DragDropGlobalState.getDragShadowBuilder());

        // Get state by drop event.
        DragEvent dropEvent = Mockito.mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        DragDropGlobalState.getState(dropEvent);
        assertEquals(
                "Builder is not impact by getState.",
                builder,
                DragDropGlobalState.getDragShadowBuilder());

        DragDropGlobalState.clear(mToken);
        assertNull(
                "Drag shadow builder is removed after clear.",
                DragDropGlobalState.getDragShadowBuilder());
    }
}
