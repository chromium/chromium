// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Denotes that the annotated integer represents an Android View visibility value. */
@IntDef({View.VISIBLE, View.INVISIBLE, View.GONE})
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE_USE)
@NullMarked
public @interface ViewVisibility {}
