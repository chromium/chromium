// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.weblayer_private.interfaces.IClientPage;

/**
 * This objects tracks the lifetime of a loaded web page. Most of the time there is only one Page
 * object per tab at a time. However features like back-forward cache, prerendering etc... sometime
 * involve the creation of additional Page object. {@link Navigation.getPage} will return the Page
 * for a given navigation. Similarly it'll the same Page object that's passed in
 * {@link NavigationCallback.onPageDestroyed}.
 *
 * @since 90
 */
class Page extends IClientPage.Stub {}
