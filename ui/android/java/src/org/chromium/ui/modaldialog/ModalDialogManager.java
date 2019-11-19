// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import android.util.SparseArray;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.ui.UiSwitches;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Manager for managing the display of a queue of {@link PropertyModel}s.
 */
public class ModalDialogManager {
    /**
     * An observer of the ModalDialogManager intended to broadcast notifications about any dialog
     * being shown. Observers will know if something is overlaying the screen.
     */
    public interface ModalDialogManagerObserver {
        /**
         * A notification that the manager started showing a modal dialog.
         * @param model The model that describes the dialog that was shown.
         */
        void onDialogShown(PropertyModel model);

        /**
         * A notification that the manager hid a modal dialog.
         * @param model The model that describes the dialog that was hidden.
         */
        void onDialogHidden(PropertyModel model);
    }

    /**
     * Present a {@link PropertyModel} in a container.
     */
    public abstract static class Presenter {
        private Callback<Integer> mDismissCallback;
        private PropertyModel mDialogModel;

        /**
         * @param model The dialog model that's currently showing in this presenter.
         *              If null, no dialog is currently showing.
         */
        private void setDialogModel(
                @Nullable PropertyModel model, @Nullable Callback<Integer> dismissCallback) {
            if (model == null) {
                removeDialogView(mDialogModel);
                mDialogModel = null;
                mDismissCallback = null;
            } else {
                assert mDialogModel
                        == null : "Should call setDialogModel(null) before setting a dialog model.";
                mDialogModel = model;
                mDismissCallback = dismissCallback;
                addDialogView(model);
            }
        }

        /**
         * Run the cached cancel callback and reset the cached callback.
         */
        public final void dismissCurrentDialog(@DialogDismissalCause int dismissalCause) {
            if (mDismissCallback == null) return;

            // Set #mCancelCallback to null before calling the callback to avoid it being
            // updated during the callback.
            Callback<Integer> callback = mDismissCallback;
            mDismissCallback = null;
            callback.onResult(dismissalCause);
        }

        /**
         * @return The dialog model that this presenter is showing.
         */
        public final PropertyModel getDialogModel() {
            return mDialogModel;
        }

        /**
         * @param model The dialog model from which the properties should be obtained.
         * @return The property value for {@link ModalDialogProperties#CONTENT_DESCRIPTION}, or a
         *         fallback content description if it is not set.
         */
        protected static String getContentDescription(PropertyModel model) {
            String description = model.get(ModalDialogProperties.CONTENT_DESCRIPTION);
            if (description == null) description = model.get(ModalDialogProperties.TITLE);
            return description;
        }

        /**
         * Creates a view for the specified dialog model and puts the view in a container.
         * @param model The dialog model that needs to be shown.
         */
        protected abstract void addDialogView(PropertyModel model);

        /**
         * Removes the view created for the specified model from a container.
         * @param model The dialog model that needs to be removed.
         */
        protected abstract void removeDialogView(PropertyModel model);
    }

    @IntDef({ModalDialogType.APP, ModalDialogType.TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModalDialogType {
        // The integer assigned to each type represents its priority. A smaller number represents a
        // higher priority type of dialog.
        int APP = 0;
        int TAB = 1;
    }

    /** Mapping of the {@link Presenter}s and the type of dialogs they are showing. */
    private final SparseArray<Presenter> mPresenters = new SparseArray<>();

    /** Mapping of the lists of pending dialogs and the type of the dialogs. */
    private final SparseArray<List<PropertyModel>> mPendingDialogs = new SparseArray<>();

    /**
     * The list of suspended types of dialogs. The dialogs of types in the list will be suspended
     * from showing and will only be shown after {@link #resumeType(int)} is called.
     */
    private final Set<Integer> mSuspendedTypes = new HashSet<>();

    /** The default presenter to be used if a specified type is not supported. */
    private final Presenter mDefaultPresenter;

    /**
     * The presenter of the type of the dialog that is currently showing. Note that if there is no
     * matching {@link Presenter} for {@link #mCurrentType}, this will be the default presenter.
     */
    private Presenter mCurrentPresenter;

    /**
     * The type of the current dialog. This can be different from the type of the current
     * {@link Presenter} if there is no registered presenter for this type.
     */
    private @ModalDialogType int mCurrentType;

    /**
     * True if the current dialog is in the process of being dismissed.
     */
    private boolean mDismissingCurrentDialog;

    /** Observers of this manager. */
    private final ObserverList<ModalDialogManagerObserver> mObserverList = new ObserverList<>();

    /** Tokens for features temporarily suppressing dialogs. */
    private final Map<Integer, TokenHolder> mTokenHolders = new HashMap<>();

    /**
     * Constructor for initializing default {@link Presenter}.
     * @param defaultPresenter The default presenter to be used when no presenter specified.
     * @param defaultType The dialog type of the default presenter.
     */
    public ModalDialogManager(
            @NonNull Presenter defaultPresenter, @ModalDialogType int defaultType) {
        mDefaultPresenter = defaultPresenter;
        registerPresenter(defaultPresenter, defaultType);

        mTokenHolders.put(ModalDialogType.APP,
                new TokenHolder(() -> resumeTypeInternal(ModalDialogType.APP)));
        mTokenHolders.put(ModalDialogType.TAB,
                new TokenHolder(() -> resumeTypeInternal(ModalDialogType.TAB)));
    }

    /** Clears any dependencies on the showing or pending dialogs. */
    public void destroy() {
        dismissAllDialogs(DialogDismissalCause.ACTIVITY_DESTROYED);
        mObserverList.clear();
    }

    /**
     * Add an observer to this manager.
     * @param observer The observer to add.
     */
    public void addObserver(ModalDialogManagerObserver observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Remove an observer of this manager.
     * @param observer The observer to remove.
     */
    public void removeObserver(ModalDialogManagerObserver observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * Register a {@link Presenter} that shows a specific type of dialog. Note that only one
     * presenter of each type can be registered.
     * @param presenter The {@link Presenter} to be registered.
     * @param dialogType The type of the dialog shown by the specified presenter.
     */
    public void registerPresenter(Presenter presenter, @ModalDialogType int dialogType) {
        assert mPresenters.get(dialogType)
                == null : "Only one presenter can be registered for each type.";
        mPresenters.put(dialogType, presenter);
    }

    /**
     * @return Whether a dialog is currently showing.
     */
    public boolean isShowing() {
        return mCurrentPresenter != null;
    }

    /**
     * Show the specified dialog. If another dialog is currently showing, the specified dialog will
     * be added to the end of the pending dialog list of the specified type.
     * @param model The dialog model to be shown or added to pending list.
     * @param dialogType The type of the dialog to be shown.
     */
    public void showDialog(PropertyModel model, @ModalDialogType int dialogType) {
        showDialog(model, dialogType, false);
    }

    /**
     * Show the specified dialog. If another dialog is currently showing, the specified dialog will
     * be added to the pending dialog list. If showNext is set to true, the dialog will be added
     * to the top of the pending list of its type, otherwise it will be added to the end.
     * @param model The dialog model to be shown or added to pending list.
     * @param dialogType The type of the dialog to be shown.
     * @param showAsNext Whether the specified dialog should be set highest priority of its type.
     */
    public void showDialog(
            PropertyModel model, @ModalDialogType int dialogType, boolean showAsNext) {
        if (CommandLine.getInstance().hasSwitch(UiSwitches.ENABLE_SCREENSHOT_UI_MODE)) {
            return;
        }

        List<PropertyModel> dialogs = mPendingDialogs.get(dialogType);
        if (dialogs == null) mPendingDialogs.put(dialogType, dialogs = new ArrayList<>());

        // Put the new dialog in pending list if the dialog type is suspended or the current dialog
        // is of higher priority.
        if (mSuspendedTypes.contains(dialogType) || (isShowing() && mCurrentType <= dialogType)) {
            dialogs.add(showAsNext ? 0 : dialogs.size(), model);
            return;
        }

        if (isShowing()) suspendCurrentDialog();

        assert !isShowing();
        mCurrentType = dialogType;
        mCurrentPresenter = mPresenters.get(dialogType, mDefaultPresenter);
        mCurrentPresenter.setDialogModel(
                model, (dismissalCause) -> dismissDialog(model, dismissalCause));
        for (ModalDialogManagerObserver o : mObserverList) o.onDialogShown(model);
    }

    /**
     * Dismiss the specified dialog. If the dialog is not currently showing, it will be removed from
     * the pending dialog list. If the dialog is currently being dismissed this function does
     * nothing.
     * @param model The dialog model to be dismissed or removed from pending list.
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialog is
     *                       dismissed.
     */
    public void dismissDialog(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        if (model == null) return;
        if (mCurrentPresenter == null || model != mCurrentPresenter.getDialogModel()) {
            for (int i = 0; i < mPendingDialogs.size(); ++i) {
                List<PropertyModel> dialogs = mPendingDialogs.valueAt(i);
                for (int j = 0; j < dialogs.size(); ++j) {
                    if (dialogs.get(j) == model) {
                        dialogs.remove(j)
                                .get(ModalDialogProperties.CONTROLLER)
                                .onDismiss(model, dismissalCause);
                        return;
                    }
                }
            }
            // If the specified dialog is not found, return without any callbacks.
            return;
        }

        if (!isShowing()) return;
        assert model == mCurrentPresenter.getDialogModel();
        if (mDismissingCurrentDialog) return;
        mDismissingCurrentDialog = true;
        model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
        for (ModalDialogManagerObserver o : mObserverList) o.onDialogHidden(model);
        mCurrentPresenter.setDialogModel(null, null);
        mCurrentPresenter = null;
        mDismissingCurrentDialog = false;
        showNextDialog();
    }

    /**
     * Dismiss the dialog currently shown and remove all pending dialogs.
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialogs are
     *                       dismissed.
     */
    public void dismissAllDialogs(@DialogDismissalCause int dismissalCause) {
        for (int i = 0; i < mPendingDialogs.size(); ++i) {
            dismissPendingDialogsOfType(mPendingDialogs.keyAt(i), dismissalCause);
        }
        if (isShowing()) dismissDialog(mCurrentPresenter.getDialogModel(), dismissalCause);
    }

    /**
     * Dismiss the dialog currently shown and remove all pending dialogs of the specified type.
     * @param dialogType The specified type of dialog.
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialogs are
     *                       dismissed.
     */
    public void dismissDialogsOfType(
            @ModalDialogType int dialogType, @DialogDismissalCause int dismissalCause) {
        dismissPendingDialogsOfType(dialogType, dismissalCause);
        if (isShowing() && dialogType == mCurrentType) {
            dismissDialog(mCurrentPresenter.getDialogModel(), dismissalCause);
        }
    }

    /** Helper method to dismiss pending dialogs of the specified type. */
    private void dismissPendingDialogsOfType(
            @ModalDialogType int dialogType, @DialogDismissalCause int dismissalCause) {
        List<PropertyModel> dialogs = mPendingDialogs.get(dialogType);
        if (dialogs == null) return;

        while (!dialogs.isEmpty()) {
            PropertyModel model = dialogs.remove(0);
            ModalDialogProperties.Controller controller =
                    model.get(ModalDialogProperties.CONTROLLER);
            controller.onDismiss(model, dismissalCause);
        }
    }

    /**
     * Suspend all dialogs of the specified type, including the one currently shown. These dialogs
     * will be prevented from showing unless {@link #resumeType(int, int)} is called after the
     * suspension. If the current dialog is suspended, it will be moved back to the first dialog
     * in the pending list. Any dialogs of the specified type in the pending list will be skipped.
     * @param dialogType The specified type of dialogs to be suspended.
     * @return A token to use when resuming the suspended type.
     */
    public int suspendType(@ModalDialogType int dialogType) {
        mSuspendedTypes.add(dialogType);
        if (isShowing() && dialogType == mCurrentType) {
            suspendCurrentDialog();
            showNextDialog();
        }
        return mTokenHolders.get(dialogType).acquireToken();
    }

    /**
     * Resume the specified type of dialogs after suspension. This method does not resume showing
     * the dialog until after all held tokens are released.
     * @param dialogType The specified type of dialogs to be resumed.
     * @param token The token generated from suspending the dialog type.
     */
    public void resumeType(@ModalDialogType int dialogType, int token) {
        mTokenHolders.get(dialogType).releaseToken(token);
    }

    /**
     * Actually resumes showing the type of dialog after all tokens are released.
     * @param dialogType The specified type of dialogs to be resumed.
     */
    private void resumeTypeInternal(@ModalDialogType int dialogType) {
        if (mTokenHolders.get(dialogType).hasTokens()) return;
        mSuspendedTypes.remove(dialogType);
        if (!isShowing()) showNextDialog();
    }

    /** Hide the current dialog and put it back to the front of the pending list. */
    private void suspendCurrentDialog() {
        assert isShowing();
        PropertyModel dialogView = mCurrentPresenter.getDialogModel();
        mCurrentPresenter.setDialogModel(null, null);
        mCurrentPresenter = null;
        mPendingDialogs.get(mCurrentType).add(0, dialogView);
    }

    /** Helper method for showing the next available dialog in the pending dialog list. */
    private void showNextDialog() {
        assert !isShowing();
        // Show the next dialog of highest priority that its type is not suspended.
        for (int i = 0; i < mPendingDialogs.size(); ++i) {
            int dialogType = mPendingDialogs.keyAt(i);
            if (mSuspendedTypes.contains(dialogType)) continue;

            List<PropertyModel> dialogs = mPendingDialogs.valueAt(i);
            if (!dialogs.isEmpty()) {
                showDialog(dialogs.remove(0), dialogType);
                return;
            }
        }
    }

    @VisibleForTesting
    public PropertyModel getCurrentDialogForTest() {
        return mCurrentPresenter == null ? null : mCurrentPresenter.getDialogModel();
    }

    @VisibleForTesting
    public List<PropertyModel> getPendingDialogsForTest(@ModalDialogType int dialogType) {
        return mPendingDialogs.get(dialogType);
    }

    @VisibleForTesting
    public Presenter getPresenterForTest(@ModalDialogType int dialogType) {
        return mPresenters.get(dialogType);
    }

    @VisibleForTesting
    public Presenter getCurrentPresenterForTest() {
        return mCurrentPresenter;
    }
}
