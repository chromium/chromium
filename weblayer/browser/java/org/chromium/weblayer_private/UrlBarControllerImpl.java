// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Bundle;
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
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.omnibox.SecurityButtonAnimationDelegate;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PermissionParamsListBuilderDelegate;
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
    @Deprecated
    public IObjectWrapper /* View */ deprecatedCreateUrlBarView(Bundle options) {
        return createUrlBarView(
                options, /* OnLongClickListener */ null, /* OnLongClickListener */ null);
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

    protected class UrlBarView
            extends LinearLayout implements BrowserImpl.VisibleSecurityStateObserver {
        private final UrlBarControllerImpl mController;
        private float mTextSize;
        private boolean mShowPageInfoWhenUrlTextClicked;

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

            updateView();
        }

        // BrowserImpl.VisibleSecurityStateObserver
        @Override
        public void onVisibleSecurityStateOfActiveTabChanged() {
            updateView();
        }

        @Override
        protected void onAttachedToWindow() {
            if (mBrowserImpl != null) {
                mBrowserImpl.addVisibleSecurityStateObserver(this);
                updateView();
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

        private void updateView() {
            if (mBrowserImpl == null) return;
            String displayUrl =
                    UrlBarControllerImplJni.get().getUrlForDisplay(mNativeUrlBarController);
            mUrlTextView.setText(displayUrl);
            mUrlTextView.setTextSize(
                    TypedValue.COMPLEX_UNIT_SP, Math.max(MINIMUM_TEXT_SIZE, mTextSize));
            Context embedderContext = mBrowserImpl.getEmbedderActivityContext();
            if (mUrlTextColor != 0 && embedderContext != null) {
                mUrlTextView.setTextColor(ContextCompat.getColor(embedderContext, mUrlTextColor));
            }

            mSecurityButtonAnimationDelegate.updateSecurityButton(getSecurityIcon());
            mSecurityButton.setContentDescription(getContext().getResources().getString(
                    SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(
                            UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                                    mNativeUrlBarController))));

            if (mUrlIconColor != 0 && embedderContext != null) {
                ImageViewCompat.setImageTintList(mSecurityButton,
                        ColorStateList.valueOf(
                                ContextCompat.getColor(embedderContext, mUrlIconColor)));
            }

            if (mShowPageInfoWhenUrlTextClicked) {
                // Set clicklisteners on the entire UrlBarView.
                assert (mUrlBarClickListener == null);
                mSecurityButton.setClickable(false);
                setOnClickListener(v -> { showPageInfoUi(v); });

                if (mUrlBarLongClickListener != null) {
                    setOnLongClickListener(mUrlBarLongClickListener);
                }
            } else {
                // Set a clicklistener on the security status and TextView separately. This mode
                // can be used to create an editable URL bar using WebLayer.
                mSecurityButton.setOnClickListener(v -> { showPageInfoUi(v); });
                if (mUrlBarClickListener != null) {
                    mUrlTextView.setOnClickListener(mUrlBarClickListener);
                }
                if (mUrlBarLongClickListener != null) {
                    mUrlTextView.setOnLongClickListener(mUrlBarLongClickListener);
                }
            }
        }

        private void showPageInfoUi(View v) {
            WebContents webContents = mBrowserImpl.getActiveTab().getWebContents();
            PageInfoController.show(mBrowserImpl.getWindowAndroid().getActivity().get(),
                    webContents,
                    /* contentPublisher= */ null, PageInfoController.OpenedFromSource.TOOLBAR,
                    PageInfoControllerDelegateImpl.create(webContents),
                    new PermissionParamsListBuilderDelegate(mBrowserImpl.getProfile()) {
                        @Override
                        public String getDelegateAppName(
                                Origin origin, @ContentSettingsType int type) {
                            if (type == ContentSettingsType.GEOLOCATION
                                    && WebLayerImpl.isLocationPermissionManaged(origin)) {
                                return WebLayerImpl.getClientApplicationName();
                            }

                            return null;
                        }
                    });
        }

        @DrawableRes
        private int getSecurityIcon() {
            return SecurityStatusIcon.getSecurityIconResource(
                    UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                            mNativeUrlBarController),
                    UrlBarControllerImplJni.get().shouldShowDangerTriangleForWarningLevel(
                            mNativeUrlBarController),
                    mBrowserImpl.isWindowOnSmallDevice(),
                    /* skipIconForNeutralState= */ true);
        }
    }

    @NativeMethods()
    interface Natives {
        long createUrlBarController(long browserPtr);
        void deleteUrlBarController(long urlBarControllerImplPtr);
        String getUrlForDisplay(long nativeUrlBarControllerImpl);
        int getConnectionSecurityLevel(long nativeUrlBarControllerImpl);
        boolean shouldShowDangerTriangleForWarningLevel(long nativeUrlBarControllerImpl);
    }
}
