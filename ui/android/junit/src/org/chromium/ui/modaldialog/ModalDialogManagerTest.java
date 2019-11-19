// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import static junit.framework.Assert.assertFalse;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests to validate actions within {@link ModalDialogManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModalDialogManagerTest {
    private static final int MAX_DIALOGS = 4;

    @Mock
    private ModalDialogManager.Presenter mAppModalPresenter;
    @Mock
    private ModalDialogManager.Presenter mTabModalPresenter;

    private ModalDialogManager mModalDialogManager;
    private List<PropertyModel> mDialogModels = new ArrayList<>();

    @Mock
    private ModalDialogManagerObserver mObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModalDialogManager = new ModalDialogManager(mAppModalPresenter, ModalDialogType.APP);
        mModalDialogManager.registerPresenter(mTabModalPresenter, ModalDialogType.TAB);
        mModalDialogManager.addObserver(mObserver);

        for (int i = 0; i < MAX_DIALOGS; ++i) {
            ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {}

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {}
            };

            mDialogModels.add(new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                      .with(ModalDialogProperties.CONTROLLER, spy(controller))
                                      .build());
        }
    }

    /** Tests that the events on the {@link ModalDialogManagerObserver} are called correctly. */
    @Test
    @Feature({"ModalDialogManagerObserver"})
    public void testModalDialogObserver() {
        // Show two dialogs and make sure show is only called on one until it is hidden.
        verify(mObserver, times(0)).onDialogShown(mDialogModels.get(0));
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        verify(mObserver, times(1)).onDialogShown(mDialogModels.get(0));
        verify(mObserver, times(0)).onDialogShown(mDialogModels.get(1));

        verify(mObserver, times(0)).onDialogHidden(mDialogModels.get(0));
        mModalDialogManager.dismissDialog(mDialogModels.get(0), ModalDialogType.APP);
        verify(mObserver, times(1)).onDialogHidden(mDialogModels.get(0));
        verify(mObserver, times(1)).onDialogShown(mDialogModels.get(1));
    }

    /** Tests showing a dialog when no dialog is currently showing. */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_NoDialogIsShowing() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertTrue(mModalDialogManager.isShowing());
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mTabModalPresenter, times(0)).addDialogView(any());
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
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Show another app modal dialog and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mAppModalPresenter, times(0)).addDialogView(mDialogModels.get(1));
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Show a tab modal dialog and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.TAB);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mAppModalPresenter, times(0)).addDialogView(mDialogModels.get(1));
        verify(mTabModalPresenter, times(0)).addDialogView(any());
    }

    /** Tests showing a dialog when another dialog of lower priority is currently showing. */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_DialogOfLowerPriorityIsShowing() {
        // Show a tab modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.TAB);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(0)).addDialogView(any());
        verify(mTabModalPresenter, times(1)).addDialogView(mDialogModels.get(0));

        // Show an app modal dialog, and verify that the app modal dialog is shown, and the tab
        // modal dialog is queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        assertEquals(mDialogModels.get(1), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        verify(mAppModalPresenter, times(0)).addDialogView(mDialogModels.get(0));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(1));
        verify(mTabModalPresenter, times(1)).addDialogView(any());
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
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Show a second and a third app modal dialog and verify that they are queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(any());
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Dismiss the first dialog and verify that the second dialog is shown.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(1), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(1));
        verify(mAppModalPresenter, times(0)).addDialogView(mDialogModels.get(2));
        verify(mTabModalPresenter, times(0)).addDialogView(any());
    }

    /** Tests showing a dialog as the next available dialog in the pending queue. */
    @Test
    @Feature({"ModalDialog"})
    public void testShowDialog_ShowAsNext() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Show a second app modal dialog and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(1), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(any());
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Show a third app modal dialog as next and verify that it is queued.
        mModalDialogManager.showDialog(mDialogModels.get(2), ModalDialogType.APP, true);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(any());
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Dismiss the first dialog and verify that the third dialog is shown.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(2), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertNull(mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));
        verify(mAppModalPresenter, times(0)).addDialogView(mDialogModels.get(1));
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(2));
        verify(mTabModalPresenter, times(0)).addDialogView(any());
    }

    /** Tests dismissing the current dialog. */
    @Test
    @Feature({"ModalDialog"})
    public void testDismissDialog_CurrentDialog() {
        // Show an app modal dialog and verify that it is showing.
        mModalDialogManager.showDialog(mDialogModels.get(0), ModalDialogType.APP);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(0));

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
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
        verify(mAppModalPresenter, times(1)).addDialogView(mDialogModels.get(2));
        verify(mTabModalPresenter, times(0)).addDialogView(any());

        // Dismiss the first dialog again and verify nothing is changed.
        mModalDialogManager.dismissDialog(mDialogModels.get(0), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(0), 1);
        assertEquals(mDialogModels.get(2), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
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
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(1, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Dismiss the third dialog.
        mModalDialogManager.dismissDialog(mDialogModels.get(2), DialogDismissalCause.UNKNOWN);
        assertOnDismissCalled(mDialogModels.get(2), 1);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
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
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
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
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
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
        assertEquals(0, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.APP).size());
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
        assertFalse(mModalDialogManager.isShowing());
        assertEquals(3, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());

        // Resume tab modal dialogs.
        mModalDialogManager.resumeType(ModalDialogType.TAB, token);
        assertEquals(mDialogModels.get(0), mModalDialogManager.getCurrentDialogForTest());
        assertEquals(2, mModalDialogManager.getPendingDialogsForTest(ModalDialogType.TAB).size());
    }

    private static void assertOnDismissCalled(PropertyModel model, int numberOfInvocations) {
        verify(model.get(ModalDialogProperties.CONTROLLER), times(numberOfInvocations))
                .onDismiss(eq(model), anyInt());
    }
}
