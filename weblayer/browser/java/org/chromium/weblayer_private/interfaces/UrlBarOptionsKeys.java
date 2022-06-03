// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/** Keys for the Bundle of arguments with which BrowserFragments are created. */
public interface UrlBarOptionsKeys {
    /**
     * If true, clicking on the url shows page info. Default is false.
     */
    String SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED = "ShowPageInfoWhenUrlTextClicked";
    /**
     * If true, shows publisher url. Default is false.
     * @since 88
     */
    String SHOW_PUBLISHER_URL = "ShowPublisherUrl";
    String URL_ICON_COLOR = "UrlIconColor";
    String URL_TEXT_COLOR = "UrlTextColor";
    String URL_TEXT_SIZE = "UrlTextSize";
}
