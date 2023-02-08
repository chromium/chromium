// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IContextMenuParams;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

/**
 * Interface used by Tab to inform the client of changes. This largely duplicates the
 * TabCallback interface, but is a singleton to avoid unnecessary IPC.
 */
interface ITabClient {
  void visibleUriChanged(in String uriString) = 0;

  void onNewTab(in int tabId, in int mode) = 1;

  void onRenderProcessGone() = 2;

  // ID 3 was onCloseTab and was removed in M87.

  // Deprecated in M88.
  void showContextMenu(in IObjectWrapper pageUrl, in IObjectWrapper linkUrl,
      in IObjectWrapper linkText, in IObjectWrapper titleOrAltText,
      in IObjectWrapper srcUrl) = 4;

  void onTabModalStateChanged(in boolean isTabModalShowing) = 5;

  void onTitleUpdated(in IObjectWrapper title) = 6;

  void bringTabToFront() = 7;

  void onTabDestroyed() = 8;

  void onBackgroundColorChanged(in int color) = 9;

  void onScrollNotification(
          in int notificationType, in float currentScrollRatio) = 10;

  void onVerticalScrollOffsetChanged(in int offset) = 11;

  // Added in M88
  void onActionItemClicked(
          in int actionModeItemType, in IObjectWrapper selectedString) = 12;
  void showContextMenu2(in IObjectWrapper pageUrl, in IObjectWrapper linkUrl,
      in IObjectWrapper linkText, in IObjectWrapper titleOrAltText,
      in IObjectWrapper srcUrl, in boolean isImage, in boolean isVideo, in boolean canDownload,
      in IContextMenuParams contextMenuParams) = 13;

  // Added in M101.
  void onVerticalOverscroll(float accumulatedOverscrollY) = 14;

  // Added in M111.
  void onPostMessage(in String message, in String origin) = 15;
}
