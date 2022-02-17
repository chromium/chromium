// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import static android.util.Patterns.WEB_URL;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.Display;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.webkit.ValueCallback;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ViewSwitcher;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.IntentUtils;
import org.chromium.base.compat.ApiHelperForR;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.BrowsingDataType;
import org.chromium.weblayer.ContextMenuParams;
import org.chromium.weblayer.DarkModeStrategy;
import org.chromium.weblayer.ErrorPageCallback;
import org.chromium.weblayer.FaviconCallback;
import org.chromium.weblayer.FaviconFetcher;
import org.chromium.weblayer.FindInPageCallback;
import org.chromium.weblayer.FullscreenCallback;
import org.chromium.weblayer.NavigateParams;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.NewTabType;
import org.chromium.weblayer.OpenUrlCallback;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.SiteSettingsActivity;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.UrlBarOptions;
import org.chromium.weblayer.UserIdentityCallback;
import org.chromium.weblayer.WebLayer;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Activity for managing the Demo Shell.
 */
// This isn't part of Chrome, so using explicit colors/sizes is ok.
@SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
public class WebLayerShellActivity extends AppCompatActivity {
    public static void setDarkMode(boolean enabled) {
        AppCompatDelegate.setDefaultNightMode(
                enabled ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);
    }

    private static final String NON_INCOGNITO_PROFILE_NAME = "DefaultProfile";
    private static final String EXTRA_START_IN_INCOGNITO = "EXTRA_START_IN_INCOGNITO";
    private static final String KEY_PREVIOUS_TAB_GUIDS = "previousTabGuids";

    private static class ContextMenuCreator
            implements View.OnCreateContextMenuListener, MenuItem.OnMenuItemClickListener {
        private static final int MENU_ID_COPY_LINK_URI = 1;
        private static final int MENU_ID_COPY_LINK_TEXT = 2;
        private static final int MENU_ID_DOWNLOAD_IMAGE = 3;
        private static final int MENU_ID_DOWNLOAD_VIDEO = 4;
        private static final int MENU_ID_DOWNLOAD_LINK = 5;

        private ContextMenuParams mParams;
        private Tab mTab;
        private Context mContext;

        public ContextMenuCreator(ContextMenuParams params, Tab tab) {
            mParams = params;
            mTab = tab;
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            mContext = v.getContext();
            menu.add(mParams.pageUri.toString());
            if (mParams.linkUri != null) {
                MenuItem copyLinkUriItem =
                        menu.add(Menu.NONE, MENU_ID_COPY_LINK_URI, Menu.NONE, "Copy link address");
                copyLinkUriItem.setOnMenuItemClickListener(this);
            }
            if (!TextUtils.isEmpty(mParams.linkText)) {
                MenuItem copyLinkTextItem =
                        menu.add(Menu.NONE, MENU_ID_COPY_LINK_TEXT, Menu.NONE, "Copy link text");
                copyLinkTextItem.setOnMenuItemClickListener(this);
            }
            if (mParams.canDownload) {
                if (mParams.isImage) {
                    MenuItem downloadImageItem = menu.add(
                            Menu.NONE, MENU_ID_DOWNLOAD_IMAGE, Menu.NONE, "Download image");
                    downloadImageItem.setOnMenuItemClickListener(this);
                } else if (mParams.isVideo) {
                    MenuItem downloadVideoItem = menu.add(
                            Menu.NONE, MENU_ID_DOWNLOAD_VIDEO, Menu.NONE, "Download video");
                    downloadVideoItem.setOnMenuItemClickListener(this);
                } else if (mParams.linkUri != null) {
                    MenuItem downloadVideoItem =
                            menu.add(Menu.NONE, MENU_ID_DOWNLOAD_LINK, Menu.NONE, "Download link");
                    downloadVideoItem.setOnMenuItemClickListener(this);
                }
            }
            if (!TextUtils.isEmpty(mParams.titleOrAltText)) {
                TextView altTextView = new TextView(mContext);
                altTextView.setText(mParams.titleOrAltText);
                menu.setHeaderView(altTextView);
            }
            v.setOnCreateContextMenuListener(null);

            // Clear the menu if we didn't add any actions. This will prevent it from showing up.
            if (menu.size() == 1) menu.clear();
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            ClipboardManager clipboard =
                    (ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            switch (item.getItemId()) {
                case MENU_ID_COPY_LINK_URI:
                    clipboard.setPrimaryClip(
                            ClipData.newPlainText("link address", mParams.linkUri.toString()));
                    break;
                case MENU_ID_COPY_LINK_TEXT:
                    clipboard.setPrimaryClip(ClipData.newPlainText("link text", mParams.linkText));
                    break;
                case MENU_ID_DOWNLOAD_IMAGE:
                case MENU_ID_DOWNLOAD_VIDEO:
                case MENU_ID_DOWNLOAD_LINK:
                    mTab.download(mParams);
                    break;
                default:
                    break;
            }
            return true;
        }
    }

    // Used for any state that is per Tab.
    private static final class PerTabState {
        public final FaviconFetcher mFaviconFetcher;
        public final TabCallback mTabCallback;

        PerTabState(FaviconFetcher faviconFetcher, TabCallback tabCallback) {
            mFaviconFetcher = faviconFetcher;
            mTabCallback = tabCallback;
        }
    }

    private static final class FullscreenCallbackImpl extends FullscreenCallback {
        private WebLayerShellActivity mActivity;
        private int mSystemVisibilityToRestore;

        public FullscreenCallbackImpl(WebLayerShellActivity activity) {
            mActivity = activity;
        }

        public void setActivity(WebLayerShellActivity activity) {
            mActivity = activity;
        }

        @Override
        public void onEnterFullscreen(Runnable exitFullscreenRunnable) {
            if (mActivity == null) return;
            // This comes from Chrome code to avoid an extra resize.
            final WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
            attrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
            mActivity.getWindow().setAttributes(attrs);

            // Hide the controls bar as we need to give WebLayer all available screen space.
            mActivity.findViewById(R.id.controls).setVisibility(View.GONE);

            View decorView = mActivity.getWindow().getDecorView();
            // Caching the system ui visibility is ok for shell, but likely not ok for
            // real code.
            mSystemVisibilityToRestore = decorView.getSystemUiVisibility();
            decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                    | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                    | View.SYSTEM_UI_FLAG_LOW_PROFILE | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }

        @Override
        public void onExitFullscreen() {
            if (mActivity == null) return;
            View decorView = mActivity.getWindow().getDecorView();
            decorView.setSystemUiVisibility(mSystemVisibilityToRestore);

            mActivity.findViewById(R.id.controls).setVisibility(View.VISIBLE);

            final WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
            if ((attrs.flags & WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS) != 0) {
                attrs.flags &= ~WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                mActivity.getWindow().setAttributes(attrs);
            }
        }
    }

    private static final String TAG = "WebLayerShell";
    private static final float DEFAULT_TEXT_SIZE = 15.0F;
    private static final int EDITABLE_URL_TEXT_VIEW = 0;
    private static final int NONEDITABLE_URL_TEXT_VIEW = 1;

    private Profile mProfile;
    private Browser mBrowser;
    private ImageButton mAppMenuButton;
    private ViewSwitcher mUrlViewContainer;
    private EditText mEditUrlView;
    private ProgressBar mLoadProgressBar;
    private View mTopContentsContainer;
    private View mAltTopContentsContainer;
    private TabListCallback mTabListCallback;
    private NewTabCallback mNewTabCallback;
    private FullscreenCallbackImpl mFullscreenCallback;
    private NavigationCallback mNavigationCallback;
    private ErrorPageCallback mErrorPageCallback;
    private List<Tab> mPreviousTabList = new ArrayList<>();
    private Map<Tab, PerTabState> mTabToPerTabState = new HashMap<>();
    private boolean mIsTopViewVisible = true;
    private View mBottomView;
    private int mTopViewMinHeight;
    private boolean mTopViewPinnedToContentTop;
    private boolean mAnimateControlsChanges;
    private boolean mSetDarkMode;
    private boolean mInIncognitoMode;
    private boolean mEnableAltTopView;
    private int mDarkModeStrategy = R.id.dark_mode_web_theme;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSetDarkMode = AppCompatDelegate.getDefaultNightMode() == AppCompatDelegate.MODE_NIGHT_YES;
        mInIncognitoMode = getIntent().getBooleanExtra(EXTRA_START_IN_INCOGNITO, false);

        setContentView(R.layout.main);
        TextView versionText = (TextView) findViewById(R.id.version_text);
        versionText.setText(getString(
                R.string.version, WebLayer.getVersion(), WebLayer.getSupportedFullVersion(this)));
        ImageButton controlsMenuButton = (ImageButton) findViewById(R.id.controls_menu_button);
        controlsMenuButton.setOnClickListener(this::onControlsMenuButtonClicked);

        mAltTopContentsContainer =
                LayoutInflater.from(this).inflate(R.layout.alt_shell_browser_controls, null);
        mTopContentsContainer =
                LayoutInflater.from(this).inflate(R.layout.shell_browser_controls, null);
        mUrlViewContainer = mTopContentsContainer.findViewById(R.id.url_view_container);

        mEditUrlView = mUrlViewContainer.findViewById(R.id.editable_url_view);
        mEditUrlView.setOnEditorActionListener((TextView v, int actionId, KeyEvent event) -> {
            loadUrl(mEditUrlView.getText().toString());
            mEditUrlView.clearFocus();
            return true;
        });
        mUrlViewContainer.setDisplayedChild(EDITABLE_URL_TEXT_VIEW);

        mAppMenuButton = mTopContentsContainer.findViewById(R.id.app_menu_button);
        mAppMenuButton.setOnClickListener(this::onAppMenuButtonClicked);

        mLoadProgressBar = mTopContentsContainer.findViewById(R.id.progress_bar);

        try {
            // This ensures asynchronous initialization of WebLayer on first start of activity.
            // If activity is re-created during process restart, FragmentManager attaches
            // BrowserFragment immediately, resulting in synchronous init. By the time this line
            // executes, the synchronous init has already happened.
            WebLayer.loadAsync(
                    getApplication(), webLayer -> onWebLayerReady(webLayer, savedInstanceState));
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        // Store the stack of previous tab GUIDs that are used to set the next active tab when a tab
        // closes. Also used to setup various callbacks again on restore.
        String[] previousTabGuids = new String[mPreviousTabList.size()];
        for (int i = 0; i < mPreviousTabList.size(); ++i) {
            previousTabGuids[i] = mPreviousTabList.get(i).getGuid();
        }
        outState.putStringArray(KEY_PREVIOUS_TAB_GUIDS, previousTabGuids);
    }

    private void onAppMenuButtonClicked(View appMenuButtonView) {
        PopupMenu popup = new PopupMenu(WebLayerShellActivity.this, appMenuButtonView);
        popup.getMenuInflater().inflate(R.menu.app_menu, popup.getMenu());
        popup.getMenu()
                .findItem(R.id.translate_menu_id)
                .setVisible(mBrowser.getActiveTab().canTranslate());
        popup.getMenu().findItem(R.id.enable_incognito_menu_id).setVisible(!mInIncognitoMode);
        popup.getMenu().findItem(R.id.disable_incognito_menu_id).setVisible(mInIncognitoMode);
        boolean isDesktopUserAgent = mBrowser.getActiveTab().isDesktopUserAgentEnabled();
        popup.getMenu().findItem(R.id.desktop_site_menu_id).setVisible(!isDesktopUserAgent);
        popup.getMenu().findItem(R.id.no_desktop_site_menu_id).setVisible(isDesktopUserAgent);
        popup.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == R.id.reload_menu_id) {
                mBrowser.getActiveTab().getNavigationController().reload();
                return true;
            }

            if (item.getItemId() == R.id.find_begin_menu_id) {
                // TODO(estade): add a UI for FIP. For now, just search for "cat", or go
                // to the next result if a search has already been initiated.
                mBrowser.getActiveTab().getFindInPageController().setFindInPageCallback(
                        new FindInPageCallback() {});
                mBrowser.getActiveTab().getFindInPageController().find("cat", true);
                return true;
            }

            if (item.getItemId() == R.id.find_end_menu_id) {
                mBrowser.getActiveTab().getFindInPageController().setFindInPageCallback(null);
                return true;
            }

            if (item.getItemId() == R.id.site_settings_menu_id) {
                // TODO(crbug.com/1083233): Figure out the right long-term behavior here.
                if (mInIncognitoMode) return true;

                Intent intent = SiteSettingsActivity.createIntentForCategoryList(
                        this, NON_INCOGNITO_PROFILE_NAME);
                IntentUtils.safeStartActivity(this, intent);
                return true;
            }

            if (item.getItemId() == R.id.accessibility_settings_menu_id) {
                Intent intent = SiteSettingsActivity.createIntentForAccessibilitySettings(
                        this, mProfile.getName(), mProfile.isIncognito());
                IntentUtils.safeStartActivity(this, intent);
                return true;
            }

            if (item.getItemId() == R.id.translate_menu_id) {
                mBrowser.getActiveTab().showTranslateUi();
                return true;
            }

            if (item.getItemId() == R.id.clear_browsing_data_menu_id) {
                mProfile.clearBrowsingData(
                        new int[] {BrowsingDataType.COOKIES_AND_SITE_DATA, BrowsingDataType.CACHE,
                                BrowsingDataType.SITE_SETTINGS},

                        () -> {
                            Toast.makeText(getApplicationContext(), "Data cleared!",
                                         Toast.LENGTH_SHORT)
                                    .show();
                        });
                return true;
            }

            if (item.getItemId() == R.id.enable_incognito_menu_id) {
                restartShell(true);
                return true;
            }

            if (item.getItemId() == R.id.disable_incognito_menu_id) {
                restartShell(false);
                return true;
            }

            if (item.getItemId() == R.id.desktop_site_menu_id) {
                mBrowser.getActiveTab().setDesktopUserAgentEnabled(true);
                return true;
            }

            if (item.getItemId() == R.id.no_desktop_site_menu_id) {
                mBrowser.getActiveTab().setDesktopUserAgentEnabled(false);
                return true;
            }

            if (item.getItemId() == R.id.set_translate_target_lang_menu_id) {
                mBrowser.getActiveTab().setTranslateTargetLanguage("de");
                return true;
            }

            if (item.getItemId() == R.id.clear_translate_target_lang_menu_id) {
                mBrowser.getActiveTab().setTranslateTargetLanguage("");
                return true;
            }

            if (item.getItemId() == R.id.add_to_homescreen) {
                try {
                    // Since the API is still experimental, it's private.
                    Method addToHomescreen = Tab.class.getDeclaredMethod("addToHomescreen");
                    addToHomescreen.setAccessible(true);
                    addToHomescreen.invoke(mBrowser.getActiveTab());
                } catch (Exception e) {
                    throw new RuntimeException("Failed to start addToHomescreen", e);
                }
                return true;
            }

            return false;
        });
        popup.show();
    }

    private void onControlsMenuButtonClicked(View controlsMenuButtonView) {
        PopupMenu popup = new PopupMenu(WebLayerShellActivity.this, controlsMenuButtonView);
        popup.getMenuInflater().inflate(R.menu.controls_menu, popup.getMenu());
        popup.getMenu().findItem(R.id.toggle_top_view_id).setChecked(mIsTopViewVisible);
        popup.getMenu().findItem(R.id.toggle_bottom_view_id).setChecked(mBottomView != null);
        popup.getMenu()
                .findItem(R.id.toggle_top_view_min_height_id)
                .setChecked(mTopViewMinHeight > 0);
        popup.getMenu()
                .findItem(R.id.toggle_top_view_pinned_to_top_id)
                .setChecked(mTopViewPinnedToContentTop);
        popup.getMenu().findItem(R.id.toggle_alt_top_view_id).setChecked(mEnableAltTopView);
        popup.getMenu()
                .findItem(R.id.toggle_controls_animations_id)
                .setChecked(mAnimateControlsChanges);
        popup.getMenu().findItem(R.id.toggle_dark_mode).setChecked(mSetDarkMode);
        popup.getMenu().findItem(mDarkModeStrategy).setChecked(true);

        popup.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == R.id.toggle_top_view_id) {
                mIsTopViewVisible = !mIsTopViewVisible;
                updateTopView();
                return true;
            }

            if (item.getItemId() == R.id.toggle_bottom_view_id) {
                if (mBottomView == null) {
                    mBottomView = LayoutInflater.from(this).inflate(R.layout.bottom_controls, null);
                } else {
                    mBottomView = null;
                }
                mBrowser.setBottomView(mBottomView);
                return true;
            }

            if (item.getItemId() == R.id.toggle_top_view_min_height_id) {
                mTopViewMinHeight = (mTopViewMinHeight == 0) ? 50 : 0;
                updateTopView();
                return true;
            }

            if (item.getItemId() == R.id.toggle_top_view_pinned_to_top_id) {
                mTopViewPinnedToContentTop = !mTopViewPinnedToContentTop;
                updateTopView();
                return true;
            }

            if (item.getItemId() == R.id.toggle_alt_top_view_id) {
                mEnableAltTopView = !mEnableAltTopView;
                updateTopView();
                return true;
            }

            if (item.getItemId() == R.id.toggle_controls_animations_id) {
                mAnimateControlsChanges = !mAnimateControlsChanges;
                updateTopView();
                return true;
            }

            if (item.getItemId() == R.id.toggle_dark_mode) {
                mSetDarkMode = !mSetDarkMode;
                setDarkMode(mSetDarkMode);
                return true;
            }

            if (item.getItemId() == R.id.dark_mode_web_theme) {
                mDarkModeStrategy = R.id.dark_mode_web_theme;
                mBrowser.setDarkModeStrategy(DarkModeStrategy.WEB_THEME_DARKENING_ONLY);
                return true;
            }

            if (item.getItemId() == R.id.dark_mode_user_agent) {
                mDarkModeStrategy = R.id.dark_mode_user_agent;
                mBrowser.setDarkModeStrategy(DarkModeStrategy.USER_AGENT_DARKENING_ONLY);
                return true;
            }

            if (item.getItemId() == R.id.dark_mode_prefer_web_theme) {
                mDarkModeStrategy = R.id.dark_mode_prefer_web_theme;
                mBrowser.setDarkModeStrategy(
                        DarkModeStrategy.PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING);
                return true;
            }

            return false;
        });
        popup.show();
    }

    private void updateTopView() {
        View topView = null;
        if (mIsTopViewVisible) {
            topView = mEnableAltTopView ? mAltTopContentsContainer : mTopContentsContainer;
        }
        mBrowser.setTopView(
                topView, mTopViewMinHeight, mTopViewPinnedToContentTop, mAnimateControlsChanges);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // NOTE: It's important to unregister any state installed on browser/tab as during a
        // configuration change (such as rotation) the activity is destroyed but the
        // browser/fragment/tabs are not.
        mUrlViewContainer.reset();
        if (mTabListCallback != null) {
            unregisterBrowserAndTabCallbacks();
        }
        if (mFullscreenCallback != null) {
            mFullscreenCallback.setActivity(null);
        }
    }

    private void unregisterBrowserAndTabCallbacks() {
        assert mBrowser != null;
        mBrowser.unregisterTabListCallback(mTabListCallback);
        mTabListCallback = null;

        Set<Tab> tabs = new HashSet(mTabToPerTabState.keySet());
        for (Tab tab : tabs) unregisterTabCallbacks(tab);
        mTabToPerTabState.clear();
    }

    private void onWebLayerReady(WebLayer webLayer, Bundle savedInstanceState) {
        if (isFinishing() || isDestroyed()) return;

        webLayer.setRemoteDebuggingEnabled(true);

        Fragment fragment = getOrCreateBrowserFragment(savedInstanceState);

        // Have WebLayer Shell retain the fragment instance to simulate the behavior of
        // external embedders (note that if this is changed, then WebLayer Shell should handle
        // rotations and resizes itself via its manifest, as otherwise the user loses all state
        // when the shell is rotated in the foreground).
        fragment.setRetainInstance(true);
        mBrowser = Browser.fromFragment(fragment);
        Point point = getDisplaySize();
        mBrowser.setMinimumSurfaceSize(point.x, point.y);
        mProfile = mBrowser.getProfile();
        mProfile.setUserIdentityCallback(new UserIdentityCallback() {
            @Override
            public String getEmail() {
                return "user@example.com";
            }

            @Override
            public String getFullName() {
                return "Jill Doe";
            }

            @Override
            public void getAvatar(int desiredSize, ValueCallback<Bitmap> avatarLoadedCallback) {
                // Simulate a delayed load.
                final Handler handler = new Handler(Looper.getMainLooper());
                handler.postDelayed(() -> {
                    Bitmap bitmap = BitmapFactory.decodeResource(
                            getApplicationContext().getResources(), R.drawable.avatar_sunglasses);
                    // Curveball: set an arbitrary density.
                    bitmap.setDensity(120);
                    avatarLoadedCallback.onReceiveValue(bitmap);
                }, 3000);
            }
        });
        mProfile.setTablessOpenUrlCallback(new OpenUrlCallback() {
            @Override
            public Browser getBrowserForNewTab() {
                return mBrowser;
            }

            @Override
            public void onTabAdded(Tab tab) {
                onTabAddedImpl(tab);
            }
        });

        createTabCallbacks();

        restorePreviousTabList(savedInstanceState);

        registerTabCallbacks(mBrowser.getActiveTab());

        updateTopView();

        mTabListCallback = new TabListCallback() {
            @Override
            public void onActiveTabChanged(Tab activeTab) {
                mUrlViewContainer.setDisplayedChild(NONEDITABLE_URL_TEXT_VIEW);

                // This callback is fired with null as the param on removal of the active tab.
                if (activeTab == null) return;

                updateFavicon(activeTab);
            }
            @Override
            public void onTabRemoved(Tab tab) {
                closeTab(tab);
            }
            @Override
            public void onWillDestroyBrowserAndAllTabs() {
                unregisterBrowserAndTabCallbacks();
            }
        };
        mBrowser.registerTabListCallback(mTabListCallback);
        View nonEditUrlView = mBrowser.getUrlBarController().createUrlBarView(
                UrlBarOptions.builder()
                        .setTextSizeSP(DEFAULT_TEXT_SIZE)
                        .setTextColor(android.R.color.black)
                        .setIconColor(android.R.color.black)
                        .showPublisherUrl()
                        .setTextClickListener(v -> {
                            mEditUrlView.setText("");
                            mUrlViewContainer.setDisplayedChild(EDITABLE_URL_TEXT_VIEW);
                            mEditUrlView.requestFocus();
                        })
                        .setTextLongClickListener(v -> {
                            ClipboardManager clipboard =
                                    (ClipboardManager) v.getContext().getSystemService(
                                            Context.CLIPBOARD_SERVICE);
                            clipboard.setPrimaryClip(
                                    ClipData.newPlainText("link address", getCurrentDisplayUrl()));
                            return true;
                        })
                        .build());
        nonEditUrlView.setOnClickListener(v -> {
            mEditUrlView.setText("");
            mUrlViewContainer.setDisplayedChild(EDITABLE_URL_TEXT_VIEW);
            mEditUrlView.requestFocus();
        });
        RelativeLayout nonEditUrlViewContainer =
                mTopContentsContainer.findViewById(R.id.noneditable_url_view_container);
        nonEditUrlViewContainer.addView(nonEditUrlView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mUrlViewContainer.setDisplayedChild(NONEDITABLE_URL_TEXT_VIEW);

        if (getCurrentDisplayUrl() != null) {
            return;
        }
        loadUrl(getUrlFromIntent(getIntent()));
    }

    /* Returns the Url for the current tab as a String, or null if there is no
     * current tab. */
    private String getCurrentDisplayUrl() {
        NavigationController navigationController =
                mBrowser.getActiveTab().getNavigationController();

        if (navigationController.getNavigationListSize() == 0) {
            return null;
        }

        return navigationController
                .getNavigationEntryDisplayUri(navigationController.getNavigationListCurrentIndex())
                .toString();
    }

    private void createTabCallbacks() {
        mNewTabCallback = new NewTabCallback() {
            @Override
            public void onNewTab(Tab newTab, @NewTabType int type) {
                onTabAddedImpl(newTab);
            }
        };

        mNavigationCallback = new NavigationCallback() {
            @Override
            public void onLoadStateChanged(boolean isLoading, boolean shouldShowLoadingUi) {
                mLoadProgressBar.setVisibility(
                        isLoading && shouldShowLoadingUi ? View.VISIBLE : View.INVISIBLE);
            }

            @Override
            public void onLoadProgressChanged(double progress) {
                mLoadProgressBar.setProgress((int) Math.round(100 * progress));
            }
        };

        mErrorPageCallback = new ErrorPageCallback() {
            @Override
            public boolean onBackToSafety() {
                WebLayerShellActivity.this.onBackPressed();
                return true;
            }
        };
    }

    private void restorePreviousTabList(Bundle savedInstanceState) {
        if (savedInstanceState == null) return;
        String[] previousTabGuids = savedInstanceState.getStringArray(KEY_PREVIOUS_TAB_GUIDS);
        if (previousTabGuids == null) return;

        Map<String, Tab> currentTabMap = new HashMap<String, Tab>();
        for (Tab tab : mBrowser.getTabs()) {
            currentTabMap.put(tab.getGuid(), tab);
        }

        for (String tabGuid : previousTabGuids) {
            Tab tab = currentTabMap.get(tabGuid);
            if (tab == null) continue;
            mPreviousTabList.add(tab);
            registerTabCallbacks(tab);
        }
    }

    private void onTabAddedImpl(Tab newTab) {
        registerTabCallbacks(newTab);
        mPreviousTabList.add(mBrowser.getActiveTab());
        mBrowser.setActiveTab(newTab);
    }

    private void registerTabCallbacks(Tab tab) {
        tab.setNewTabCallback(mNewTabCallback);

        if (mFullscreenCallback != null) {
            tab.setFullscreenCallback(mFullscreenCallback);
        } else if (tab.getFullscreenCallback() != null) {
            mFullscreenCallback = (FullscreenCallbackImpl) tab.getFullscreenCallback();
            mFullscreenCallback.setActivity(this);
        } else {
            mFullscreenCallback = new FullscreenCallbackImpl(this);
            tab.setFullscreenCallback(mFullscreenCallback);
        }

        tab.getNavigationController().registerNavigationCallback(mNavigationCallback);
        tab.setErrorPageCallback(mErrorPageCallback);
        TabCallback tabCallback = new TabCallback() {
            @Override
            public void onVisibleUriChanged(Uri uri) {
                mUrlViewContainer.setDisplayedChild(NONEDITABLE_URL_TEXT_VIEW);
            }

            @Override
            public void onTabModalStateChanged(boolean isTabModalShowing) {
                mAppMenuButton.setEnabled(!isTabModalShowing);
            }

            @Override
            public void showContextMenu(ContextMenuParams params) {
                View webLayerView = getSupportFragmentManager().getFragments().get(0).getView();
                webLayerView.setOnCreateContextMenuListener(new ContextMenuCreator(params, tab));
                webLayerView.showContextMenu();
            }

            @Override
            public void bringTabToFront() {
                tab.getBrowser().setActiveTab(tab);

                Activity activity = WebLayerShellActivity.this;
                Intent intent = new Intent(activity, WebLayerShellActivity.class);
                intent.setAction(Intent.ACTION_MAIN);
                activity.startActivity(intent);
            }
        };
        tab.registerTabCallback(tabCallback);
        FaviconFetcher faviconFetcher = tab.createFaviconFetcher(new FaviconCallback() {
            @Override
            public void onFaviconChanged(Bitmap favicon) {
                updateFavicon(tab);
            }
        });
        mTabToPerTabState.put(tab, new PerTabState(faviconFetcher, tabCallback));
    }

    private void unregisterTabCallbacks(Tab tab) {
        // Do not unset FullscreenCallback here which is called from onDestroy, since
        // unsetting FullscreenCallback also exits fullscreen.
        tab.setNewTabCallback(null);
        tab.getNavigationController().unregisterNavigationCallback(mNavigationCallback);
        tab.setErrorPageCallback(null);
        PerTabState perTabState = mTabToPerTabState.get(tab);
        assert perTabState != null;
        tab.unregisterTabCallback(perTabState.mTabCallback);
        perTabState.mFaviconFetcher.destroy();
        mTabToPerTabState.remove(tab);
    }

    private void closeTab(Tab tab) {
        mPreviousTabList.remove(tab);
        if (mBrowser.getActiveTab() == null && !mPreviousTabList.isEmpty()) {
            mBrowser.setActiveTab(mPreviousTabList.remove(mPreviousTabList.size() - 1));
        }
        unregisterTabCallbacks(tab);
    }

    private Fragment getOrCreateBrowserFragment(Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (savedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return fragments.get(0);
            }
        }

        String profileName = mInIncognitoMode ? null : NON_INCOGNITO_PROFILE_NAME;

        Fragment fragment = WebLayer.createBrowserFragment(profileName);
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(R.id.weblayer, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();
        return fragment;
    }

    public void loadUrl(String input) {
        // Disable intent processing for urls typed in. This way the user can navigate to urls that
        // match apps (this is similar to what Chrome does).
        NavigateParams.Builder navigateParamsBuilder =
                new NavigateParams.Builder().disableIntentProcessing();

        mBrowser.getActiveTab().getNavigationController().navigate(
                getUriFromInput(input), navigateParamsBuilder.build());
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    /**
     * Given input which may be empty, null, a URL, or search terms, this forms a URI suitable for
     * loading in a tab.
     * @param input The text.
     * @return A valid URL.
     */
    public static Uri getUriFromInput(String input) {
        if (TextUtils.isEmpty(input)) {
            return Uri.parse("https://google.com");
        }

        // WEB_URL doesn't match port numbers. Special case "localhost:" to aid
        // testing where a port is remapped.
        // Use WEB_URL first to ensure this matches urls such as 'https.'
        if (WEB_URL.matcher(input).matches() || input.startsWith("http://localhost:")) {
            // WEB_URL matches relative urls (relative meaning no scheme), but this branch is only
            // interested in absolute urls. Fall through if no scheme is supplied.
            Uri uri = Uri.parse(input);
            if (!uri.isRelative()) return uri;
        }

        if (input.startsWith("www.") || input.indexOf(":") == -1) {
            String url = "http://" + input;
            if (WEB_URL.matcher(url).matches()) {
                return Uri.parse(url);
            }
        }

        if (input.startsWith("chrome://")) return Uri.parse(input);

        return Uri.parse("https://google.com/search")
                .buildUpon()
                .appendQueryParameter("q", input)
                .build();
    }

    @Override
    public void onBackPressed() {
        if (mBrowser != null) {
            Tab activeTab = mBrowser.getActiveTab();

            if (activeTab.dismissTransientUi()) return;

            NavigationController controller = activeTab.getNavigationController();
            if (controller.canGoBack()) {
                controller.goBack();
                return;
            }
            if (!mPreviousTabList.isEmpty()) {
                activeTab.dispatchBeforeUnloadAndClose();
                return;
            }
        }
        super.onBackPressed();
    }

    @SuppressWarnings("checkstyle:SystemExitCheck") // Allowed since this shouldn't be a crash.
    private void restartShell(boolean enableIncognito) {
        finish();

        Intent intent = new Intent();
        intent.setClassName(getPackageName(), getClass().getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_START_IN_INCOGNITO, enableIncognito);
        startActivity(intent);
        System.exit(0);
    }

    private void updateFavicon(@NonNull Tab tab) {
        if (tab == mBrowser.getActiveTab()) {
            assert mTabToPerTabState.containsKey(tab);
            ((ImageView) mTopContentsContainer.findViewById(R.id.favicon_image_view))
                    .setImageBitmap(mTabToPerTabState.get(tab)
                                            .mFaviconFetcher.getFaviconForCurrentNavigation());
        }
    }

    private Point getDisplaySize() {
        Point point = new Point();
        WindowManager windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Rect rect = ApiHelperForR.getMaximumWindowMetricsBounds(windowManager);
            point.set(rect.width(), rect.height());
        } else {
            Display display = windowManager.getDefaultDisplay();
            display.getRealSize(point);
        }
        return point;
    }
}
