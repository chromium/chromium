// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.activity.ComponentDialog;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Tests to validate actions within {@link ModalDialogManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModalDialogManagerTest {
    private static final int MAX_DIALOGS = 4;

    @Spy private ModalDialogManager.Presenter mAppModalPresenter;
    @Mock private ModalDialogManager.Presenter mTabModalPresenter;

    private ModalDialogManager mModalDialogManager;
    private List<PropertyModel> mDialogModels = new ArrayList<>();

    @Mock private ModalDialogManagerObserver mObserver;

    @Captor ArgumentCaptor<PropertyModel> mDialogModelCaptor;
    @Captor ArgumentCaptor<Callback<ComponentDialog>> mOnDialogShownCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModalDialogManager = new ModalDialogManager(mAppModalPresenter, ModalDialogType.APP);
        mModalDialogManager.registerPresenter(mTabModalPresenter, ModalDialogType.TAB);
        mModalDialogManager.addObserver(mObserver);

        for (int i = 0; i < MAX_DIALOGS; ++i) {
            ModalDialogProperties.Controller controller =
                    new ModalDialogProperties.Controller() {
                        @Override
                        public void onClick(PropertyModel model, int buttonType) {}

                        @Override
                        public void onDismiss(PropertyModel model, int dismissalCause) {}
                    };

            mDialogModels.add(
                    new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                            .with(ModalDialogProperties.CONTROLLER, spy(controller))
                            .build());
        }
    }

    /** Tests that the events on the {@link ModalDialogManagerObserver} are called correctly. */
    @Test
    @Feature({"ModalDialogManagerObserver"})
    public void testModalDialogObserver() {
        // Show two dialogs and make sure show is only called on one until it is hidden.
        verify(mObserver, times(0)).onDialogAdded(mDialogModels.get(0));
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        verify(mObserver, times(1)).onDialogAdded(mDialogModels.get(0));
        verify(mObserver, times(0)).onDialogAdded(mDialogModels.get(1));

        verify(mObserver, times(0)).onDialogDismissed(mDialogModels.get(0));
        mModalDialogManager.dismissDialog(mDialogModels.get(0), DialogDismissalCause.UNKNOWN);
        verify(mObserver, times(1)).onDialogDismissed(mDialogModels.get(0));
        verify(mObserver, times(1)).onDialogAdded(mDialogModels.get(1));

        mModalDialogManager.dismissDialog(mDialogModels.get(1), DialogDismissalCause.UNKNOWN);

        // Dialog view should be removed first before #onDialogDismissed; otherwise,
        // Presenter#getCurrentModel inside #onDialogDismissed will not return null.
        InOrder inOrder = Mockito.inOrder(mAppModalPresenter, mObserver);
        inOrder.verify(mAppModalPresenter).removeDialogView(mDialogModels.get(1));
        inOrder.verify(mObserver, times(1)).onDialogDismissed(mDialogModels.get(1));
        // Calling the same function again, as well as dismissDialogsOfType() should not trigger
        // notifying of empty (because onLastDialogDismissed() was already called once, and a new
        // dialog wasn't added).
        mModalDialogManager.dismissDialog(mDialogModels.get(1), DialogDismissalCause.UNKNOWN);
        mModalDialogManager.dismissDialogsOfType(
                ModalDialogType.APP, DialogDismissalCause.ACTIVITY_DESTROYED);
        verify(mObserver, times(1)).onLastDialogDismissed();
    }

    /** Tests showing a dialog when no dialog is currently showing. */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_NoDialogIsShowing() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertTrue(mModalDialogManager.isShowing());
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());
    }

    /**
     * Tests showing a dialog when another dialog of same or higher priority is currently showing.
     */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_DialogOfSameOrHigherPriorityIsShowing() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Show another app modal dialog and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Show a tab modal dialog and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());
    }

    /** Tests showing a dialog when another dialog of lower priority is currently showing. */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_DialogOfLowerPriorityIsShowing() {
        // Show a tab modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.TAB);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(0)).addDialogView(any(), any());
        verify(mTabModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());

        // Show an app modal dialog, and verify that the app modal dialog is shown, and the tab
        // modal dialog is queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        assertEquals(mDialogModels.get(1), mModalDialogManager.getCurrentDialogForTest());
        // APP based dialog has a higher priority than TAB based dialog therefore it shouldn't
        // have initialized the pending dialogs pertaining to it when it was asked to be shown after
        // TAB based dialog.
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(1), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(1)).addDialogView(any(), any());
    }

    /**
     * Tests whether the next dialog in the pending queue is correctly shown after the current
     * dialog is dismissed.
     */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_ShowNextDialogAfterDismiss() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Show a second and a third app modal dialog and verify that they are queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(any(), any());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Dismiss the first dialog and verify that the second dialog is shown.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(1), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(2))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getAllValues().get(0));
        assertEquals(mDialogModels.get(1), mDialogModelCaptor.getAllValues().get(2));
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());
    }

    /** Tests showing a dialog as the next available dialog in the pending queue. */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_ShowAsNext() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Show a second app modal dialog and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(any(), any());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Show a third app modal dialog as next and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.APP, true);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(any(), any());
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Dismiss the first dialog and verify that the third dialog is shown.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(2), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(2))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getAllValues().get(0));
        assertEquals(mDialogModels.get(2), mDialogModelCaptor.getAllValues().get(2));
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());
    }

    /** Tests dismissing the current dialog. */
    @Test
    @Feature({"ModalDialog"})
    public void testDismissDialog_CurrentDialog() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getValue());

        // Show a tab modal dialog then a second app modal dialog and verify that they are queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.TAB);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Dismiss the current dialog and the second app modal dialog should be showing next.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(2), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1)).removeDialogView(mDialogModels.get(0));

        assertTrue(mModalDialogManager.isShowing());
        assertNull(
                "Dismissing the last modal dialog of its type didn't remove the"
                        + " corresponding pending list.",
                mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        verify(mAppModalPresenter, times(2))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(2), mDialogModelCaptor.getAllValues().get(2));
        verify(mTabModalPresenter, times(0)).addDialogView(any(), any());

        // Dismiss the first dialog again and verify nothing is changed.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(2), mModalDialogManager.getCurrentDialogForTest());
        assertNull(
                "Dismissing the last modal dialog of its type didn't remove the"
                        + " corresponding pending list.",
                mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        Mockito.verifyNoMoreInteractions(mAppModalPresenter, mTabModalPresenter);
    }

    /** Tests dismissing a dialog in the pending queue. */
    @Test
    @Feature({"ModalDialog"})
    public void testDismissDialog_DialogInQueue() {
        // Show three dialogs.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Dismiss the second dialog.
        mModalDialogManager.dismissDialog(mDialogModels.get(1), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(1), 1);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertNull(
                "Dismissing the last modal dialog of its type didn't remove the"
                        + " corresponding pending list.",
                mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Dismiss the third dialog.
        mModalDialogManager.dismissDialog(mDialogModels.get(2), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(2), 1);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
    }

    /** Tests dismissing all dialogs. */
    @Test
    @Feature({"ModalDialog"})
    public void testDismissAllDialogs() {
        // Show three dialogs.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);

        // Dismiss all dialog.
        mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertOnDismissCalled(mDialogModels.get(1), 1);
        assertOnDismissCalled(mDialogModels.get(2), 1);
        assertFalse(mModalDialogManager.isShowing());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
    }

    /** Tests dismissing dialogs of a certain type. */
    @Test
    @Feature({"ModalDialog"})
    public void testDismissDialogsOfType() {
        // Show three dialogs.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);

        // Dismiss all app modal dialogs.
        mModalDialogManager.dismissDialogsOfType(ModalDialogType.APP, DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertOnDismissCalled(mDialogModels.get(1), 1);
        assertEquals(mDialogModels.get(2), mModalDialogManager.getCurrentDialogForTest());
        assertNull(
                "Dismissing the last modal dialog of its type didn't remove the"
                        + " corresponding pending list.",
                mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertNull(
                "Dismissing the last modal dialog of its type didn't remove the"
                        + " corresponding pending list.",
                mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
    }

    /** Tests suspending dialogs of a certain type. */
    @Test
    @Feature({"ModalDialog"})
    public void testSuspendType() {
        // Show two tab modal dialogs.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.TAB);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.TAB);

        // Suspend all tab modal dialogs. onDismiss() should not be called.
        mModalDialogManager.suspendType(ModalDialogType.TAB);
        assertOnDismissCalled(mDialogModels.get(0), 0);
        assertOnDismissCalled(mDialogModels.get(1), 0);
        assertFalse(mModalDialogManager.isShowing());
        assertFalse(mModalDialogManager.isSuspended(ModalDialogType.APP));
        assertTrue(mModalDialogManager.isSuspended(ModalDialogType.TAB));
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Show a third tab modal dialog, and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);
        assertFalse(mModalDialogManager.isShowing());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(3, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Show an app modal dialog, and verify that it is shown.
        mModalDialogManager.showDialog(mDialogModels.get(3), ModalDialogType.APP);
        assertEquals(mDialogModels.get(3), mModalDialogManager.getCurrentDialogForTest());
        // APP based dialog has a higher priority than TAB based dialog therefore it shouldn't
        // have initialized the pending dialogs pertaining to it when it was asked to be shown after
        // TAB based dialog.
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP));
        assertEquals(3, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
    }

    /** Tests resuming dialogs of a certain type. */
    @Test
    @Feature({"ModalDialog"})
    public void testResumeType() {
        // Show three tab modal dialogs.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.TAB);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.TAB);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);

        // Suspend all tab modal dialogs.
        int token = mModalDialogManager.suspendType(ModalDialogType.TAB);
        assertTrue(mModalDialogManager.isSuspended(ModalDialogType.TAB));
        assertFalse(mModalDialogManager.isShowing());
        assertEquals(3, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Resume tab modal dialogs.
        mModalDialogManager.resumeType(ModalDialogType.TAB, token);
        assertFalse(mModalDialogManager.isSuspended(ModalDialogType.TAB));
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
    }

    @Test
    @Feature({"ModalDialog"})
    public void testVeryHighPriorityDialog_SuspendType_APP_DoesNotDismissCurrentDialog() {
        // Show a very high priority dialog of type APP.
        mModalDialogManager.showDialog(
                mDialogModels.get(0),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH,
                false);
        // Suspend the APP type and check we are still showing the very_high priority dialog.
        mModalDialogManager.suspendType(ModalDialogType.APP);
        assertTrue(mModalDialogManager.isShowing());
    }

    @Test
    @Feature({"ModalDialog"})
    public void testVeryHighPriorityDialog_SuspendType_TAB_DoesNotDismissCurrentDialog() {
        // Show a very high priority dialog of type TAB.
        mModalDialogManager.showDialog(
                mDialogModels.get(0),
                ModalDialogType.TAB,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH,
                false);
        // Suspend the APP type and check we are still showing the very_high priority dialog.
        mModalDialogManager.suspendType(ModalDialogType.TAB);
        assertTrue(mModalDialogManager.isShowing());
    }

    @Test
    @Feature({"ModalDialog"})
    public void testSuspendType_StillAllowsShowing_NewVeryHighPriorityDialog_OfSameType() {
        // Show a high priority dialog.
        mModalDialogManager.showDialog(
                mDialogModels.get(0),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.HIGH,
                false);

        // Suspend the APP type.
        mModalDialogManager.suspendType(ModalDialogType.APP);
        assertFalse(mModalDialogManager.isShowing());

        // Create a new dialog of the same type(!) but with a very_high priority and check it's
        // shown.
        mModalDialogManager.showDialog(
                mDialogModels.get(1),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH,
                false);
        assertTrue(mModalDialogManager.isShowing());
        assertEquals(
                mDialogModels.get(1),
                mModalDialogManager.getCurrentPresenterForTest().getDialogModel());
    }

    @Test
    @Feature({"ModalDialog"})
    public void testVeryHighPriorityDialog_IsShown_IfCurrentDialog_IsLowerPriority() {
        // Show a high priority dialog.
        mModalDialogManager.showDialog(
                mDialogModels.get(0),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.HIGH,
                false);
        // Create a new dialog of the same type but with a very_high priority and check it's
        // shown.
        mModalDialogManager.showDialog(
                mDialogModels.get(1),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH,
                false);
        assertTrue(mModalDialogManager.isShowing());
        assertEquals(
                mDialogModels.get(1),
                mModalDialogManager.getCurrentPresenterForTest().getDialogModel());
        // Check that the previously shown dialog was removed and we are now showing the new dialog
        // which has a very high priority.
        verify(mAppModalPresenter, times(1)).removeDialogView(mDialogModels.get(0));
        verify(mAppModalPresenter, times(2))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(1), mDialogModelCaptor.getAllValues().get(1));
    }

    @Test
    @Feature({"ModalDialog"})
    public void testVeryHighPriorityDialog_IsNotShown_IfCurrentDialog_IsAlsoVeryHighPriority() {
        // Show a very high priority dialog.
        mModalDialogManager.showDialog(
                mDialogModels.get(0),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH,
                false);
        assertTrue(mModalDialogManager.isShowing());
        verify(mAppModalPresenter, times(1))
                .addDialogView(
                        mDialogModelCaptor.capture(), mOnDialogShownCallbackCaptor.capture());
        assertEquals(mDialogModels.get(0), mDialogModelCaptor.getAllValues().get(0));

        // Create a new dialog of the same type and with very_high priority as well.
        mModalDialogManager.showDialog(
                mDialogModels.get(1),
                ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH,
                false);

        // Check that the new dialog is not shown and the previously shown dialog is never removed.
        verify(mAppModalPresenter, times(0)).removeDialogView(mDialogModels.get(0));
        assertEquals(
                mDialogModels.get(0),
                mModalDialogManager.getCurrentPresenterForTest().getDialogModel());
    }

    private static void assertOnDismissCalled(PropertyModel model, int numberOfInvocations) {
        verify(model.get(ModalDialogProperties.CONTROLLER), times(numberOfInvocations))
                .onDismiss(eq(model), anyInt());
    }
}
