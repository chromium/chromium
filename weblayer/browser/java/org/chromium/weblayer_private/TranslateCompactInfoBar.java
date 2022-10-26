// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.core.content.ContextCompat;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.infobar.ActionType;
import org.chromium.chrome.browser.infobar.InfobarEvent;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;
import org.chromium.components.translate.TranslateMenu;
import org.chromium.components.translate.TranslateMenuHelper;
import org.chromium.components.translate.TranslateOption;
import org.chromium.components.translate.TranslateOptions;
import org.chromium.components.translate.TranslateTabLayout;
import org.chromium.ui.widget.Toast;

/**
 * Java version of the compact translate infobar.
 */
@JNINamespace("weblayer")
public class TranslateCompactInfoBar extends InfoBar
        implements TabLayout.OnTabSelectedListener, TranslateMenuHelper.TranslateMenuListener {
    public static final int TRANSLATING_INFOBAR = 1;
    public static final int AFTER_TRANSLATING_INFOBAR = 2;

    private static final int SOURCE_TAB_INDEX = 0;
    private static final int TARGET_TAB_INDEX = 1;

    // Action ID for Snackbar.
    // Actions performed by clicking on on the overflow menu.
    public static final int ACTION_OVERFLOW_ALWAYS_TRANSLATE = 0;
    public static final int ACTION_OVERFLOW_NEVER_SITE = 1;
    public static final int ACTION_OVERFLOW_NEVER_LANGUAGE = 2;
    // Actions triggered automatically. (when translation or denied count reaches the threshold.)
    public static final int ACTION_AUTO_ALWAYS_TRANSLATE = 3;
    public static final int ACTION_AUTO_NEVER_LANGUAGE = 4;

    private final int mInitialStep;
    private final int mDefaultTextColor;
    private final TranslateOptions mOptions;

    private long mNativeTranslateInfoBarPtr;
    private TranslateTabLayout mTabLayout;

    // Metric to track the total number of translations in a page, including reverts to original.
    private int mTotalTranslationCount;

    // Histogram names for logging metrics.
    private static final String INFOBAR_HISTOGRAM_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.Translate";
    private static final String INFOBAR_HISTOGRAM_MORE_LANGUAGES_LANGUAGE =
            "Translate.CompactInfobar.Language.MoreLanguages";
    private static final String INFOBAR_HISTOGRAM_PAGE_NOT_IN_LANGUAGE =
            "Translate.CompactInfobar.Language.PageNotIn";
    private static final String INFOBAR_HISTOGRAM_ALWAYS_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.AlwaysTranslate";
    private static final String INFOBAR_HISTOGRAM_NEVER_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.NeverTranslate";
    private static final String INFOBAR_HISTOGRAM = "Translate.CompactInfobar.Event";

    // Need 2 instances of TranslateMenuHelper to prevent a race condition bug which happens when
    // showing language menu after dismissing overflow menu.
    private TranslateMenuHelper mOverflowMenuHelper;
    private TranslateMenuHelper mLanguageMenuHelper;

    private ImageButton mMenuButton;
    private InfoBarCompactLayout mParent;

    private boolean mMenuExpanded;
    private boolean mIsFirstLayout = true;
    private boolean mUserInteracted;

    @CalledByNative
    private static InfoBar create(TabImpl tab, int initialStep, String sourceLanguageCode,
            String targetLanguageCode, boolean neverLanguage, boolean neverDomain,
            boolean alwaysTranslate, boolean triggeredFromMenu, String[] languages,
            String[] languageCodes, int[] hashCodes, int tabTextColor) {
        recordInfobarAction(InfobarEvent.INFOBAR_IMPRESSION);
        return new TranslateCompactInfoBar(initialStep, sourceLanguageCode, targetLanguageCode,
                neverLanguage, neverDomain, alwaysTranslate, triggeredFromMenu, languages,
                languageCodes, hashCodes, tabTextColor);
    }

    TranslateCompactInfoBar(int initialStep, String sourceLanguageCode, String targetLanguageCode,
            boolean neverLanguage, boolean neverDomain, boolean alwaysTranslate,
            boolean triggeredFromMenu, String[] languages, String[] languageCodes, int[] hashCodes,
            int tabTextColor) {
        super(R.drawable.infobar_translate_compact, 0, null, null);

        mInitialStep = initialStep;
        mDefaultTextColor = tabTextColor;
        mOptions = TranslateOptions.create(sourceLanguageCode, targetLanguageCode, languages,
                languageCodes, neverLanguage, neverDomain, alwaysTranslate, triggeredFromMenu,
                hashCodes,
                /*contentLanguagesCodes*/ null);
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout parent) {
        LinearLayout content;
        // LayoutInflater may trigger accessing the disk.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            content = (LinearLayout) LayoutInflater.from(getContext())
                              .inflate(R.layout.weblayer_infobar_translate_compact_content, parent,
                                      false);
        }

        // When parent tab is being switched out (view detached), dismiss all menus and snackbars.
        content.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                dismissMenusAndSnackbars();
            }
        });

        mTabLayout =
                (TranslateTabLayout) content.findViewById(R.id.weblayer_translate_infobar_tabs);
        if (mDefaultTextColor > 0) {
            mTabLayout.setTabTextColors(
                    ContextCompat.getColor(getContext(), R.color.default_text_color_baseline),
                    ContextCompat.getColor(
                            getContext(), R.color.weblayer_tab_layout_selected_tab_color));
        }
        mTabLayout.addTabs(mOptions.sourceLanguageName(), mOptions.targetLanguageName());

        if (mInitialStep == TRANSLATING_INFOBAR) {
            // Set translating status in the beginning for pages translated automatically.
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            mUserInteracted = true;
        } else if (mInitialStep == AFTER_TRANSLATING_INFOBAR) {
            // Focus on target tab since we are after translation.
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        }

        mTabLayout.addOnTabSelectedListener(this);

        // Dismiss all menus and end scrolling animation when there is layout changed.
        mTabLayout.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (left != oldLeft || top != oldTop || right != oldRight || bottom != oldBottom) {
                    // Dismiss all menus to prevent menu misplacement.
                    dismissMenus();

                    if (mIsFirstLayout) {
                        // Scrolls to the end to make sure the target language tab is visible when
                        // language tabs is too long.
                        mTabLayout.startScrollingAnimationIfNeeded();
                        mIsFirstLayout = false;
                        return;
                    }

                    // End scrolling animation when layout changed.
                    mTabLayout.endScrollingAnimationIfPlaying();
                }
            }
        });

        mMenuButton = content.findViewById(R.id.weblayer_translate_infobar_menu_button);
        mMenuButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mTabLayout.endScrollingAnimationIfPlaying();
                recordInfobarAction(InfobarEvent.INFOBAR_OPTIONS);
                initMenuHelper(TranslateMenu.MENU_OVERFLOW);
                mOverflowMenuHelper.show(TranslateMenu.MENU_OVERFLOW, getParentWidth());
                mMenuExpanded = true;
            }
        });

        parent.addContent(content, 1.0f);
        mParent = parent;
    }

    private void initMenuHelper(int menuType) {
        boolean isIncognito = TranslateCompactInfoBarJni.get().isIncognito(
                mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this);
        boolean isSourceLangUnknown = mOptions.sourceLanguageCode().equals("und");
        switch (menuType) {
            case TranslateMenu.MENU_OVERFLOW:
                mOverflowMenuHelper = new TranslateMenuHelper(getContext(), mMenuButton, mOptions,
                        this, isIncognito, isSourceLangUnknown);
                return;
            case TranslateMenu.MENU_TARGET_LANGUAGE:
            case TranslateMenu.MENU_SOURCE_LANGUAGE:
                if (mLanguageMenuHelper == null) {
                    mLanguageMenuHelper = new TranslateMenuHelper(getContext(), mMenuButton,
                            mOptions, this, isIncognito, isSourceLangUnknown);
                }
                return;
            default:
                assert false : "Unsupported Menu Item Id";
        }
    }

    private void startTranslating(int tabPosition) {
        if (TARGET_TAB_INDEX == tabPosition) {
            // Already on the target tab.
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            onButtonClicked(ActionType.TRANSLATE);
            mUserInteracted = true;
        } else {
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        }
    }

    @CalledByNative
    private void onPageTranslated(int errorType) {
        if (mTabLayout != null) {
            mTabLayout.hideProgressBar();
            if (errorType != 0) {
                Toast.makeText(getContext(), R.string.translate_infobar_error, Toast.LENGTH_SHORT)
                        .show();
                // Disable OnTabSelectedListener then revert selection.
                mTabLayout.removeOnTabSelectedListener(this);
                mTabLayout.getTabAt(SOURCE_TAB_INDEX).select();
                // Add OnTabSelectedListener back.
                mTabLayout.addOnTabSelectedListener(this);
            }
        }
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        mNativeTranslateInfoBarPtr = nativePtr;
    }

    @CalledByNative
    private void setAutoAlwaysTranslate() {
        createAndShowSnackbar(ACTION_AUTO_ALWAYS_TRANSLATE);
    }

    @Override
    protected void resetNativeInfoBar() {
        mNativeTranslateInfoBarPtr = 0;
        super.resetNativeInfoBar();
    }

    private void closeInfobar(boolean explicitly) {
        if (isDismissed()) return;

        if (!mUserInteracted) {
            recordInfobarAction(InfobarEvent.INFOBAR_DECLINE);
        }

        // NOTE: In Chrome there is a check for whether auto "never translate" should be triggered
        // via a snackbar here. However, WebLayer does not have snackbars and thus does not have
        // this check as there would be no way to inform the user of the functionality being
        // triggered. The user of course has the option of choosing "never translate" from the
        // overflow menu.

        // This line will dismiss this infobar.
        super.onCloseButtonClicked();
    }

    @Override
    public void onCloseButtonClicked() {
        mTabLayout.endScrollingAnimationIfPlaying();
        closeInfobar(true);
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        switch (tab.getPosition()) {
            case SOURCE_TAB_INDEX:
                recordInfobarAction(InfobarEvent.INFOBAR_REVERT);
                onButtonClicked(ActionType.TRANSLATE_SHOW_ORIGINAL);
                return;
            case TARGET_TAB_INDEX:
                recordInfobarAction(InfobarEvent.INFOBAR_TARGET_TAB_TRANSLATE);
                recordInfobarLanguageData(
                        INFOBAR_HISTOGRAM_TRANSLATE_LANGUAGE, mOptions.targetLanguageCode());
                startTranslating(TARGET_TAB_INDEX);
                return;
            default:
                assert false : "Unexpected Tab Index";
        }
    }

    @Override
    public void onTabUnselected(TabLayout.Tab tab) {}

    @Override
    public void onTabReselected(TabLayout.Tab tab) {}

    @Override
    public void onOverflowMenuItemClicked(int itemId) {
        switch (itemId) {
            case TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE:
                recordInfobarAction(InfobarEvent.INFOBAR_MORE_LANGUAGES);
                initMenuHelper(TranslateMenu.MENU_TARGET_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_TARGET_LANGUAGE, getParentWidth());
                return;
            case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                // Only show snackbar when "Always Translate" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)) {
                    recordInfobarAction(InfobarEvent.INFOBAR_ALWAYS_TRANSLATE);
                    recordInfobarLanguageData(INFOBAR_HISTOGRAM_ALWAYS_TRANSLATE_LANGUAGE,
                            mOptions.sourceLanguageCode());
                    createAndShowSnackbar(ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                } else {
                    recordInfobarAction(InfobarEvent.INFOBAR_ALWAYS_TRANSLATE_UNDO);
                    handleTranslateOverflowOption(ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                // Only show snackbar when "Never Translate" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE)) {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE);
                    recordInfobarLanguageData(INFOBAR_HISTOGRAM_NEVER_TRANSLATE_LANGUAGE,
                            mOptions.sourceLanguageCode());
                    createAndShowSnackbar(ACTION_OVERFLOW_NEVER_LANGUAGE);
                } else {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE_UNDO);
                    handleTranslateOverflowOption(ACTION_OVERFLOW_NEVER_LANGUAGE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                // Only show snackbar when "Never Translate Site" is enabled.
                if (!mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN)) {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE_SITE);
                    createAndShowSnackbar(ACTION_OVERFLOW_NEVER_SITE);
                } else {
                    recordInfobarAction(InfobarEvent.INFOBAR_NEVER_TRANSLATE_SITE_UNDO);
                    handleTranslateOverflowOption(ACTION_OVERFLOW_NEVER_SITE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                recordInfobarAction(InfobarEvent.INFOBAR_PAGE_NOT_IN);
                initMenuHelper(TranslateMenu.MENU_SOURCE_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_SOURCE_LANGUAGE, getParentWidth());
                return;
            default:
                assert false : "Unexpected overflow menu code";
        }
    }

    @Override
    public void onTargetMenuItemClicked(String code) {
        // Reset target code in both UI and native.
        if (mNativeTranslateInfoBarPtr != 0 && mOptions.setTargetLanguage(code)) {
            recordInfobarAction(InfobarEvent.INFOBAR_MORE_LANGUAGES_TRANSLATE);
            recordInfobarLanguageData(
                    INFOBAR_HISTOGRAM_MORE_LANGUAGES_LANGUAGE, mOptions.targetLanguageCode());
            TranslateCompactInfoBarJni.get().applyStringTranslateOption(mNativeTranslateInfoBarPtr,
                    TranslateCompactInfoBar.this, TranslateOption.TARGET_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(TARGET_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
        }
    }

    @Override
    public void onSourceMenuItemClicked(String code) {
        // Reset source code in both UI and native.
        if (mNativeTranslateInfoBarPtr != 0 && mOptions.setSourceLanguage(code)) {
            recordInfobarLanguageData(
                    INFOBAR_HISTOGRAM_PAGE_NOT_IN_LANGUAGE, mOptions.sourceLanguageCode());
            TranslateCompactInfoBarJni.get().applyStringTranslateOption(mNativeTranslateInfoBarPtr,
                    TranslateCompactInfoBar.this, TranslateOption.SOURCE_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(SOURCE_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
        }
    }

    // Dismiss all overflow menus that remains open.
    // This is called when infobar started hiding or layout changed.
    private void dismissMenus() {
        if (mOverflowMenuHelper != null) mOverflowMenuHelper.dismiss();
        if (mLanguageMenuHelper != null) mLanguageMenuHelper.dismiss();
    }

    // Dismiss all overflow menus and snackbars that belong to this infobar and remain open.
    private void dismissMenusAndSnackbars() {
        dismissMenus();
    }

    @Override
    protected void onStartedHiding() {
        dismissMenusAndSnackbars();
    }

    @Override
    protected CharSequence getAccessibilityMessage(CharSequence defaultMessage) {
        return getContext().getString(R.string.translate_button);
    }

    /**
     * Returns true if overflow menu is showing.  This is only used for automation testing.
     */
    public boolean isShowingOverflowMenuForTesting() {
        if (mOverflowMenuHelper == null) return false;
        return mOverflowMenuHelper.isShowing();
    }

    /**
     * Returns true if language menu is showing.  This is only used for automation testing.
     */
    public boolean isShowingLanguageMenuForTesting() {
        if (mLanguageMenuHelper == null) return false;
        return mLanguageMenuHelper.isShowing();
    }

    /**
     * Returns true if the tab at the given |tabIndex| is selected. This is only used for automation
     * testing.
     */
    private boolean isTabSelectedForTesting(int tabIndex) {
        return mTabLayout.getTabAt(tabIndex).isSelected();
    }

    /**
     * Returns true if the target tab is selected. This is only used for automation testing.
     */
    public boolean isSourceTabSelectedForTesting() {
        return this.isTabSelectedForTesting(SOURCE_TAB_INDEX);
    }

    /**
     * Returns true if the target tab is selected. This is only used for automation testing.
     */
    public boolean isTargetTabSelectedForTesting() {
        return this.isTabSelectedForTesting(TARGET_TAB_INDEX);
    }

    /**
     * Returns the name of the target language. This is only used for automation testing.
     */
    public String getTargetLanguageForTesting() {
        return mOptions.targetLanguageName();
    }

    private void createAndShowSnackbar(int actionId) {
        // NOTE: WebLayer doesn't have snackbars, so the relevant action is just taken directly.
        // TODO(blundell): If WebLayer ends up staying with this implementation long-term, update
        // the nomenclature of this file to avoid any references to snackbars.
        handleTranslateOverflowOption(actionId);
    }

    private void handleTranslateOverflowOption(int actionId) {
        // Quit if native is destroyed.
        if (mNativeTranslateInfoBarPtr == 0) return;

        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                toggleAlwaysTranslate();
                // Start translating if always translate is selected and if page is not already
                // translated to the target language.
                if (mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)
                        && mTabLayout.getSelectedTabPosition() == SOURCE_TAB_INDEX) {
                    startTranslating(mTabLayout.getSelectedTabPosition());
                }
                return;
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                toggleAlwaysTranslate();
                return;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
                mUserInteracted = true;
                // Fallthrough intentional.
            case ACTION_AUTO_NEVER_LANGUAGE:
                mOptions.toggleNeverTranslateLanguageState(
                        !mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
                TranslateCompactInfoBarJni.get().applyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this,
                        TranslateOption.NEVER_TRANSLATE,
                        mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
                return;
            case ACTION_OVERFLOW_NEVER_SITE:
                mUserInteracted = true;
                mOptions.toggleNeverTranslateDomainState(
                        !mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
                TranslateCompactInfoBarJni.get().applyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateCompactInfoBar.this,
                        TranslateOption.NEVER_TRANSLATE_SITE,
                        mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
                return;
            default:
                assert false : "Unsupported Menu Item Id, in handle post snackbar";
        }
    }

    private void toggleAlwaysTranslate() {
        mOptions.toggleAlwaysTranslateLanguageState(
                !mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        TranslateCompactInfoBarJni.get().applyBoolTranslateOption(mNativeTranslateInfoBarPtr,
                TranslateCompactInfoBar.this, TranslateOption.ALWAYS_TRANSLATE,
                mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
    }

    private static void recordInfobarAction(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                INFOBAR_HISTOGRAM, action, InfobarEvent.INFOBAR_HISTOGRAM_BOUNDARY);
    }

    private void recordInfobarLanguageData(String histogram, String langCode) {
        Integer hashCode = mOptions.getUMAHashCodeFromCode(langCode);
        if (hashCode != null) {
            RecordHistogram.recordSparseHistogram(histogram, hashCode);
        }
    }


    // Return the width of parent in pixels.  Return 0 if there is no parent.
    private int getParentWidth() {
        return mParent != null ? mParent.getWidth() : 0;
    }

    // Selects the tab corresponding to |actionType| to simulate the user pressing on this tab.
    void selectTabForTesting(int actionType) {
        if (actionType == ActionType.TRANSLATE) {
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        } else if (actionType == ActionType.TRANSLATE_SHOW_ORIGINAL) {
            mTabLayout.getTabAt(SOURCE_TAB_INDEX).select();
        } else {
            assert false;
        }
    }

    @NativeMethods
    interface Natives {
        void applyStringTranslateOption(long nativeTranslateCompactInfoBar,
                TranslateCompactInfoBar caller, int option, String value);
        void applyBoolTranslateOption(long nativeTranslateCompactInfoBar,
                TranslateCompactInfoBar caller, int option, boolean value);
        boolean shouldAutoNeverTranslate(long nativeTranslateCompactInfoBar,
                TranslateCompactInfoBar caller, boolean menuExpanded);
        boolean isIncognito(long nativeTranslateCompactInfoBar, TranslateCompactInfoBar caller);
    }
}
