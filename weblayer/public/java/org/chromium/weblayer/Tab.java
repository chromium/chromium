// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;
import android.util.Pair;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.IClientPage;
import org.chromium.weblayer_private.interfaces.IContextMenuParams;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IExternalIntentInIncognitoCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IGoogleAccountsCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.IWebMessageCallbackClient;
import org.chromium.weblayer_private.interfaces.IWebMessageReplyProxy;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Represents a single tab in a browser. More specifically, owns a NavigationController, and allows
 * configuring state of the tab, such as delegates and callbacks.
 */
class Tab {
    // Maps from id (as returned from ITab.getId()) to Tab.
    private static final Map<Integer, Tab> sTabMap = new HashMap<Integer, Tab>();

    private ITab mImpl;
    // Remember the stack of Tab destruction.
    private Throwable mDestroyStack;
    private final NavigationController mNavigationController;
    private final FindInPageController mFindInPageController;
    private final MediaCaptureController mMediaCaptureController;
    private final ObserverList<TabCallback> mCallbacks;
    private Browser mBrowser;
    private FullscreenCallbackClientImpl mFullscreenCallbackClient;
    private NewTabCallback mNewTabCallback;
    private final ObserverList<ScrollOffsetCallback> mScrollOffsetCallbacks;
    private @Nullable ActionModeCallback mActionModeCallback;

    private TabProxy mTabProxy;
    private TabNavigationControllerProxy mTabNavigationControllerProxy;

    // Id from the remote side.
    private final int mId;
    // Guid from the remote side.
    private final String mGuid;

    // Constructor for test mocking.
    protected Tab() {
        mImpl = null;
        mNavigationController = null;
        mFindInPageController = null;
        mMediaCaptureController = null;
        mCallbacks = null;
        mScrollOffsetCallbacks = null;
        mId = 0;
        mGuid = "";
        mTabProxy = null;
        mTabNavigationControllerProxy = null;
    }

    Tab(ITab impl, Browser browser) {
        mImpl = impl;
        mBrowser = browser;
        try {
            mId = impl.getId();
            mGuid = impl.getGuid();
            mImpl.setClient(new TabClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mCallbacks = new ObserverList<TabCallback>();
        mScrollOffsetCallbacks = new ObserverList<ScrollOffsetCallback>();
        mNavigationController = NavigationController.create(mImpl);
        mFindInPageController = new FindInPageController(mImpl);
        mMediaCaptureController = new MediaCaptureController(mImpl);

        mTabProxy = new TabProxy(this);
        mTabNavigationControllerProxy = new TabNavigationControllerProxy(mNavigationController);

        registerTab(this);
    }

    static void registerTab(Tab tab) {
        assert getTabById(tab.getId()) == null;
        sTabMap.put(tab.getId(), tab);
    }

    static void unregisterTab(Tab tab) {
        assert getTabById(tab.getId()) != null;
        sTabMap.remove(tab.getId());
    }

    static Tab getTabById(int id) {
        return sTabMap.get(id);
    }

    static Set<Tab> getTabsInBrowser(Browser browser) {
        Set<Tab> tabs = new HashSet<Tab>();
        for (Tab tab : sTabMap.values()) {
            if (tab.getBrowser() == browser) tabs.add(tab);
        }
        return tabs;
    }

    private void throwIfDestroyed() {
        if (mImpl == null) {
            throw new IllegalStateException("Tab can not be used once destroyed", mDestroyStack);
        }
    }

    int getId() {
        return mId;
    }

    void setBrowser(Browser browser) {
        mBrowser = browser;
    }

    /**
     * Returns true if this Tab has been destroyed.
     */
    public boolean isDestroyed() {
        ThreadCheck.ensureOnUiThread();
        return mImpl == null;
    }

    /**
     * Returns whether the tab will automatically reload after its renderer process is lost.
     *
     * This returns true if the tab is known not to be visible, specifically if the tab is not
     * active in its browser or its Fragment is not started. When a tab in this state loses its
     * renderer process to a crash (or due to system memory reclamation), it will automatically
     * reload next the time it becomes possibly visible.
     */
    public boolean willAutomaticallyReloadAfterCrash() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            return mImpl.willAutomaticallyReloadAfterCrash();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @NonNull
    public Browser getBrowser() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return mBrowser;
    }

    public void setErrorPageCallback(@Nullable ErrorPageCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.setErrorPageCallbackClient(
                    callback == null ? null : new ErrorPageCallbackClientImpl(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setFullscreenCallback(@Nullable FullscreenCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            if (callback != null) {
                mFullscreenCallbackClient = new FullscreenCallbackClientImpl(callback);
                mImpl.setFullscreenCallbackClient(mFullscreenCallbackClient);
            } else {
                mImpl.setFullscreenCallbackClient(null);
                mFullscreenCallbackClient = null;
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Creates a {@link FaviconFetcher} that notifies a {@link FaviconCallback} when the favicon
     * changes.
     *
     * When the fetcher is no longer necessary, call {@link destroy}. Destroying the Tab implicitly
     * destroys any fetchers that were created.
     *
     * A page may provide any number of favicons. This favors a largish favicon. If a previously
     * cached icon is available, it is used, otherwise the icon is downloaded.
     *
     * {@link callback} may be called multiple times for the same navigation. This happens if the
     * page dynamically updates the favicon.
     *
     * @param callback The callback to notify of changes.
     */
    public @NonNull FaviconFetcher createFaviconFetcher(@NonNull FaviconCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return new FaviconFetcher(mImpl, callback);
    }

    /**
     * Sets the target language for translation such that whenever the translate UI shows in this
     * Tab, the target language will be |targetLanguage|. Notes:
     * - |targetLanguage| should be specified as the language code (e.g., "de" for German).
     * - Passing an empty string causes behavior to revert to default.
     * - Specifying a non-empty target language will also result in the following behaviors (all of
     *   which are intentional as part of the semantics of having a target language):
     *   - Translation is initiated automatically (note that the infobar UI is present)
     *   - Translation occurs even for languages/sites that the user has blocklisted
     *   - Translation occurs even for pages in the user's default locale
     *   - Translation does *not* occur nor is the infobar UI shown for pages in the specified
     *     target language
     */
    public void setTranslateTargetLanguage(@NonNull String targetLanguage) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.setTranslateTargetLanguage(targetLanguage);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Executes the script, and returns the result to the callback if provided.
     * @param useSeparateIsolate If true, runs the script in a separate v8 Isolate. This uses more
     * memory, but separates the injected scrips from scripts in the page. This prevents any
     * potentially malicious interaction between first-party scripts in the page, and injected
     * scripts. Use with caution, only pass false for this argument if you know this isn't an issue
     * or you need to interact with first-party scripts.
     */
    public void executeScript(@NonNull String script, boolean useSeparateIsolate,
            @Nullable ValueCallback<String> callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.executeScript(script, useSeparateIsolate, ObjectWrapper.wrap(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Runs the beforeunload handler for the main frame or any sub frame, if necessary; otherwise,
     * asynchronously closes the tab.
     *
     * If there is a beforeunload handler a dialog is shown to the user which will allow them to
     * choose whether to proceed with closing the tab. WebLayer closes the tab internally and the
     * embedder will be notified via TabListCallback#onTabRemoved(). The tab will not close if the
     * user chooses to cancel the action. If there is no beforeunload handler, the tab closure will
     * be asynchronous (but immediate) and will be notified in the same way.
     *
     * To close the tab synchronously without running beforeunload, use {@link Browser#destroyTab}.
     */
    public void dispatchBeforeUnloadAndClose() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.dispatchBeforeUnloadAndClose();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Dismisses one active transient UI, if any.
     *
     * This is useful, for example, to handle presses on the system back button. UI such as tab
     * modal dialogs, text selection popups and fullscreen will be dismissed. At most one piece of
     * UI will be dismissed, but this distinction isn't very meaningful in practice since only one
     * such kind of UI would tend to be active at a time.
     *
     * @return true if some piece of UI was dismissed, or false if nothing happened.
     */
    public boolean dismissTransientUi() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            return mImpl.dismissTransientUi();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setNewTabCallback(@Nullable NewTabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mNewTabCallback = callback;
        try {
            mImpl.setNewTabsEnabled(mNewTabCallback != null);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Nullable
    public FullscreenCallback getFullscreenCallback() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return mFullscreenCallbackClient != null ? mFullscreenCallbackClient.getCallback() : null;
    }

    @NonNull
    public NavigationController getNavigationController() {
        throwIfDestroyed();
        return mNavigationController;
    }

    @NonNull
    public FindInPageController getFindInPageController() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return mFindInPageController;
    }

    @NonNull
    public MediaCaptureController getMediaCaptureController() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return mMediaCaptureController;
    }

    public void registerTabCallback(@NonNull TabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mCallbacks.addObserver(callback);
    }

    public void unregisterTabCallback(@NonNull TabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mCallbacks.removeObserver(callback);
    }

    /**
     * Registers {@link callback} to be notified when the scroll offset changes. <b>WARNING:</b>
     * adding a {@link ScrollOffsetCallback} impacts performance, ensure
     * {@link ScrollOffsetCallback} are only installed when needed. See {@link ScrollOffsetCallback}
     * for more details.
     *
     * @param callback The ScrollOffsetCallback to notify
     */
    public void registerScrollOffsetCallback(@NonNull ScrollOffsetCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (mScrollOffsetCallbacks.isEmpty()) {
            try {
                mImpl.setScrollOffsetsEnabled(true);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        mScrollOffsetCallbacks.addObserver(callback);
    }

    public void unregisterScrollOffsetCallback(@NonNull ScrollOffsetCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mScrollOffsetCallbacks.removeObserver(callback);
        if (mScrollOffsetCallbacks.isEmpty()) {
            try {
                mImpl.setScrollOffsetsEnabled(false);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
    }

    /**
     * Take a screenshot of this tab and return it as a Bitmap.
     * This API captures only the web content, not any Java Views, including the
     * view in Browser.setTopView. The browser top view shrinks the height of
     * the screenshot if it is not completely hidden.
     * This method will fail if
     * * the Fragment of this Tab is not started during the operation
     * * this tab is not the active tab in its Browser
     * * if scale is not in the range (0, 1]
     * * Bitmap allocation fails
     * The API is asynchronous when successful, but can be synchronous on
     * failure. So embedder must take care when implementing resultCallback to
     * allow reentrancy.
     * @param scale Scale applied to the Bitmap.
     * @param resultCallback Called when operation is complete.
     */
    public void captureScreenShot(float scale, @NonNull CaptureScreenShotCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.captureScreenShot(scale,
                    ObjectWrapper.wrap(
                            (ValueCallback<Pair<Bitmap, Integer>>) (Pair<Bitmap, Integer> pair) -> {
                                callback.onScreenShotCaptured(pair.first, pair.second);
                            }));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    ITab getITab() {
        return mImpl;
    }

    /**
     * Returns a unique id that persists across restarts.
     *
     * @return the unique id.
     */
    @NonNull
    public String getGuid() {
        return mGuid;
    }

    @NonNull
    TabNavigationControllerProxy getTabNavigationControllerProxy() {
        return mTabNavigationControllerProxy;
    }

    @NonNull
    TabProxy getTabProxy() {
        return mTabProxy;
    }

    /**
     * Set arbitrary data on the tab. This will be saved and restored with the browser, so it is
     * important to keep this data as small as possible.
     *
     * @param data The data to set, must be smaller than 4K when serialized. A snapshot of this data
     *   is taken, so any changes to the passed in object after this call will not be reflected.
     *
     * @throws IllegalArgumentException if the serialzed size of the data exceeds 4K.
     */
    public void setData(@NonNull Map<String, String> data) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            if (!mImpl.setData(data)) {
                throw new IllegalArgumentException("Data given to Tab.setData() was too large.");
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Get arbitrary data set on the tab with setData().
     *
     * @return the data or an empty map if no data was set.
     */
    @NonNull
    public Map<String, String> getData() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            return (Map<String, String>) mImpl.getData();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets a callback to intercept interaction with GAIA accounts. If this callback is set, any
     * link that would result in a change to a user's GAIA account state will trigger a call to
     * {@link GoogleAccountsCallback#onGoogleAccountsRequest}.
     */
    public void setGoogleAccountsCallback(@Nullable GoogleAccountsCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.setGoogleAccountsCallbackClient(
                    callback == null ? null : new GoogleAccountsCallbackClientImpl(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets a callback to present warning dialogs gating external intent launches in incognito mode.
     * If this callback is set, any such pending intent launch will trigger a call to {@link
     * ExternalIntentInIncognitoCallback#onExternalIntentInIncognito}.
     * @since 93
     */
    public void setExternalIntentInIncognitoCallback(
            @Nullable ExternalIntentInIncognitoCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (WebLayer.getSupportedMajorVersionInternal() < 93) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.setExternalIntentInIncognitoCallbackClient(callback == null
                            ? null
                            : new ExternalIntentInIncognitoCallbackClientImpl(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a WebMessageCallback and injects a JavaScript object into each frame that the
     * WebMessageCallback will listen on.
     *
     * <p>
     * The injected JavaScript object will be named {@code jsObjectName} in the global scope. This
     * will inject the JavaScript object in any frame whose origin matches {@code
     * allowedOriginRules} for every navigation after this call, and the JavaScript object will be
     * available immediately when the page begins to load.
     *
     * <p>
     * Each {@code allowedOriginRules} entry must follow the format {@code SCHEME "://" [
     * HOSTNAME_PATTERN [ ":" PORT ] ]}, each part is explained in the below table:
     *
     * <table>
     * <col width="25%">
     * <tr><th>Rule</th><th>Description</th><th>Example</th></tr>
     *
     * <tr>
     * <td>http/https with hostname</td>
     * <td>{@code SCHEME} is http or https; {@code HOSTNAME_PATTERN} is a regular hostname; {@code
     * PORT} is optional, when not present, the rule will match port {@code 80} for http and port
     * {@code 443} for https.</td>
     * <td><ul>
     * <li>{@code https://foobar.com:8080} - Matches https:// URL on port 8080, whose normalized
     * host is foobar.com.</li>
     * <li>{@code https://www.example.com} - Matches https:// URL on port 443, whose normalized host
     * is www.example.com.</li>
     * </ul></td>
     * </tr>
     *
     * <tr>
     * <td>http/https with pattern matching</td>
     * <td>{@code SCHEME} is http or https; {@code HOSTNAME_PATTERN} is a sub-domain matching
     * pattern with a leading {@code *.}; {@code PORT} is optional, when not present, the rule will
     * match port {@code 80} for http and port {@code 443} for https.</td>
     *
     * <td><ul>
     * <li>{@code https://*.example.com} - Matches https://calendar.example.com and
     * https://foo.bar.example.com but not https://example.com.</li>
     * <li>{@code https://*.example.com:8080} - Matches https://calendar.example.com:8080</li>
     * </ul></td>
     * </tr>
     *
     * <tr>
     * <td>http/https with IP literal</td>
     * <td>{@code SCHEME} is https or https; {@code HOSTNAME_PATTERN} is IP literal; {@code PORT} is
     * optional, when not present, the rule will match port {@code 80} for http and port {@code 443}
     * for https.</td>
     *
     * <td><ul>
     * <li>{@code https://127.0.0.1} - Matches https:// URL on port 443, whose IPv4 address is
     * 127.0.0.1</li>
     * <li>{@code https://[::1]} or {@code https://[0:0::1]}- Matches any URL to the IPv6 loopback
     * address with port 443.</li>
     * <li>{@code https://[::1]:99} - Matches any https:// URL to the IPv6 loopback on port 99.</li>
     * </ul></td>
     * </tr>
     *
     * <tr>
     * <td>Custom scheme</td>
     * <td>{@code SCHEME} is a custom scheme; {@code HOSTNAME_PATTERN} and {@code PORT} must not be
     * present.</td>
     * <td><ul>
     * <li>{@code my-app-scheme://} - Matches any my-app-scheme:// URL.</li>
     * </ul></td>
     * </tr>
     *
     * <tr><td>{@code *}</td>
     * <td>Wildcard rule, matches any origin.</td>
     * <td><ul><li>{@code *}</li></ul></td>
     * </table>
     *
     * <p>
     * Note that this is a powerful API, as the JavaScript object will be injected when the frame's
     * origin matches any one of the allowed origins. The HTTPS scheme is strongly recommended for
     * security; allowing HTTP origins exposes the injected object to any potential network-based
     * attackers. If a wildcard {@code "*"} is provided, it will inject the JavaScript object to all
     * frames. A wildcard should only be used if the app wants <b>any</b> third party web page to be
     * able to use the injected object. When using a wildcard, the app must treat received messages
     * as untrustworthy and validate any data carefully.
     *
     * <p>
     * This method can be called multiple times to inject multiple JavaScript objects.
     *
     * <p>
     * Let's say the injected JavaScript object is named {@code myObject}. We will have following
     * methods on that object once it is available to use:
     * <pre class="prettyprint">
     * // message needs to be a JavaScript String.
     * myObject.postMessage(message)
     *
     * // To receive the message posted from the app side. event has a "data" property, which is the
     * // message string from the app side.
     * myObject.onmessage(event)
     *
     * // To be compatible with DOM EventTarget's addEventListener, it accepts type and listener
     * // parameters, where type can be only "message" type and listener can only be a JavaScript
     * // function for myObject. An event object will be passed to listener with a "data" property,
     * // which is the message string from the app side.
     * myObject.addEventListener(type, listener)
     *
     * // To be compatible with DOM EventTarget's removeEventListener, it accepts type and listener
     * // parameters, where type can be only "message" type and listener can only be a JavaScript
     * // function for myObject.
     * myObject.removeEventListener(type, listener)
     * </pre>
     *
     * <p>
     * We start the communication between JavaScript and the app from the JavaScript side. In order
     * to send message from the app to JavaScript, it needs to post a message from JavaScript first,
     * so the app will have a {@link WebMessageReplyProxy} object to respond. Example:
     * <pre class="prettyprint">
     * // Web page (in JavaScript)
     * myObject.onmessage = function(event) {
     *   // prints "Got it!" when we receive the app's response.
     *   console.log(event.data);
     * }
     * myObject.postMessage("I'm ready!");
     *
     * // App (in Java)
     * WebMessageCallback callback = new WebMessageCallback() {
     *   &#064;Override
     *   public void onWebMessageReceived(WebMessageReplyProxy proxy,
     *                                    WebMessage message) {
     *     // do something about view, message, sourceOrigin and isMainFrame.
     *     proxy.postMessage(new WebMessage("Got it!"));
     *   }
     * };
     * tab.registerWebMessageCallback(callback, "myObject", rules);
     * </pre>
     *
     * @param callback The WebMessageCallback to notify of messages.
     * @param jsObjectName The name to give the injected JavaScript object.
     * @param allowedOrigins The set of allowed origins.
     *
     * @throws IllegalArgumentException if jsObjectName or allowedOrigins is invalid.
     */
    public void registerWebMessageCallback(@NonNull WebMessageCallback callback,
            @NonNull String jsObjectName, @NonNull List<String> allowedOrigins) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.registerWebMessageCallback(
                    jsObjectName, allowedOrigins, new WebMessageCallbackClientImpl(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Removes the JavaScript object previously registered by way of registerWebMessageCallback.
     * This impacts future navigations (not any already loaded navigations).
     *
     * @param jsObjectName Name of the JavaScript object.
     */
    public void unregisterWebMessageCallback(@NonNull String jsObjectName) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.unregisterWebMessageCallback(jsObjectName);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if the content displayed in this tab can be translated.
     */
    public boolean canTranslate() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            return mImpl.canTranslate();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Shows the UI which allows the user to translate the content displayed in this tab.
     */
    public void showTranslateUi() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.showTranslateUi();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Allow controlling and overriding custom items in the floating seleciton menu.
     * Note floating action mode is available on M and up.
     * @param actionModeItemTypes a bit field of values in ActionModeItemType.
     * @param callback can be null if actionModeItemTypes is 0.
     *
     * @since 88
     */
    public void setFloatingActionModeOverride(
            int actionModeItemTypes, @Nullable ActionModeCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        mActionModeCallback = callback;
        try {
            mImpl.setFloatingActionModeOverride(actionModeItemTypes);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Turns on desktop user agent if enable is true, otherwise reverts back to mobile user agent.
     * The selected user agent will be used for future navigations until this method is called
     * again. Each navigation saves the user agent mode it was navigated with and will reuse that on
     * back/forward navigations. The tab will be reloaded with the new user agent.
     * @param enable if true requests desktop site, otherwise mobile site.
     *
     * @since 88
     */
    public void setDesktopUserAgentEnabled(boolean enable) {
        if (WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.setDesktopUserAgentEnabled(enable);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if the currently loaded page used a desktop user agent.
     *
     * @since 88
     */
    public boolean isDesktopUserAgentEnabled() {
        if (WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        try {
            return mImpl.isDesktopUserAgentEnabled();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Downloads the item linked to from the context menu. This could be an image/video or link.
     * This will request the WRITE_EXTERNAL_STORAGE permission if it's not granted to the app.
     *
     * @throws IllegalArgumentException if {@link ContextMenuParams.canDownload} is false or if
     *         the ContextMenuParams object parameter wasn't constructed by WebLayer.
     *
     * @since 88
     */
    public void download(ContextMenuParams contextMenuParams) {
        if (WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        if (!contextMenuParams.canDownload) {
            throw new IllegalArgumentException("ContextMenuParams not downloadable.");
        }
        if (contextMenuParams.mContextMenuParams == null) {
            throw new IllegalArgumentException("ContextMenuParams not constructed by WebLayer.");
        }

        try {
            mImpl.download(contextMenuParams.mContextMenuParams);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Experimental (for now) API to trigger the AddToHomescreen dialog for the page in the tab.
     * This adds a homescreen shortcut for it, or installs as a PWA or WebAPK.
     *
     * @since 90
     */
    private void addToHomescreen() {
        if (WebLayer.getSupportedMajorVersionInternal() < 90) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.addToHomescreen();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private static final class WebMessageCallbackClientImpl extends IWebMessageCallbackClient.Stub {
        private final WebMessageCallback mCallback;
        // Maps from id of IWebMessageReplyProxy to WebMessageReplyProxy. This is done to avoid AIDL
        // creating an implementation of IWebMessageReplyProxy every time onPostMessage() is called.
        private final Map<Integer, WebMessageReplyProxy> mProxyIdToProxy =
                new HashMap<Integer, WebMessageReplyProxy>();

        private WebMessageCallbackClientImpl(WebMessageCallback callback) {
            mCallback = callback;
        }

        @Override
        public void onNewReplyProxy(IWebMessageReplyProxy proxy, int proxyId, boolean isMainFrame,
                String sourceOrigin) {
            StrictModeWorkaround.apply();
            assert mProxyIdToProxy.get(proxyId) == null;
            mProxyIdToProxy.put(
                    proxyId, new WebMessageReplyProxy(proxy, isMainFrame, sourceOrigin));
        }

        @Override
        public void onPostMessage(int proxyId, String string) {
            StrictModeWorkaround.apply();
            assert mProxyIdToProxy.get(proxyId) != null;
            mCallback.onWebMessageReceived(mProxyIdToProxy.get(proxyId), new WebMessage(string));
        }

        @Override
        public void onReplyProxyDestroyed(int proxyId) {
            StrictModeWorkaround.apply();
            WebMessageReplyProxy proxy = mProxyIdToProxy.get(proxyId);
            assert proxy != null;
            proxy.markClosed();
            mProxyIdToProxy.remove(proxyId);
            mCallback.onWebMessageReplyProxyClosed(proxy);
        }

        @Override
        public void onReplyProxyActiveStateChanged(int proxyId) {
            StrictModeWorkaround.apply();
            WebMessageReplyProxy proxy = mProxyIdToProxy.get(proxyId);
            assert proxy != null;
            mCallback.onWebMessageReplyProxyActiveStateChanged(proxy);
        }

        @Override
        public void onSetPage(int proxyId, IClientPage clientPage) {
            StrictModeWorkaround.apply();
            assert mProxyIdToProxy.get(proxyId) != null;
            mProxyIdToProxy.get(proxyId).setPage((Page) clientPage);
        }
    }

    private final class TabClientImpl extends ITabClient.Stub {
        @Override
        public void visibleUriChanged(String uriString) {
            StrictModeWorkaround.apply();
            Uri uri = Uri.parse(uriString);
            for (TabCallback callback : mCallbacks) {
                callback.onVisibleUriChanged(uri);
            }
        }

        @Override
        public void onNewTab(int tabId, int mode) {
            StrictModeWorkaround.apply();
            // This should only be hit if setNewTabCallback() has been called with a non-null
            // value.
            assert mNewTabCallback != null;
            Tab tab = getTabById(tabId);
            // Tab should have already been created by way of BrowserClient.
            assert tab != null;
            assert tab.getBrowser() == getBrowser();
            mNewTabCallback.onNewTab(tab, mode);
        }

        @Override
        public void onTabDestroyed() {
            unregisterTab(Tab.this);
            // Ensure that the app will fail fast if the embedder mistakenly tries to call back
            // into the implementation via this Tab.
            mImpl = null;
            mDestroyStack = new RuntimeException("onTabDestroyed");
        }

        @Override
        public void onRenderProcessGone() {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onRenderProcessGone();
            }
        }

        @Override
        public void showContextMenu(IObjectWrapper pageUrl, IObjectWrapper linkUrl,
                IObjectWrapper linkText, IObjectWrapper titleOrAltText, IObjectWrapper srcUrl) {
            showContextMenu2(
                    pageUrl, linkUrl, linkText, titleOrAltText, srcUrl, false, false, false, null);
        }

        @Override
        public void showContextMenu2(IObjectWrapper pageUrl, IObjectWrapper linkUrl,
                IObjectWrapper linkText, IObjectWrapper titleOrAltText, IObjectWrapper srcUrl,
                boolean isImage, boolean isVideo, boolean canDownload,
                IContextMenuParams contextMenuParams) {
            StrictModeWorkaround.apply();
            String pageUrlString = ObjectWrapper.unwrap(pageUrl, String.class);
            String linkUrlString = ObjectWrapper.unwrap(linkUrl, String.class);
            String srcUrlString = ObjectWrapper.unwrap(srcUrl, String.class);
            ContextMenuParams params = new ContextMenuParams(Uri.parse(pageUrlString),
                    linkUrlString != null ? Uri.parse(linkUrlString) : null,
                    ObjectWrapper.unwrap(linkText, String.class),
                    ObjectWrapper.unwrap(titleOrAltText, String.class),
                    srcUrlString != null ? Uri.parse(srcUrlString) : null, isImage, isVideo,
                    canDownload, contextMenuParams);
            for (TabCallback callback : mCallbacks) {
                callback.showContextMenu(params);
            }
        }

        @Override
        public void onTabModalStateChanged(boolean isTabModalShowing) {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onTabModalStateChanged(isTabModalShowing);
            }
        }

        @Override
        public void onTitleUpdated(IObjectWrapper title) {
            StrictModeWorkaround.apply();
            String titleString = ObjectWrapper.unwrap(title, String.class);
            for (TabCallback callback : mCallbacks) {
                callback.onTitleUpdated(titleString);
            }
        }

        @Override
        public void bringTabToFront() {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.bringTabToFront();
            }
        }

        @Override
        public void onBackgroundColorChanged(int color) {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onBackgroundColorChanged(color);
            }
        }

        @Override
        public void onScrollNotification(
                @ScrollNotificationType int notificationType, float currentScrollRatio) {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onScrollNotification(notificationType, currentScrollRatio);
            }
        }

        @Override
        public void onVerticalScrollOffsetChanged(int value) {
            StrictModeWorkaround.apply();
            for (ScrollOffsetCallback callback : mScrollOffsetCallbacks) {
                callback.onVerticalScrollOffsetChanged(value);
            }
        }

        @Override
        public void onActionItemClicked(
                int actionModeItemType, IObjectWrapper selectedStringWrapper) {
            StrictModeWorkaround.apply();
            String selectedString = ObjectWrapper.unwrap(selectedStringWrapper, String.class);
            if (mActionModeCallback != null) {
                mActionModeCallback.onActionItemClicked(actionModeItemType, selectedString);
            }
        }

        @Override
        public void onVerticalOverscroll(float accumulatedOverscrollY) {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onVerticalOverscroll(accumulatedOverscrollY);
            }
        }
    }

    private static final class ErrorPageCallbackClientImpl extends IErrorPageCallbackClient.Stub {
        private final ErrorPageCallback mCallback;

        ErrorPageCallbackClientImpl(ErrorPageCallback callback) {
            mCallback = callback;
        }

        public ErrorPageCallback getCallback() {
            return mCallback;
        }

        @Override
        public boolean onBackToSafety() {
            StrictModeWorkaround.apply();
            return mCallback.onBackToSafety();
        }
        @Override
        public String getErrorPageContent(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            ErrorPage errorPage = mCallback.getErrorPage((Navigation) navigation);
            return errorPage == null ? null : errorPage.htmlContent;
        }
    }

    private static final class FullscreenCallbackClientImpl extends IFullscreenCallbackClient.Stub {
        private FullscreenCallback mCallback;

        /* package */ FullscreenCallbackClientImpl(FullscreenCallback callback) {
            mCallback = callback;
        }

        public FullscreenCallback getCallback() {
            return mCallback;
        }

        @Override
        public void enterFullscreen(IObjectWrapper exitFullscreenWrapper) {
            StrictModeWorkaround.apply();
            ValueCallback<Void> exitFullscreenCallback = (ValueCallback<Void>) ObjectWrapper.unwrap(
                    exitFullscreenWrapper, ValueCallback.class);
            mCallback.onEnterFullscreen(() -> exitFullscreenCallback.onReceiveValue(null));
        }

        @Override
        public void exitFullscreen() {
            StrictModeWorkaround.apply();
            mCallback.onExitFullscreen();
        }
    }

    private static final class GoogleAccountsCallbackClientImpl
            extends IGoogleAccountsCallbackClient.Stub {
        private GoogleAccountsCallback mCallback;

        GoogleAccountsCallbackClientImpl(GoogleAccountsCallback callback) {
            mCallback = callback;
        }

        @Override
        public void onGoogleAccountsRequest(
                int serviceType, String email, String continueUrl, boolean isSameTab) {
            StrictModeWorkaround.apply();
            mCallback.onGoogleAccountsRequest(new GoogleAccountsParams(
                    serviceType, email, Uri.parse(continueUrl), isSameTab));
        }

        @Override
        public String getGaiaId() {
            StrictModeWorkaround.apply();
            return mCallback.getGaiaId();
        }
    }

    private static final class ExternalIntentInIncognitoCallbackClientImpl
            extends IExternalIntentInIncognitoCallbackClient.Stub {
        private ExternalIntentInIncognitoCallback mCallback;

        ExternalIntentInIncognitoCallbackClientImpl(ExternalIntentInIncognitoCallback callback) {
            mCallback = callback;
        }

        @Override
        public void onExternalIntentInIncognito(IObjectWrapper onUserDecisionWrapper) {
            StrictModeWorkaround.apply();
            ValueCallback<Integer> valueCallback =
                    ObjectWrapper.unwrap(onUserDecisionWrapper, ValueCallback.class);

            mCallback.onExternalIntentInIncognito(
                    (userDecision) -> valueCallback.onReceiveValue(userDecision));
        }
    }
}
