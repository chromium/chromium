// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.SubmenuHeaderFactory;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.FlyoutPopupSpecCalculator;
import org.chromium.ui.widget.RectProvider;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Instrumentation tests for the hierarchical menu (drilldown and flyout). */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class HierarchicalMenuTest {

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private BlankUiTestActivity mActivity;

    private HierarchicalMenuController<AnchoredPopupWindow> mController;
    private FlyoutController<AnchoredPopupWindow> mFlyoutController;
    private FlyoutHandlerImpl mFlyoutHandler;

    private View mRootView;
    private AnchoredPopupWindow mPopupWindow;
    private View mPopupView;

    private AtomicReference<String> mExecutedItemRunnableTitle;
    private AtomicBoolean mDismissRunnableExecuted;

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchActivity(/* startIntent= */ null);
        mDismissRunnableExecuted = new AtomicBoolean(false);
        mExecutedItemRunnableTitle = new AtomicReference<String>(null);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout activityContent = new FrameLayout(mActivity);
                    mActivity.setContentView(activityContent);

                    mRootView = new View(mActivity);
                    mRootView.setLayoutParams(
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));
                    mRootView.setBackgroundColor(Color.BLUE);
                    activityContent.addView(mRootView);

                    List<ListItem> items =
                            Arrays.asList(
                                    item(
                                            "Item 1 >",
                                            item(
                                                    "Item 1-1 >",
                                                    item("Item 1-1-1"),
                                                    item("Item 1-1-2"),
                                                    item("Item 1-1-3")),
                                            item(
                                                    "Item 1-2 >",
                                                    item("Item 1-2-1"),
                                                    item("Item 1-2-2"),
                                                    item("Item 1-2-3")),
                                            item("Item 1-3"),
                                            item("Item 1-4")),
                                    item(
                                            "Item 2 >",
                                            item("Item 2-1"),
                                            item("Item 2-2"),
                                            item("Item 2-3")),
                                    item("Item 3"),
                                    item("Item 4"));

                    HierarchicalMenuKeyProvider keyProvider =
                            HierarchicalMenuTestUtils.createKeyProvider();

                    SubmenuHeaderFactory submenuHeaderFactory =
                            (clickedItem, backRunnable) -> {
                                PropertyModel.Builder builder =
                                        new PropertyModel.Builder(
                                                HierarchicalMenuTestUtils.ALL_MENU_ITEM_KEYS);
                                HierarchicalMenuController.populateDefaultHeaderProperties(
                                        builder, keyProvider, "Header", backRunnable);
                                return new ListItem(
                                        HierarchicalMenuTestUtils.MENU_ITEM_SUBMENU_HEADER,
                                        builder.build());
                            };

                    mController =
                            new HierarchicalMenuController<>(
                                    mActivity, keyProvider, submenuHeaderFactory);

                    ModelList modelList = createModelList(items);
                    mController.setupCallbacksRecursively(
                            /* headerModelList= */ null,
                            modelList,
                            /* dismissDialog= */ () -> {
                                mDismissRunnableExecuted.set(true);
                                mController.destroyFlyoutController();
                            });

                    mPopupWindow =
                            new AnchoredPopupWindow.Builder(
                                            mActivity,
                                            mRootView,
                                            new ColorDrawable(Color.TRANSPARENT),
                                            () -> createStyledListView(mActivity, modelList),
                                            new RectProvider(new Rect(0, 0, 100, 100)))
                                    .setVerticalOverlapAnchor(false)
                                    .setHorizontalOverlapAnchor(true)
                                    .setFocusable(true)
                                    .setMaxWidth(300)
                                    .setOutsideTouchable(true)
                                    .addOnDismissListener(
                                            () -> {
                                                mController.destroyFlyoutController();
                                            })
                                    .build();
                    mPopupWindow.show();

                    mFlyoutHandler = new FlyoutHandlerImpl();
                    mController.setupFlyoutController(
                            mFlyoutHandler, mPopupWindow, /* drillDownOverrideValue= */ null);
                    mFlyoutController = mController.getFlyoutController();
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(
                () -> {
                    View content = mPopupWindow.getContentView();
                    return content != null && content.hasWindowFocus();
                },
                "Timed out waiting for Flyout Popup to gain window focus");
        mPopupView = mPopupWindow.getContentView();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mController != null && mController.getFlyoutController() != null) {
                        mController.destroyFlyoutController();
                    }
                });
    }

    @Test
    @MediumTest
    public void testHoverCreatesAnchoredFlyout() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HierarchicalMenuController.setDrillDownOverrideValueForTesting(false);
                });

        // Hover on Item 1.
        View item1 =
                waitForViewWithText(getRootViewWithPopupIndex(mFlyoutController, 0), "Item 1 >");
        Assert.assertNotNull("Could not find Item 1", item1);
        dispatchHoverEnterEvent(item1);
        CriteriaHelper.pollUiThread(
                () -> mFlyoutController.getNumberOfPopups() == 2, "Popup count did not reach 2");

        // Hover on Item 1-1 (in the new 2nd popup).
        View item1Sub1 =
                waitForViewWithText(getRootViewWithPopupIndex(mFlyoutController, 1), "Item 1-1 >");
        dispatchHoverEnterEvent(item1Sub1);
        CriteriaHelper.pollUiThread(
                () -> mFlyoutController.getNumberOfPopups() == 3, "Popup count did not reach 3");

        // Hover on Item 2 (in the 1st popup, which is now in the background).
        View item2 =
                waitForViewWithText(getRootViewWithPopupIndex(mFlyoutController, 0), "Item 2 >");
        dispatchHoverEnterEvent(item2);
        CriteriaHelper.pollUiThread(
                () -> mFlyoutController.getNumberOfPopups() == 2, "Popup count did not reach 2");

        // Hover on Item 3 (in the 1st popup).
        View item3 = waitForViewWithText(getRootViewWithPopupIndex(mFlyoutController, 0), "Item 3");
        Assert.assertNotNull("Could not find Item 3", item3);
        dispatchHoverEnterEvent(item3);
        CriteriaHelper.pollUiThread(
                () -> mFlyoutController.getNumberOfPopups() == 1, "Popup count did not reach 1");
    }

    @Test
    @MediumTest
    public void testDrillDownInteraction() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HierarchicalMenuController.setDrillDownOverrideValueForTesting(true);
                });

        // Initially, Item 1 and Item 2 should be visible, but Item 1-1 should not.
        Assert.assertNotNull(
                "Item 1 should be visible", waitForViewWithText(mPopupView, "Item 1 >"));
        Assert.assertNotNull(
                "Item 2 should be visible", waitForViewWithText(mPopupView, "Item 2 >"));
        Assert.assertNull(
                "Item 1-1 should not be visible anymore",
                findViewWithText(mPopupView, "Item 1-1 >"));
        Assert.assertNull(
                "Header should not be visible anymore", findViewWithText(mPopupView, "Header"));

        // Click on Item 1 to drill down.
        View item1 = waitForViewWithText(mPopupView, "Item 1 >");
        ThreadUtils.runOnUiThreadBlocking(item1::performClick);

        // Now, Item 1-1 and the Header should be visible. Item 1 and Item 2 should not.
        Assert.assertNotNull("Header should be visible", waitForViewWithText(mPopupView, "Header"));
        Assert.assertNotNull(
                "Item 1-1 should be visible", waitForViewWithText(mPopupView, "Item 1-1 >"));
        Assert.assertNull("Item 1 should not be visible", findViewWithText(mPopupView, "Item 1 >"));
        Assert.assertNull("Item 2 should not be visible", findViewWithText(mPopupView, "Item 2 >"));

        // Click on the "Header" to go back.
        View header = waitForViewWithText(mPopupView, "Header");
        ThreadUtils.runOnUiThreadBlocking(header::performClick);

        // We should be back to the initial state.
        Assert.assertNotNull(
                "Item 1 should be visible again", waitForViewWithText(mPopupView, "Item 1 >"));
        Assert.assertNotNull(
                "Item 2 should be visible again", waitForViewWithText(mPopupView, "Item 2 >"));
        Assert.assertNull(
                "Item 1-1 should not be visible anymore",
                findViewWithText(mPopupView, "Item 1-1 >"));
        Assert.assertNull(
                "Header should not be visible anymore", findViewWithText(mPopupView, "Header"));
    }

    @Test
    @MediumTest
    public void testFlyoutTerminalItemClick() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HierarchicalMenuController.setDrillDownOverrideValueForTesting(false);
                });

        // Hover on Item 1 to open first flyout.
        View item1 =
                waitForViewWithText(getRootViewWithPopupIndex(mFlyoutController, 0), "Item 1 >");
        dispatchHoverEnterEvent(item1);
        CriteriaHelper.pollUiThread(
                () -> mFlyoutController.getNumberOfPopups() == 2, "Popup count did not reach 2");

        // Click on "Item 1-3".
        View item1Sub3 =
                waitForViewWithText(getRootViewWithPopupIndex(mFlyoutController, 1), "Item 1-3");
        Assert.assertNotNull("Item 1-3 should be visible", item1Sub3);
        Assert.assertNull(
                "No item runnable should have been executed", mExecutedItemRunnableTitle.get());
        ThreadUtils.runOnUiThreadBlocking(item1Sub3::performClick);

        CriteriaHelper.pollUiThread(
                () -> "Item 1-3".equals(mExecutedItemRunnableTitle.get()),
                "Runnable for Item 1-3 was not executed");
        CriteriaHelper.pollUiThread(
                () -> mDismissRunnableExecuted.get(), "Dismiss runnable was not executed");
    }

    @Test
    @MediumTest
    public void testDrillDownTerminalItemClick() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HierarchicalMenuController.setDrillDownOverrideValueForTesting(true);
                });

        // Click on "Item 1" to drill down.
        View item1 = waitForViewWithText(mPopupView, "Item 1 >");
        ThreadUtils.runOnUiThreadBlocking(item1::performClick);

        // Click on "Item 1-3".
        Assert.assertNotNull(
                "Item 1-3 should be visible", waitForViewWithText(mPopupView, "Item 1-3"));
        View item1Sub3 = waitForViewWithText(mPopupView, "Item 1-3");
        ThreadUtils.runOnUiThreadBlocking(item1Sub3::performClick);

        CriteriaHelper.pollUiThread(
                () -> "Item 1-3".equals(mExecutedItemRunnableTitle.get()),
                "Runnable for Item 1-3 was not executed");
        CriteriaHelper.pollUiThread(
                () -> mDismissRunnableExecuted.get(), "Dismiss runnable was not executed");
    }

    /** Manually dispatches a {@code HOVER_ENTER} event to a specific target view. */
    private void dispatchHoverEnterEvent(View target) {
        long now = SystemClock.uptimeMillis();
        MotionEvent event =
                MotionEvent.obtain(
                        now,
                        now,
                        MotionEvent.ACTION_HOVER_ENTER,
                        target.getWidth() / 2f,
                        target.getHeight() / 2f,
                        /* metaState= */ 0);
        event.setSource(InputDevice.SOURCE_CLASS_POINTER);

        ThreadUtils.runOnUiThreadBlocking(() -> target.dispatchGenericMotionEvent(event));
        event.recycle();
    }

    /**
     * Helper to create a styled ListView for the popup content. Includes a custom adapter to render
     * text with specific padding and borders.
     *
     * @param context The context for the view.
     * @param items The list of items to display.
     * @return The configured ListView.
     */
    private static ListView createStyledListView(Context context, ModelList items) {
        ListView listView = new ListView(context);
        listView.setBackgroundColor(Color.WHITE);
        listView.setDivider(null);

        ModelListAdapter adapter = new ModelListAdapter(items);

        ViewBinder<PropertyModel, TextView, PropertyKey> binder =
                (model, view, key) -> {
                    if (key == HierarchicalMenuTestUtils.TITLE) {
                        view.setText(model.get(HierarchicalMenuTestUtils.TITLE));
                    } else if (key == HierarchicalMenuTestUtils.IS_HIGHLIGHTED) {
                        boolean isHighlighted = model.get(HierarchicalMenuTestUtils.IS_HIGHLIGHTED);
                        view.setHovered(isHighlighted);
                        ((GradientDrawable) view.getBackground())
                                .setColor(isHighlighted ? Color.LTGRAY : Color.WHITE);
                    } else if (key == HierarchicalMenuTestUtils.CLICK_LISTENER) {
                        view.setOnClickListener(
                                model.get(HierarchicalMenuTestUtils.CLICK_LISTENER));
                    } else if (key == HierarchicalMenuTestUtils.HOVER_LISTENER) {
                        view.setOnHoverListener(
                                model.get(HierarchicalMenuTestUtils.HOVER_LISTENER));
                    }
                };

        MVCListAdapter.ViewBuilder<TextView> builder =
                parent -> {
                    TextView view = new TextView(context);
                    view.setPadding(30, 30, 30, 30);
                    GradientDrawable border = new GradientDrawable();
                    border.setColor(Color.WHITE);
                    border.setStroke(1, Color.LTGRAY);
                    view.setBackground(border);
                    return view;
                };

        adapter.registerType(HierarchicalMenuTestUtils.MENU_ITEM, builder, binder);
        adapter.registerType(HierarchicalMenuTestUtils.MENU_ITEM_WITH_SUBMENU, builder, binder);
        adapter.registerType(HierarchicalMenuTestUtils.MENU_ITEM_SUBMENU_HEADER, builder, binder);

        listView.setAdapter(adapter);
        return listView;
    }

    /**
     * Concrete implementation of FlyoutHandler for testing purposes. Manages the creation,
     * positioning, and dismissal of {@link AnchoredPopupWindow}s.
     */
    private class FlyoutHandlerImpl implements FlyoutHandler<AnchoredPopupWindow> {
        @Override
        public Rect getPopupRect(AnchoredPopupWindow popupWindow) {
            return ListMenuUtils.getViewRectRelativeToItsRootView(popupWindow.getContentView());
        }

        @Override
        public void dismissPopup(AnchoredPopupWindow popupWindow) {
            popupWindow.dismiss();
        }

        @Override
        public void setWindowFocus(AnchoredPopupWindow popupWindow, boolean hasFocus) {
            popupWindow.setFocusable(hasFocus);
        }

        @Override
        public AnchoredPopupWindow createAndShowFlyoutPopup(
                ListItem item, View anchorView, Runnable dismissRunnable) {
            Rect anchorRect = FlyoutController.calculateFlyoutAnchorRect(anchorView, mRootView);
            anchorRect.offset(0, (int) topContentOffset(mActivity));

            AnchoredPopupWindow window =
                    new AnchoredPopupWindow.Builder(
                                    mActivity,
                                    mRootView,
                                    new ColorDrawable(Color.TRANSPARENT),
                                    () ->
                                            createStyledListView(
                                                    mActivity,
                                                    createModelList(
                                                            item.model.get(
                                                                    HierarchicalMenuTestUtils
                                                                            .SUBMENU_ITEMS))),
                                    new RectProvider(anchorRect))
                            .setVerticalOverlapAnchor(true)
                            .setHorizontalOverlapAnchor(false)
                            .setFocusable(true)
                            .setTouchModal(false)
                            .setAnimateFromAnchor(false)
                            .setMaxWidth(300)
                            .setSpecCalculator(new FlyoutPopupSpecCalculator())
                            .addOnDismissListener(dismissRunnable::run)
                            .build();
            window.show();

            return window;
        }
    }

    /** Helper to create a {@link ListItem} with the given text and optional sub-items. */
    private ListItem item(String text, ListItem... subItems) {
        boolean isGroup = subItems.length > 0;
        ListItem item = createMenuItem(text, isGroup);
        if (isGroup) {
            item.model.set(HierarchicalMenuTestUtils.SUBMENU_ITEMS, Arrays.asList(subItems));
        }
        return item;
    }

    /** Helper to construct the {@link PropertyModel} for a menu item. */
    private ListItem createMenuItem(String title, boolean hasSubmenu) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(
                        hasSubmenu
                                ? HierarchicalMenuTestUtils.ALL_SUBMENU_ITEM_KEYS
                                : HierarchicalMenuTestUtils.ALL_MENU_ITEM_KEYS);
        builder.with(HierarchicalMenuTestUtils.TITLE, title);
        builder.with(
                HierarchicalMenuTestUtils.CLICK_LISTENER,
                (view) -> {
                    mExecutedItemRunnableTitle.set(title);
                });
        return new ListItem(
                hasSubmenu
                        ? HierarchicalMenuTestUtils.MENU_ITEM_WITH_SUBMENU
                        : HierarchicalMenuTestUtils.MENU_ITEM,
                builder.build());
    }

    /** Helper to wrap a list of {@link ListItem}s into a {@link ModelList}. */
    private static ModelList createModelList(List<ListItem> items) {
        ModelList list = new ModelList();
        for (ListItem item : items) {
            list.add(item);
        }
        return list;
    }

    /** Calculates the vertical offset of the visible window content to correct coordinates. */
    private float topContentOffset(Activity activity) {
        View view = activity.getWindow().getDecorView();
        Rect windowVisibleRect = new Rect();
        view.getWindowVisibleDisplayFrame(windowVisibleRect);
        int[] windowRootCoordinates = new int[2];
        view.getLocationOnScreen(windowRootCoordinates);
        return windowVisibleRect.top - windowRootCoordinates[1];
    }

    /**
     * Polls the UI thread until a view with the specific text is found and attached to the window.
     * This custom implementation is necessary because Espresso's {@code onViewWaiting()} matchers
     * do not reliably match views inside unfocused windows. This is critical when testing flyout
     * menus, where the flyout popup holds focus but we still need to verify or interact with views
     * in the background window.
     */
    private View waitForViewWithText(View root, String text) {
        final View[] result = new View[1];
        CriteriaHelper.pollUiThread(
                () -> {
                    View found = findViewWithText(root, text);
                    if (found != null && found.isAttachedToWindow()) {
                        result[0] = found;
                        return true;
                    }
                    return false;
                },
                "Timed out waiting for attached view with text: " + text);
        return result[0];
    }

    /** Recursively searches for a view containing the specified text within a view hierarchy. */
    private static View findViewWithText(View root, String text) {
        if (root instanceof TextView) {
            if (text.equals(((TextView) root).getText().toString())) {
                return root;
            }
        }
        if (root instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                View found = findViewWithText(group.getChildAt(i), text);
                if (found != null) return found;
            }
        }
        return null;
    }

    /** Helper to get the root view of a specific popup window managed by the controller. */
    private View getRootViewWithPopupIndex(
            FlyoutController<AnchoredPopupWindow> controller, int index) {
        List<AnchoredPopupWindow> popups = controller.getPopupsForTest();
        assert index < popups.size();
        return popups.get(index).getContentView().getRootView();
    }
}
