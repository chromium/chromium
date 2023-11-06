// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.function.Consumer;

/** Robolectric tests for testing the functionalities of {@link PendingDialogContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PendingDialogContainerTest {
    private PendingDialogContainer mPendingDialogContainer;

    @Mock private Consumer<PropertyModel> mPropertyModelConsumerMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPendingDialogContainer = new PendingDialogContainer();
    }

    @Test
    @SmallTest
    public void testSimpleInsertion() {
        PropertyModel model = new PropertyModel();
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model, /* showAsNext= */ false);
        assertTrue("Model should exist after insertion.", mPendingDialogContainer.contains(model));
    }

    @Test
    @SmallTest
    public void testMultipleInsertion_WithShowAsNextSpecified() {
        PropertyModel model1 = new PropertyModel();
        PropertyModel model2 = new PropertyModel();

        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model1, /* showAsNext= */ false);
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model2, /* showAsNext= */ true);

        assertTrue(
                "model1 should exist after insertion.", mPendingDialogContainer.contains(model1));
        assertTrue(
                "model2 should exist after insertion.", mPendingDialogContainer.contains(model2));

        PendingDialogContainer.PendingDialogType nextDialog =
                mPendingDialogContainer.getNextPendingDialog(new HashSet<>());
        // Model2 should be put in first because |showAsNext| was set to true for it.
        assertEquals(
                "model2 should be the next dialog to be shown because of |showAsNext| to true",
                model2,
                nextDialog.propertyModel);
        assertEquals(
                "The dialog type should never change for a pending dialog.",
                ModalDialogType.APP,
                nextDialog.dialogType);
        assertEquals(
                "The dialog priority should never change for a pending dialog.",
                ModalDialogPriority.LOW,
                nextDialog.dialogPriority);

        // Check getNextPendingDialog removes the property model.
        assertFalse(
                "getNextPendingDialog should have removed the model2 from pending dialog"
                        + " container.",
                mPendingDialogContainer.contains(model2));
        // Check getNextPendingDialog doesn't touch the other property model.
        assertTrue(
                "model1 should continue existing in pending dialog container.",
                mPendingDialogContainer.contains(model1));
    }

    @Test
    @SmallTest
    public void testInsertionWithDifferentPriorities() {
        PropertyModel model1 = new PropertyModel();
        PropertyModel model2 = new PropertyModel();

        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model1, /* showAsNext= */ false);
        mPendingDialogContainer.put(
                ModalDialogType.TAB, ModalDialogPriority.HIGH, model2, /* showAsNext= */ false);

        assertTrue(
                "model1 should exist after insertion.", mPendingDialogContainer.contains(model1));
        assertTrue(
                "model2 should exist after insertion.", mPendingDialogContainer.contains(model2));

        PendingDialogContainer.PendingDialogType nextDialog =
                mPendingDialogContainer.getNextPendingDialog(new HashSet<>());
        // Even though model2 is TAB based dialog, this is the next dialog in line because of its
        // priority.
        assertEquals(
                "model2 should be the next dialog because of higher priority than model1.",
                model2,
                nextDialog.propertyModel);
        assertEquals(
                "The dialog type should never change for a pending dialog.",
                ModalDialogType.TAB,
                nextDialog.dialogType);
        assertEquals(
                "The dialog priority should never change for a pending dialog.",
                ModalDialogPriority.HIGH,
                nextDialog.dialogPriority);

        // Check getNextPendingDialog removes the property model.
        assertFalse(
                "getNextPendingDialog should have removed the model2 from"
                        + " pending dialog container.",
                mPendingDialogContainer.contains(model2));
        // Check getNextPendingDialog doesn't touch the other property model.
        assertTrue(
                "model1 should continue existing in pending dialog container.",
                mPendingDialogContainer.contains(model1));
    }

    @Test
    @SmallTest
    public void testSimpleRemoval() {
        PropertyModel model = new PropertyModel();
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model, /* showAsNext= */ false);
        assertTrue("model should exist after insertion.", mPendingDialogContainer.contains(model));
        assertTrue(
                "model should have been found and removed.", mPendingDialogContainer.remove(model));
        assertFalse(
                "model should have been removed after remove operation.",
                mPendingDialogContainer.contains(model));
    }

    @Test
    @SmallTest
    public void testRemovalWithConsumer() {
        PropertyModel model = new PropertyModel();
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model, /* showAsNext= */ false);
        assertTrue("model should exist after insertion.", mPendingDialogContainer.contains(model));

        doNothing().when(mPropertyModelConsumerMock).accept(model);
        assertTrue(
                "model should have been found and removed.",
                mPendingDialogContainer.remove(ModalDialogType.APP, mPropertyModelConsumerMock));
        verify(mPropertyModelConsumerMock).accept(model);
        assertFalse(
                "model should have been removed after remove operation.",
                mPendingDialogContainer.contains(model));
    }

    @Test
    @SmallTest
    public void testMultipleRemovalsWithConsumer_SameDialogType() {
        PropertyModel model1 = new PropertyModel();
        PropertyModel model2 = new PropertyModel();
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model1, /* showAsNext= */ false);
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model2, /* showAsNext= */ false);
        assertTrue(
                "model1 should exist after insertion.", mPendingDialogContainer.contains(model1));
        assertTrue(
                "model2 should exist after insertion.", mPendingDialogContainer.contains(model2));

        doNothing().when(mPropertyModelConsumerMock).accept(model1);
        doNothing().when(mPropertyModelConsumerMock).accept(model2);
        assertTrue(
                "App based dialogs should have been found and removed.",
                mPendingDialogContainer.remove(ModalDialogType.APP, mPropertyModelConsumerMock));
        verify(mPropertyModelConsumerMock, times(2)).accept(any());

        assertFalse(
                "model1 should have been removed because it was of APP type.",
                mPendingDialogContainer.contains(model1));
        assertFalse(
                "model2 should have been removed because it was of APP type.",
                mPendingDialogContainer.contains(model2));
        assertTrue("All dialogs should have been removed.", mPendingDialogContainer.isEmpty());
    }

    @Test
    @SmallTest
    public void testMultipleRemovalsWithConsumer_DifferentDialogType() {
        PropertyModel model1 = new PropertyModel();
        PropertyModel model2 = new PropertyModel();
        mPendingDialogContainer.put(
                ModalDialogType.APP, ModalDialogPriority.LOW, model1, /* showAsNext= */ false);
        mPendingDialogContainer.put(
                ModalDialogType.TAB, ModalDialogPriority.LOW, model2, /* showAsNext= */ false);
        assertTrue(
                "model1 should exist after insertion.", mPendingDialogContainer.contains(model1));
        assertTrue(
                "model2 should exist after insertion.", mPendingDialogContainer.contains(model2));

        doNothing().when(mPropertyModelConsumerMock).accept(model1);
        assertTrue(
                "App based dialogs should have been found and removed.",
                mPendingDialogContainer.remove(ModalDialogType.APP, mPropertyModelConsumerMock));
        verify(mPropertyModelConsumerMock).accept(model1);

        assertFalse(
                "model1 should have been removed because it was of APP type.",
                mPendingDialogContainer.contains(model1));
        assertTrue(
                "Nothing should happen to dialogs of type TAB because we only removed"
                        + " APP based dialogs.",
                mPendingDialogContainer.contains(model2));
        assertFalse(
                "Container shouldn't be empty because there's still model2.",
                mPendingDialogContainer.isEmpty());
    }

    @Test
    @SmallTest
    public void testNonExistentenceOfDialogs() {
        PropertyModel model = new PropertyModel();
        assertFalse(
                "Model was not added therefore shouldn't be contained in pending dialog"
                        + " container.",
                mPendingDialogContainer.contains(model));
    }

    @Test
    @SmallTest
    public void testRemovalOfNonExistententPendingDialogs() {
        PropertyModel model = new PropertyModel();
        assertFalse(
                "Model was not added therefore shouldn't have been found and removed.",
                mPendingDialogContainer.remove(model));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testOutOfBoundValues_LowerBound() {
        PropertyModel model = new PropertyModel();
        mPendingDialogContainer.put(ModalDialogType.APP, 0, model, /* showAsNext= */ false);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testOutOfBoundValues_UpperBound() {
        PropertyModel model = new PropertyModel();
        mPendingDialogContainer.put(ModalDialogType.APP, 10, model, /* showAsNext= */ false);
    }

    @Test
    @SmallTest
    public void testKeyUniqueness() {
        for (@ModalDialogPriority int priority = ModalDialogPriority.RANGE_MIN;
                priority <= ModalDialogPriority.RANGE_MAX;
                ++priority) {
            for (@ModalDialogType int type = ModalDialogType.RANGE_MIN;
                    type <= ModalDialogType.RANGE_MAX;
                    ++type) {
                mPendingDialogContainer.put(
                        type, priority, new PropertyModel(), /* showAsNext= */ false);
            }
        }
        final int totalPriorities = ModalDialogPriority.NUM_ENTRIES;
        final int totalTypes = ModalDialogType.NUM_ENTRIES;

        assertEquals(
                "mPendingDialogContainer should have created unique keys to map the"
                        + " pending dialog list of distinct type and priority.",
                totalPriorities * totalTypes,
                mPendingDialogContainer.size());
    }
}
