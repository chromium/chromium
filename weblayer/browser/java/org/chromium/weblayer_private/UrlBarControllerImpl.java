// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.SecurityButtonAnimationDelegate;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoHighlight;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IUrlBarController;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.interfaces.UrlBarOptionsKeys;

/**
 *  Implementation of {@link IUrlBarController}.
 */
@JNINamespace("weblayer")
// This isn't part of Chrome, so using explicit colors/sizes is ok.
@SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
public class UrlBarControllerImpl extends IUrlBarController.Stub {
    public static final float DEFAULT_TEXT_SIZE = 10.0F;
    public static final float MINIMUM_TEXT_SIZE = 5.0F;

    private BrowserImpl mBrowserImpl;
    private long mNativeUrlBarController;
    // A count of how many Views created by this controller are attached to a Window.
    private int mActiveViewCount;
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private String getUrlForDisplay() {
        return UrlBarControllerImplJni.get().getUrlForDisplay(mNativeUrlBarController);
    }

    void destroy() {
        UrlBarControllerImplJni.get().deleteUrlBarController(mNativeUrlBarController);
        mNativeUrlBarController = 0;
        mBrowserImpl = null;

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    public UrlBarControllerImpl(BrowserImpl browserImpl, long nativeBrowser) {
        mBrowserImpl = browserImpl;
        mNativeUrlBarController =
                UrlBarControllerImplJni.get().createUrlBarController(nativeBrowser);
    }

    void addActiveView() {
        mActiveViewCount++;
    }

    void removeActiveView() {
        mActiveViewCount--;
    }

    boolean hasActiveView() {
        return mActiveViewCount != 0;
    }

    @Override
    public IObjectWrapper /* View */ createUrlBarView(Bundle options,
            @Nullable IObjectWrapper /* OnLongClickListener */ clickListener,
            @Nullable IObjectWrapper /* OnLongClickListener */ longClickListener) {
        StrictModeWorkaround.apply();
        if (mBrowserImpl == null) {
            throw new IllegalStateException("UrlBarView cannot be created without a valid Browser");
        }
        Context context = mBrowserImpl.getContext();
        if (context == null) throw new IllegalStateException("BrowserFragment not attached yet.");

        UrlBarView urlBarView =
                new UrlBarView(this, context, options, clickListener, longClickListener);
        return ObjectWrapper.wrap(urlBarView);
    }

    @Override
    public void showPageInfo(@NonNull Bundle bundle) {
        if (mBrowserImpl == null) {
            throw new IllegalStateException("Page info can not be shown without a valid Browser");
        }
        if (!mBrowserImpl.isViewAttachedToWindow()) {
            throw new IllegalStateException("Must be attached to window to show page info");
        }
        final boolean showPublisherUrl =
                bundle.getBoolean(UrlBarOptionsKeys.SHOW_PUBLISHER_URL, /*default= */ false);
        showPageInfoUi(showPublisherUrl);
    }

    private void showPageInfoUi(boolean showPublisherUrl) {
        WebContents webContents = mBrowserImpl.getActiveTab().getWebContents();

        String publisherUrl = null;
        if (showPublisherUrl) {
            String publisherUrlMaybeNull =
                    UrlBarControllerImplJni.get().getPublisherUrl(mNativeUrlBarController);
            if (publisherUrlMaybeNull != null && !TextUtils.isEmpty(publisherUrlMaybeNull)) {
                publisherUrl = UrlUtilities.extractPublisherFromPublisherUrl(publisherUrlMaybeNull);
            }
        }

        PageInfoController.show(mBrowserImpl.getWindowAndroid().getActivity().get(), webContents,
                publisherUrl, PageInfoController.OpenedFromSource.TOOLBAR,
                PageInfoControllerDelegateImpl.create(webContents),
                PageInfoHighlight.noHighlight());
    }

    protected class UrlBarView
            extends LinearLayout implements BrowserImpl.VisibleSecurityStateObserver {
        private final UrlBarControllerImpl mController;
        private float mTextSize;
        private boolean mShowPageInfoWhenUrlTextClicked;
        private boolean mShowPublisherUrl;

        // These refer to the resources in the embedder's APK, not WebLayer's.
        private @ColorRes int mUrlTextColor;
        private @ColorRes int mUrlIconColor;

        private TextView mUrlTextView;
        private ImageButton mSecurityButton;
        private final SecurityButtonAnimationDelegate mSecurityButtonAnimationDelegate;
        OnClickListener mUrlBarClickListener;
        OnLongClickListener mUrlBarLongClickListener;

        public UrlBarView(@NonNull UrlBarControllerImpl controller, @NonNull Context context,
                @NonNull Bundle options,
                @Nullable IObjectWrapper /* OnClickListener */ clickListener,
                @Nullable IObjectWrapper /* OnLongClickListener */ longClickListener) {
            super(context);
            mController = controller;
            setGravity(Gravity.CENTER_HORIZONTAL);

            mTextSize = options.getFloat(UrlBarOptionsKeys.URL_TEXT_SIZE, DEFAULT_TEXT_SIZE);
            mShowPageInfoWhenUrlTextClicked = options.getBoolean(
                    UrlBarOptionsKeys.SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED, /*default= */ false);
            mShowPublisherUrl =
                    options.getBoolean(UrlBarOptionsKeys.SHOW_PUBLISHER_URL, /*default= */ false);
            mUrlTextColor = options.getInt(UrlBarOptionsKeys.URL_TEXT_COLOR, /*default= */ 0);
            mUrlIconColor = options.getInt(UrlBarOptionsKeys.URL_ICON_COLOR, /*default= */ 0);

            View.inflate(getContext(), R.layout.weblayer_url_bar, this);
            setOrientation(LinearLayout.HORIZONTAL);
            setBackgroundColor(Color.TRANSPARENT);
            mUrlTextView = findViewById(R.id.url_text);
            mSecurityButton = (ImageButton) findViewById(R.id.security_button);
            mSecurityButtonAnimationDelegate = new SecurityButtonAnimationDelegate(
                    mSecurityButton, mUrlTextView, R.dimen.security_status_icon_size);
            mUrlBarClickListener = ObjectWrapper.unwrap(clickListener, OnClickListener.class);
            mUrlBarLongClickListener =
                    ObjectWrapper.unwrap(longClickListener, OnLongClickListener.class);

            // NOTE: We don't animate the security button update here because this is not a change
            // per se but rather an initial setting of the state. See crbug.com/1247666 for details.
            updateView(/*animateSecurityButtonUpdate=*/false);
        }

        // BrowserImpl.VisibleSecurityStateObserver
        @Override
        public void onVisibleSecurityStateOfActiveTabChanged() {
            updateView(/*animateSecurityButtonUpdate=*/true);
        }

        @Override
        protected void onAttachedToWindow() {
            if (mBrowserImpl != null) {
                mBrowserImpl.addVisibleSecurityStateObserver(this);

                // NOTE: We don't animate the security button update here because this is not a
                // change per se but rather an initial setting of the state. See crbug.com/1247666
                // for details.
                updateView(/*animateSecurityButtonUpdate=*/false);
            }

            super.onAttachedToWindow();
            mController.addActiveView();
        }

        @Override
        protected void onDetachedFromWindow() {
            if (mBrowserImpl != null) mBrowserImpl.removeVisibleSecurityStateObserver(this);
            super.onDetachedFromWindow();
            mController.removeActiveView();
        }

        private void updateView(boolean animateSecurityButtonUpdate) {
            if (mBrowserImpl == null) return;
            int securityLevel = UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                    mNativeUrlBarController);

            String publisherUrl =
                    UrlBarControllerImplJni.get().getPublisherUrl(mNativeUrlBarController);

            String displayUrl;
            int securityIcon;
            if (mShowPublisherUrl && securityLevel != ConnectionSecurityLevel.DANGEROUS
                    && !TextUtils.isEmpty(publisherUrl)) {
                displayUrl = getContext().getResources().getString(R.string.amp_publisher_url,
                        UrlUtilities.extractPublisherFromPublisherUrl(publisherUrl));
                securityIcon = R.drawable.amp_icon;
            } else {
                displayUrl = getUrlForDisplay();
                securityIcon = getSecurityIcon();
            }

            mUrlTextView.setText(displayUrl);
            mUrlTextView.setTextSize(
                    TypedValue.COMPLEX_UNIT_SP, Math.max(MINIMUM_TEXT_SIZE, mTextSize));
            Context embedderContext = mBrowserImpl.getEmbedderActivityContext();
            if (mUrlTextColor != 0 && embedderContext != null) {
                mUrlTextView.setTextColor(ContextCompat.getColor(embedderContext, mUrlTextColor));
            }

            mSecurityButtonAnimationDelegate.updateSecurityButton(
                    securityIcon, animateSecurityButtonUpdate);

            mSecurityButton.setContentDescription(getContext().getResources().getString(
                    SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(securityLevel)));

            if (mUrlIconColor != 0 && embedderContext != null) {
                ImageViewCompat.setImageTintList(mSecurityButton,
                        ColorStateList.valueOf(
                                ContextCompat.getColor(embedderContext, mUrlIconColor)));
            }

            if (mShowPageInfoWhenUrlTextClicked) {
                // Set clicklisteners on the entire UrlBarView.
                assert (mUrlBarClickListener == null);
                mSecurityButton.setClickable(false);
                setOnClickListener(v -> { showPageInfoUi(mShowPublisherUrl); });

                if (mUrlBarLongClickListener != null) {
                    setOnLongClickListener(mUrlBarLongClickListener);
                }
            } else {
                // Set a clicklistener on the security status and TextView separately. This mode
                // can be used to create an editable URL bar using WebLayer.
                mSecurityButton.setOnClickListener(v -> { showPageInfoUi(mShowPublisherUrl); });
                if (mUrlBarClickListener != null) {
                    mUrlTextView.setOnClickListener(mUrlBarClickListener);
                }
                if (mUrlBarLongClickListener != null) {
                    mUrlTextView.setOnLongClickListener(mUrlBarLongClickListener);
                }
            }
        }

        public boolean showPublisherUrl() {
            return mShowPublisherUrl;
        }

        @DrawableRes
        private int getSecurityIcon() {
            return SecurityStatusIcon.getSecurityIconResource(
                    UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                            mNativeUrlBarController),
                    mBrowserImpl.isWindowOnSmallDevice(),
                    /*skipIconForNeutralState=*/true,
                    /*useUpdatedConnectionSecurityIndicators=*/false);
        }
    }

    @NativeMethods()
    interface Natives {
        long createUrlBarController(long browserPtr);
        void deleteUrlBarController(long urlBarControllerImplPtr);
        String getUrlForDisplay(long nativeUrlBarControllerImpl);
        String getPublisherUrl(long nativeUrlBarControllerImpl);
        int getConnectionSecurityLevel(long nativeUrlBarControllerImpl);
    }
}
