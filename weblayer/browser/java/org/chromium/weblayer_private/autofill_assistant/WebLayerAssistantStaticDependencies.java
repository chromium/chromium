// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.os.RemoteException;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill_assistant.AssistantAccessTokenUtil;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantEditorFactory;
import org.chromium.components.autofill_assistant.AssistantFeedbackUtil;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.components.autofill_assistant.AssistantProfileImageUtil;
import org.chromium.components.autofill_assistant.AssistantSettingsUtil;
import org.chromium.components.autofill_assistant.AssistantStaticDependencies;
import org.chromium.components.autofill_assistant.AssistantTabObscuringUtil;
import org.chromium.components.autofill_assistant.AssistantTabUtil;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.weblayer_private.ProfileImpl;
import org.chromium.weblayer_private.WebLayerAccessibilityUtil;
import org.chromium.weblayer_private.interfaces.IUserIdentityCallbackClient;

/**
 * Provides default implementations of {@link AssistantStaticDependencies} for WebLayer.
 */
@JNINamespace("weblayer")
public class WebLayerAssistantStaticDependencies
        implements AssistantStaticDependencies, SimpleFactoryKeyHandle {
    protected final WebContents mWebContents;
    protected final WebLayerAssistantTabChangeObserver mWebLayerAssistantTabChangeObserver;

    // There exists one instance of this class per WebContents and per TabImpl.
    WebLayerAssistantStaticDependencies(WebContents webContents,
            WebLayerAssistantTabChangeObserver webLayerAssistantTabChangeObserver) {
        mWebContents = webContents;
        mWebLayerAssistantTabChangeObserver = webLayerAssistantTabChangeObserver;
    }

    // AssistantStaticDependencies implementation:

    @Override
    public long createNative() {
        return WebLayerAssistantStaticDependenciesJni.get().init(
                new WebLayerAssistantStaticDependencies(
                        mWebContents, mWebLayerAssistantTabChangeObserver));
    }

    @Override
    public AssistantDependencies createDependencies(Activity activity) {
        return new WebLayerAssistantDependencies(mWebContents, mWebLayerAssistantTabChangeObserver);
    }

    @Override
    public AccessibilityUtil getAccessibilityUtil() {
        return WebLayerAccessibilityUtil.get();
    }

    @Override
    @Nullable
    public AssistantTabObscuringUtil getTabObscuringUtilOrNull(WindowAndroid windowAndroid) {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantInfoPageUtil createInfoPageUtil() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantFeedbackUtil createFeedbackUtil() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantTabUtil createTabUtil() {
        // This method should do nothing under WebLayer as it is only used to close CCTs in Chrome.
        return (activity) -> {};
    }

    @Override
    public AssistantSettingsUtil createSettingsUtil() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantAccessTokenUtil createAccessTokenUtil() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public BrowserContextHandle getBrowserContext() {
        return WebLayerAssistantStaticDependenciesJni.get().getJavaProfile(mWebContents);
    }

    @Override
    public ImageFetcher createImageFetcher() {
        return ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY, this);
    }

    @Override
    public LargeIconBridge createIconBridge() {
        BrowserContextHandle browserContext = getBrowserContext();
        if (browserContext == null) return null;

        return new LargeIconBridge(getBrowserContext());
    }

    @Nullable
    @CalledByNative
    private String getEmailOrNull(ProfileImpl profile) throws RemoteException {
        IUserIdentityCallbackClient userIdentityCallback = profile.getUserIdentityCallbackClient();
        if (userIdentityCallback == null) return null;

        return userIdentityCallback.getEmail();
    }

    @Override
    @Nullable
    public AssistantProfileImageUtil createProfileImageUtilOrNull(
            Context context, @DimenRes int imageSizeRedId) {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    @Nullable
    public AssistantEditorFactory createEditorFactory() {
        // This factory should not be used in a WebLayer context. All code paths leading to the
        // use of this factory point to a misconfiguration. For WebLayer, the external editors
        // should be used.
        return null;
    }

    // SimpleFactoryKeyHandle implementation:

    @Override
    public long getNativeSimpleFactoryKeyPointer() {
        BrowserContextHandle browserContext = getBrowserContext();
        if (browserContext == null) return 0;
        long nativeBrowserContextPointer = browserContext.getNativeBrowserContextPointer();
        if (nativeBrowserContextPointer == 0) return 0;
        return WebLayerAssistantStaticDependenciesJni.get().getSimpleFactoryKey(
                nativeBrowserContextPointer);
    }

    @NativeMethods
    interface Natives {
        long init(AssistantStaticDependencies staticDependencies);

        ProfileImpl getJavaProfile(WebContents webContents);

        long getSimpleFactoryKey(long browserContext);
    }
}
