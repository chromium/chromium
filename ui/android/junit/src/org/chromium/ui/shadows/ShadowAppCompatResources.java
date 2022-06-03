// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.shadows;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowDrawable;

@Implements(AppCompatResources.class)
public class ShadowAppCompatResources {
    @Implementation
    @Nullable
    public static Drawable getDrawable(@NonNull Context context, @DrawableRes int resId) {
        return ShadowDrawable.createFromResourceId(resId);
    }
}
