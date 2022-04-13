// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.autofill_assistant;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

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
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.weblayer_private.WebLayerAccessibilityUtil;

/**
 * Provides default implementations of {@link AssistantStaticDependencies} for WebLayer.
 */
@JNINamespace("weblayer")
public class WebLayerAssistantStaticDependencies implements AssistantStaticDependencies {
    @Override
    public long createNative() {
        return WebLayerAssistantStaticDependenciesJni.get().init(
                new WebLayerAssistantStaticDependencies());
    }

    @Override
    public AssistantDependencies createDependencies(Activity activity) {
        return new WebLayerAssistantDependencies(activity);
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
        // TODO(b/222671580): Implement
        return null;
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
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public ImageFetcher createImageFetcher() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public LargeIconBridge createIconBridge() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    @Nullable
    public String getSignedInAccountEmailOrNull() {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    @Nullable
    public AssistantProfileImageUtil createProfileImageUtilOrNull(
            Context context, @DimenRes int imageSizeRedId) {
        // TODO(b/222671580): Implement
        return null;
    }

    @Override
    public AssistantEditorFactory createEditorFactory() {
        // TODO(b/222671580): Implement
        return null;
    }

    @NativeMethods
    interface Natives {
        long init(AssistantStaticDependencies staticDependencies);
    }
}
