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
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.ContextMenu;
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

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.BrowsingDataType;
import org.chromium.weblayer.ContextMenuParams;
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
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.SettingType;
import org.chromium.weblayer.SiteSettingsActivity;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.UrlBarOptions;
import org.chromium.weblayer.UserIdentityCallback;
import org.chromium.weblayer.WebLayer;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
    private static final String EXTRA_WEBVIEW_COMPAT = "EXTRA_WEBVIEW_COMPAT";

    private static class ContextMenuCreator
            implements View.OnCreateContextMenuListener, MenuItem.OnMenuItemClickListener {
        private static final int MENU_ID_COPY_LINK_URI = 1;
        private static final int MENU_ID_COPY_LINK_TEXT = 2;

        private ContextMenuParams mParams;
        private Context mContext;

        public ContextMenuCreator(ContextMenuParams params) {
            mParams = params;
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
            if (!TextUtils.isEmpty(mParams.titleOrAltText)) {
                TextView altTextView = new TextView(mContext);
                altTextView.setText(mParams.titleOrAltText);
                menu.setHeaderView(altTextView);
            }
            v.setOnCreateContextMenuListener(null);
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
                default:
                    break;
            }
            return true;
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
    private List<Tab> mPreviousTabList = new ArrayList<>();
    private Map<Tab, FaviconFetcher> mTabToFaviconFetcher = new HashMap<>();
    private Runnable mExitFullscreenRunnable;
    private boolean mIsTopViewVisible = true;
    private View mBottomView;
    private int mTopViewMinHeight;
    private boolean mTopViewPinnedToContentTop;
    private boolean mAnimateControlsChanges;
    private boolean mSetDarkMode;
    private boolean mInIncognitoMode;
    private boolean mEnableWebViewCompat;
    private boolean mEnableAltTopView;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSetDarkMode = AppCompatDelegate.getDefaultNightMode() == AppCompatDelegate.MODE_NIGHT_YES;
        mEnableWebViewCompat = getIntent().getBooleanExtra(EXTRA_WEBVIEW_COMPAT, false);
        if (mEnableWebViewCompat) {
            WebLayer.initializeWebViewCompatibilityMode(getApplicationContext());
        }
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

    private void onAppMenuButtonClicked(View appMenuButtonView) {
        PopupMenu popup = new PopupMenu(WebLayerShellActivity.this, appMenuButtonView);
        popup.getMenuInflater().inflate(R.menu.app_menu, popup.getMenu());
        popup.getMenu()
                .findItem(R.id.translate_menu_id)
                .setVisible(mBrowser.getActiveTab().canTranslate());
        popup.getMenu().findItem(R.id.webview_compat_menu_id).setVisible(!mEnableWebViewCompat);
        popup.getMenu().findItem(R.id.no_webview_compat_menu_id).setVisible(mEnableWebViewCompat);
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
            }

            if (item.getItemId() == R.id.webview_compat_menu_id) {
                restartShell(true);
            }

            if (item.getItemId() == R.id.no_webview_compat_menu_id) {
                restartShell(false);
            }

            if (item.getItemId() == R.id.set_translate_target_lang_menu_id) {
                mBrowser.getActiveTab().setTranslateTargetLanguage("de");
            }

            if (item.getItemId() == R.id.clear_translate_target_lang_menu_id) {
                mBrowser.getActiveTab().setTranslateTargetLanguage("");
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
        mUrlViewContainer.reset();
        if (mTabListCallback != null) {
            mBrowser.unregisterTabListCallback(mTabListCallback);
            mTabListCallback = null;
        }
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
        mProfile = mBrowser.getProfile();
        mProfile.setBooleanSetting(SettingType.UKM_ENABLED, true);
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
                    avatarLoadedCallback.onReceiveValue(BitmapFactory.decodeResource(
                            getApplicationContext().getResources(), R.drawable.avatar_sunglasses));
                }, 3000);
            }
        });

        setTabCallbacks(mBrowser.getActiveTab(), fragment);

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
        };
        mBrowser.registerTabListCallback(mTabListCallback);
        View nonEditUrlView = mBrowser.getUrlBarController().createUrlBarView(
                UrlBarOptions.builder()
                        .setTextSizeSP(DEFAULT_TEXT_SIZE)
                        .setTextColor(android.R.color.black)
                        .setIconColor(android.R.color.black)
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

    private void setTabCallbacks(Tab tab, Fragment fragment) {
        tab.setNewTabCallback(new NewTabCallback() {
            @Override
            public void onNewTab(Tab newTab, @NewTabType int type) {
                setTabCallbacks(newTab, fragment);
                mPreviousTabList.add(mBrowser.getActiveTab());
                mBrowser.setActiveTab(newTab);
            }
        });
        tab.setFullscreenCallback(new FullscreenCallback() {
            private int mSystemVisibilityToRestore;

            @Override
            public void onEnterFullscreen(Runnable exitFullscreenRunnable) {
                mExitFullscreenRunnable = exitFullscreenRunnable;
                // This comes from Chrome code to avoid an extra resize.
                final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                attrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                getWindow().setAttributes(attrs);

                View decorView = getWindow().getDecorView();
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
                mExitFullscreenRunnable = null;
                View decorView = getWindow().getDecorView();
                decorView.setSystemUiVisibility(mSystemVisibilityToRestore);

                final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                if ((attrs.flags & WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS) != 0) {
                    attrs.flags &= ~WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                    getWindow().setAttributes(attrs);
                }
            }
        });
        tab.registerTabCallback(new TabCallback() {
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
                webLayerView.setOnCreateContextMenuListener(new ContextMenuCreator(params));
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
        });
        tab.getNavigationController().registerNavigationCallback(new NavigationCallback() {
            @Override
            public void onLoadStateChanged(boolean isLoading, boolean toDifferentDocument) {
                mLoadProgressBar.setVisibility(
                        isLoading && toDifferentDocument ? View.VISIBLE : View.INVISIBLE);
            }

            @Override
            public void onLoadProgressChanged(double progress) {
                mLoadProgressBar.setProgress((int) Math.round(100 * progress));
            }
        });
        tab.setErrorPageCallback(new ErrorPageCallback() {
            @Override
            public boolean onBackToSafety() {
                fragment.getActivity().onBackPressed();
                return true;
            }
        });
        mTabToFaviconFetcher.put(tab, tab.createFaviconFetcher(new FaviconCallback() {
            @Override
            public void onFaviconChanged(Bitmap favicon) {
                updateFavicon(tab);
            }
        }));
    }

    private void closeTab(Tab tab) {
        mPreviousTabList.remove(tab);
        if (mBrowser.getActiveTab() == null && !mPreviousTabList.isEmpty()) {
            mBrowser.setActiveTab(mPreviousTabList.remove(mPreviousTabList.size() - 1));
        }
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

        if (CommandLine.isInitialized()
                && CommandLine.getInstance().hasSwitch("start-in-incognito")) {
            mInIncognitoMode = true;
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
    private void restartShell(boolean enableWebViewCompat) {
        finish();

        Intent intent = new Intent();
        intent.setClassName(getPackageName(), getClass().getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_WEBVIEW_COMPAT, enableWebViewCompat);
        startActivity(intent);
        System.exit(0);
    }

    private void updateFavicon(@NonNull Tab tab) {
        if (tab == mBrowser.getActiveTab()) {
            assert mTabToFaviconFetcher.containsKey(tab);
            ((ImageView) findViewById(R.id.favicon_image_view))
                    .setImageBitmap(mTabToFaviconFetcher.get(tab).getFaviconForCurrentNavigation());
        }
    }
}
