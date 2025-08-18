// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import org.chromium.build.annotations.NullMarked;

/** Marker interface that a class is subscribe to incognito state update. */
@NullMarked
public interface IncognitoAware extends ThemeResourceWrapper.ThemeObserver {}
