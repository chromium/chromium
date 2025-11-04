// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import static org.chromium.ui.base.KeyNavigationUtil.isGoBackward;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Handler;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.ArrayList;
import java.util.List;

/**
 * A controller to manage the logic for hierarchical menus i.e., flyout and drilldown.
 *
 * <p>This class centralizes the logic for handling submenu interactions. It uses a {@link
 * HierarchicalMenuKeyProvider} to interact with a menu's PropertyModel.
 *
 * @param <T> The type of the object that the {@link FlyoutHandler} manages (e.g., PopupWindow).
 */
@NullMarked
public class HierarchicalMenuController<T> {

    /** An interface for creating the ListItem for drilldown's header. */
    public interface SubmenuHeaderFactory {
        /**
         * Creates the ListItem for drilldown's header.
         *
         * @param clickedItem The item that was clicked to open this submenu.
         * @param backRunnable The runnable to execute when the header is clicked or a "back" key
         *     event is received, allowing navigation back to the parent menu.
         * @return The {@link ListItem} to be used as the submenu's header.
         */
        ListItem createHeaderItem(ListItem clickedItem, Runnable backRunnable);
    }

    private final Context mContext;
    private final HierarchicalMenuKeyProvider mKeyProvider;

    private @Nullable Handler mHoverExitDelayHandler;
    private @Nullable Runnable mPendingHoverExitRunnable;

    private @Nullable Boolean mDrillDownOverrideValue;

    private final SubmenuHeaderFactory mSubmenuHeaderFactory;

    private @Nullable FlyoutController<T> mFlyoutController;
    private List<ListItem> mLastHighlightedPath = new ArrayList<ListItem>();

    /**
     * Creates an instance of the controller.
     *
     * @param context The application's {@link Context} to retrieve resources.
     * @param submenuHeaderFactory The {@link SubmenuHeaderFactory} to use.
     * @param keyProvider The {@link HierarchicalMenuKeyProvider} for the controller to use.
     */
    public HierarchicalMenuController(
            Context context,
            HierarchicalMenuKeyProvider keyProvider,
            SubmenuHeaderFactory submenuHeaderFactory) {
        mContext = context;
        mSubmenuHeaderFactory = submenuHeaderFactory;
        mKeyProvider = keyProvider;

        // To use flyout, call {@link setupFlyoutController}.
        mFlyoutController = null;
        mDrillDownOverrideValue = true;
    }

    /**
     * Creates and initializes the {@link FlyoutController}.
     *
     * @param flyoutHandler The {@link FlyoutHandler} for the controller to use for displaying
     *     flyout popups.
     * @param mainPopup The main popup window for flyout popups to fly out of.
     * @param drillDownOverrideValue If not null, forces the menu behavior to be drill-down ({@code
     *     true}) or flyout ({@code false}), overriding the default.
     */
    public void setupFlyoutController(
            FlyoutHandler<T> flyoutHandler, T mainPopup, @Nullable Boolean drillDownOverrideValue) {
        mFlyoutController = new FlyoutController<T>(flyoutHandler, mKeyProvider, mainPopup, this);
        mDrillDownOverrideValue = drillDownOverrideValue;
    }

    /** Dismiss all popups including the main window and destroy the {@link FlyoutController}. */
    public void destroyFlyoutController() {
        assert mFlyoutController != null;
        mFlyoutController.destroy();
        mFlyoutController = null;
    }

    /**
     * Gets the {@link FlyoutController}.
     *
     * @return The {@link FlyoutController} that this controller manages.
     */
    public @Nullable FlyoutController<T> getFlyoutController() {
        return mFlyoutController;
    }

    /**
     * Determines whether to use a drill-down menu style. If an override value is given, it is
     * respected. If the override value is null and a pointer device is connected, we use the window
     * width and the position of the main, non-flyout popup to calculate whether to use flyout or
     * drilldown. For flyout to be chosen, there has to be enough space for a set margin and a set
     * spacing in at least one side of the main popup window for the maximum width flyout popup to
     * fit in.
     *
     * @return True to use the drilldown, false to use the flyout style.
     */
    public boolean shouldUseDrillDown() {
        if (mDrillDownOverrideValue != null) {
            if (!mDrillDownOverrideValue) {
                assert mFlyoutController != null;
            }
            return mDrillDownOverrideValue;
        }

        if (mFlyoutController == null) {
            return true;
        }

        if (!DeviceInput.supportsPrecisionPointer()) {
            return true;
        }

        DisplayMetrics displayMetrics = new DisplayMetrics();
        DisplayAndroidManager.getDefaultDisplayForContext(mContext).getMetrics(displayMetrics);

        return !possibleToFitFlyout(
                mFlyoutController.getMainPopupRect(),
                displayMetrics.widthPixels,
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.hierarchical_menu_min_spacing_for_flyout),
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.hierarchical_menu_min_margin_for_flyout),
                mContext.getResources().getDimensionPixelSize(R.dimen.list_menu_width));
    }

    /**
     * In order for flyout to fit, the available space, either to the left or the right of the main
     * popup, has to be larger than minSpacing + maxFlyoutWidth + minMargin.
     *
     * <pre>
     *     +----------------------+
     *     +----------------------+
     *     |     +------+         |
     *     |     | main |         |
     *     |     |  +--------+    |
     *     |     +--| flyout |    |
     *     |        +--------+    |
     *     +----------------------+
     *           <-->         <--->
     *        minSpacing    minMargin
     *           <---------------->
     *             availableSpace
     * </pre>
     */
    @VisibleForTesting
    static boolean possibleToFitFlyout(
            Rect mainPopupRect,
            int windowWidth,
            int minSpacing,
            int minMargin,
            int maxFlyoutWidth) {
        int spaceToLeft = mainPopupRect.right;
        int spaceToRight = windowWidth - mainPopupRect.left;
        int availableSpace = spaceToLeft > spaceToRight ? spaceToLeft : spaceToRight;

        if (availableSpace < minSpacing + maxFlyoutWidth + minMargin) {
            return false;
        }

        return true;
    }

    /**
     * Updates the highlight state of menu items based on the new hover path. The addition of flyout
     * windows requires us to precisely control the hover states of the items. Specifically, when
     * the user is hovering on an item inside a flyout popup, all of the ancestor items should
     * remain highlighted, even when the hover itself is not on those items.
     *
     * @param highlightPath The list of {@link ListItem}s from the root to the currently hovered
     *     item that should be highlighted.
     */
    public void updateHighlights(List<ListItem> highlightPath) {
        int forkIndex = -1;

        for (int i = 0; i < Math.min(mLastHighlightedPath.size(), highlightPath.size()); i++) {
            if (mLastHighlightedPath.get(i) == highlightPath.get(i)) {
                forkIndex = i;
            } else {
                break;
            }
        }

        WritableBooleanPropertyKey isHighlightedKey = mKeyProvider.getIsHighlightedKey();

        for (int i = forkIndex + 1; i < mLastHighlightedPath.size(); i++) {
            mLastHighlightedPath.get(i).model.set(isHighlightedKey, false);
        }

        for (int i = forkIndex + 1; i < highlightPath.size(); i++) {
            highlightPath.get(i).model.set(isHighlightedKey, true);
        }

        mLastHighlightedPath = highlightPath;
    }

    /**
     * Processes hover events for a menu item, managing highlight states and flyout menu triggers.
     * This method should be called from an OnHoverListener.
     *
     * <p>On ACTION_HOVER_ENTER, it initiates the logic to highlight the item and potentially show a
     * flyout submenu after a delay.
     *
     * <p>On ACTION_HOVER_EXIT, it updates the highlight path and cancels any pending flyout, but
     * intentionally leaves existing flyout menus open until the user hovers over a new item.
     *
     * @param event The MotionEvent triggered by the hover.
     * @param item The ListItem that is the target of the hover event.
     * @param view The View associated with the hovered ListItem.
     * @param levelOfHoveredItem The depth of the item within the menu hierarchy (e.g., 0 for root
     *     items, 1 for sub-menu items).
     * @param highlightPath The complete list of items from the root of the menu to the currently
     *     hovered {@code item}, inclusive.
     * @return {@code true} if the hover event was handled (ACTION_HOVER_ENTER or
     *     ACTION_HOVER_EXIT), {@code false} otherwise.
     */
    public boolean handleHoverEvent(
            MotionEvent event,
            ListItem item,
            View view,
            int levelOfHoveredItem,
            List<ListItem> highlightPath) {
        if (mPendingHoverExitRunnable != null) {
            assert mHoverExitDelayHandler != null;
            mHoverExitDelayHandler.removeCallbacks(mPendingHoverExitRunnable);
            mPendingHoverExitRunnable = null;
            mHoverExitDelayHandler = null;
        }

        switch (event.getAction()) {
            case MotionEvent.ACTION_HOVER_ENTER:
                updateHighlights(highlightPath);
                if (!shouldUseDrillDown()) {
                    assert mFlyoutController != null;
                    mFlyoutController.onItemHovered(item, view, levelOfHoveredItem, highlightPath);
                }
                return true;
            case MotionEvent.ACTION_HOVER_EXIT:
                // Update highlights after a short delay. This is to prevent UI flicker when the
                // user moves the pointer from the parent item view to a flyout item view. We
                // receive an {@code ACTION_HOVER_EXIT} event to the parent view right before we
                // receive an {@code ACTION_HOVER_ENTER} event on the flyout view. If we faithfully
                // follow these, the parent item momentarily loses the hover style, so we ignore the
                // first exit event in case it's immediately followed by an enter event.
                if (!shouldUseDrillDown()) {
                    assert mFlyoutController != null;
                    mFlyoutController.cancelFlyoutDelay(view);
                }

                mPendingHoverExitRunnable =
                        () -> {
                            if (item.model.get(mKeyProvider.getIsHighlightedKey())) {
                                updateHighlights(
                                        highlightPath.subList(0, highlightPath.size() - 1));
                            }
                            mPendingHoverExitRunnable = null;
                        };
                mHoverExitDelayHandler = view.getHandler();
                assert mHoverExitDelayHandler != null;
                mHoverExitDelayHandler.postDelayed(
                        mPendingHoverExitRunnable,
                        view.getContext()
                                .getResources()
                                .getInteger(R.integer.flyout_menu_hover_exit_delay_in_ms));

                // We only want to remove the flyout popups when the user hovers
                // over another item. We don't close the flyout popup even when the
                // item itself loses hover.
                return true;
            default:
                return false;
        }
    }

    /**
     * Callback to use when a menu item of type MENU_ITEM_WITH_SUBMENU is clicked.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param item The menu item which was clicked.
     */
    private void onItemWithSubmenuClicked(
            @Nullable ModelList headerModelList, ModelList contentModelList, ListItem item) {
        if (!shouldUseDrillDown()) {
            return;
        }

        @Nullable ModelList parentHeaderModelList =
                headerModelList == null ? null : shallowCopy(headerModelList);
        ModelList parentModelList = shallowCopy(contentModelList);
        // Add the clicked item as a header to the submenu.
        Runnable headerBackClick =
                () -> {
                    if (headerModelList != null && parentHeaderModelList != null) {
                        setModelListContent(headerModelList, parentHeaderModelList);
                    }
                    setModelListContent(contentModelList, parentModelList);
                };

        ListItem headerItem = mSubmenuHeaderFactory.createHeaderItem(item, headerBackClick);
        List<ListItem> newContentList = new ArrayList<>();
        if (headerModelList == null) {
            newContentList.add(headerItem);
        } else {
            headerModelList.set(List.of(headerItem));
        }
        newContentList.addAll(item.model.get(mKeyProvider.getSubmenuItemsKey()));
        contentModelList.set(newContentList);
    }

    private void setModelListContent(ModelList modelList, ModelList target) {
        List<ListItem> targetItems = new ArrayList<>();
        for (ListItem item : target) {
            targetItems.add(item);
        }
        modelList.set(targetItems);
    }

    /** Returns a shallow copy of {@param modelList}. */
    private ModelList shallowCopy(ModelList modelList) {
        ModelList result = new ModelList();
        for (ListItem item : modelList) {
            result.add(item);
        }
        return result;
    }

    /** Returns whether {@param item} has a click listener. */
    public boolean hasClickListener(ListItem item) {
        return item.model != null
                && item.model.containsKey(mKeyProvider.getClickListenerKey())
                && item.model.get(mKeyProvider.getClickListenerKey()) != null;
    }

    /**
     * Makes {@param dismissDialog} run at the end of the callback of {@param item}. If the item
     * doesn't already have a click callback in its model, no click callback is added.
     *
     * @param item The item to which we would add {@param runnable}.
     * @param dismissDialog The {@link Runnable} to run to dismiss the dialog.
     */
    private void addRunnableToCallback(ListItem item, Runnable dismissDialog) {
        if (hasClickListener(item)) {
            View.OnClickListener oldListener = item.model.get(mKeyProvider.getClickListenerKey());
            item.model.set(
                    mKeyProvider.getClickListenerKey(),
                    (view) -> {
                        oldListener.onClick(view);
                        dismissDialog.run();
                    });
        }
    }

    /**
     * Sets up the necessary callbacks for a menu item and its sub-items, recursively. This includes
     * setting hover listener for flyout menus and click listener for drill-down menus. It also
     * attaches the {@param dismissDialog} runnable to the click handlers of terminal items.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param item The item to start with.
     * @param dismissDialog The {@link Runnable} to run.
     */
    private void setupCallbacksRecursivelyForItem(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            ListItem item,
            Runnable dismissDialog,
            int levelOfHoveredItem,
            List<ListItem> ancestorPath) {
        if (item.model == null) return;

        List<ListItem> highlightPath = new ArrayList<ListItem>(ancestorPath);
        highlightPath.add(item);

        // We add hover listener to items without submenus too because we might need to dismiss
        // open flyout popups.
        if (item.model.containsKey(mKeyProvider.getHoverListenerKey())) {
            item.model.set(
                    mKeyProvider.getHoverListenerKey(),
                    (view, event) -> {
                        return handleHoverEvent(
                                event, item, view, levelOfHoveredItem, highlightPath);
                    });

            View.OnKeyListener originalListener = item.model.get(mKeyProvider.getKeyListenerKey());
            item.model.set(
                    mKeyProvider.getKeyListenerKey(),
                    (view, keyCode, keyEvent) -> {
                        if (isGoBackward(keyEvent)) {
                            if (!shouldUseDrillDown()) {
                                assert mFlyoutController != null;
                                mFlyoutController.exitFlyoutWithoutDelay(
                                        levelOfHoveredItem, view, highlightPath);
                            }
                            return true;
                        }

                        if (originalListener != null) {
                            return originalListener.onKey(view, keyCode, keyEvent);
                        }

                        // Return false because the listener has not consumed the event.
                        return false;
                    });
        }

        if (item.model.containsKey(mKeyProvider.getSubmenuItemsKey())) {
            final View.OnClickListener existingListener =
                    item.model.get(mKeyProvider.getClickListenerKey());
            item.model.set(
                    mKeyProvider.getClickListenerKey(),
                    (view) -> {
                        if (existingListener != null) {
                            existingListener.onClick(view);
                        }
                        if (shouldUseDrillDown()) {
                            onItemWithSubmenuClicked(headerModelList, contentModelList, item);
                        } else if (mFlyoutController != null) {
                            // Allow for controlling flyout with keyboard for accessibility.
                            mFlyoutController.enterFlyoutWithoutDelay(
                                    item, view, levelOfHoveredItem, highlightPath);
                        }
                    });
            for (ListItem submenuItem :
                    PropertyModel.getFromModelOrDefault(
                            item.model, mKeyProvider.getSubmenuItemsKey(), List.of())) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        submenuItem,
                        dismissDialog,
                        levelOfHoveredItem + 1,
                        highlightPath);
            }
        } else {
            // Note: SUBMENU_HEADER items should be (and are) excluded by this, because
            // SUBMENU_HEADER items aren't in the model's SUBMENU_ITEMS.
            // MENU_ITEM_WITH_SUBMENU items should also not be included.
            // The rationale for excluding these is that we don't want to dismiss the dialog when we
            // are navigating through submenus.
            addRunnableToCallback(item, dismissDialog);
        }
    }

    /**
     * Runs {@param dismissDialog} at the end of each callback, recursively (through submenu items).
     * If an item doesn't already have a click callback in its model, no click callback is added.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param dismissDialog The {@link Runnable} to run.
     */
    public void setupCallbacksRecursively(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            Runnable dismissDialog) {
        long time = SystemClock.elapsedRealtime();
        if (headerModelList != null) {
            for (ListItem listItem : headerModelList) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        listItem,
                        dismissDialog,
                        /* levelOfHoveredItem= */ 0,
                        new ArrayList<ListItem>());
            }
        }
        for (ListItem listItem : contentModelList) {
            setupCallbacksRecursivelyForItem(
                    headerModelList,
                    contentModelList,
                    listItem,
                    dismissDialog,
                    /* levelOfHoveredItem= */ 0,
                    new ArrayList<ListItem>());
        }
        RecordHistogram.recordTimesHistogram(
                "ListMenuUtils.SetupCallbacksRecursively.Duration",
                SystemClock.elapsedRealtime() - time);
    }

    /**
     * Set the focus state for a given content view. This is to make sure that hover navigation,
     * keyboard navigation and the combination of both work for flyout menus. We have to make sure
     * that only the top flyout popup is focused, and that the {@code ListView} in the other popups
     * don't get focus when the device exits touch mode due to e.g. keyboard activity.
     *
     * @param contentView The content view to change the focus settings for.
     * @param hasFocus Whether this content view should have focus.
     */
    public static void setWindowFocusForFlyoutMenus(ViewGroup contentView, boolean hasFocus) {
        if (hasFocus) {
            contentView.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
            contentView.requestFocus();
        } else {
            contentView.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
            contentView.clearFocus();
        }
    }

    /**
     * Populates a PropertyModel.Builder with the default properties for a submenu header.
     *
     * <p>This helper is for use by {@link SubmenuHeaderFactory} implementers to easily set up the
     * required header behavior while allowing them to add their own custom properties.
     *
     * @param builder The builder to populate.
     * @param keyProvider The key provider to use for setting properties.
     * @param title The title of the parent menu item with submenu.
     * @param backRunnable The runnable to execute for back navigation.
     */
    public static void populateDefaultHeaderProperties(
            PropertyModel.Builder builder,
            HierarchicalMenuKeyProvider keyProvider,
            CharSequence title,
            Runnable backRunnable) {
        builder.with(keyProvider.getTitleKey(), title)
                .with(keyProvider.getEnabledKey(), true)
                .with(keyProvider.getClickListenerKey(), (unusedView) -> backRunnable.run())
                .with(
                        keyProvider.getKeyListenerKey(),
                        (view, keyCode, keyEvent) -> {
                            if (isGoBackward(keyEvent)) {
                                backRunnable.run();
                                return true;
                            }
                            // Return false because the listener has not consumed the event.
                            return false;
                        });
    }

    /** Watches a ModelList and updates the accessibility pane title of the View accordingly. */
    public class AccessibilityListObserver implements ListObserver<Void> {

        private final View mView;
        private final @Nullable ListView mHeaderView;
        private final ListView mContentView;
        private final Context mContext;
        private final @Nullable ModelList mHeaderModelList;
        private final ModelList mContentModelList;

        /**
         * Returns a {@link AccessibilityListObserver} that reacts to changes in {@param
         * headerModelList and {@param contentModelList}, the are backing models for {@param view}.
         */
        public AccessibilityListObserver(
                View parentView,
                @Nullable ListView headerView,
                ListView contentView,
                @Nullable ModelList headerModelList,
                ModelList contentModelList) {
            mView = parentView;
            mHeaderView = headerView;
            mContentView = contentView;
            mContext = parentView.getContext();
            mHeaderModelList = headerModelList;
            mContentModelList = contentModelList;
        }

        // Note: because HierarchicalMenuController methods use ModelList#set, they trigger
        // onItemRangeChanged.
        @Override
        public void onItemRangeChanged(
                ListObservable<Void> source, int index, int count, @Nullable Void payload) {
            if (index != 0) return; // If the 1st element wasn't changed, the "header" is the same.
            String accessibilityPaneTitle =
                    mContext.getString(R.string.hierarchicalmenu_a11y_default_pane_title);
            Object firstItem = null;
            if (mHeaderModelList != null && !mHeaderModelList.isEmpty()) {
                firstItem = mHeaderModelList.get(0);
            } else if (!mContentModelList.isEmpty()) {
                firstItem = mContentModelList.get(0);
            }
            if (firstItem instanceof ListItem firstListItem && firstListItem.model != null) {
                WritableObjectPropertyKey<CharSequence> titleKey = mKeyProvider.getTitleKey();
                WritableIntPropertyKey titleIdKey = mKeyProvider.getTitleIdKey();

                if (firstListItem.model.containsKey(titleKey)
                        && firstListItem.model.get(titleKey) != null) {
                    CharSequence title = firstListItem.model.get(titleKey);
                    if (title.length() != 0) {
                        accessibilityPaneTitle = String.valueOf(title);
                    }
                } else if (firstListItem.model.containsKey(titleIdKey)) {
                    @StringRes int titleId = firstListItem.model.get(titleIdKey);
                    if (titleId != Resources.ID_NULL) {
                        accessibilityPaneTitle =
                                mContext.getString(firstListItem.model.get(titleIdKey));
                    }
                }
            }
            ViewCompat.setAccessibilityPaneTitle(mView, accessibilityPaneTitle);
            // The method calls below ensure that when we transition to a different submenu, the
            // keyboard focus goes to the topmost element.
            mContentView.setSelection(0);
            if (mHeaderView != null && mHeaderModelList != null && !mHeaderModelList.isEmpty())
                mHeaderView.setSelection(0);
            mView.requestFocus();
        }
    }
}
