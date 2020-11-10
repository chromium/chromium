// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IUserIdentityCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IPrerenderController;
import org.chromium.weblayer_private.interfaces.IProfileClient;

interface IProfile {
  void destroy() = 0;

  void clearBrowsingData(in int[] dataTypes, long fromMillis, long toMillis,
          in IObjectWrapper completionCallback) = 1;

  String getName() = 2;

  void setDownloadDirectory(String directory) = 3;

  // Added in Version 82.
  void destroyAndDeleteDataFromDisk(in IObjectWrapper completionCallback) = 4;

  // Added in Version 83.
  void setDownloadCallbackClient(IDownloadCallbackClient client) = 5;

  // Added in Version 83.
  ICookieManager getCookieManager() = 6;

  // Added in Version 84.
  void setBooleanSetting(int type, boolean value) = 7;
  boolean getBooleanSetting(int type) = 8;

  // Added in Version 85.
  void getBrowserPersistenceIds(in IObjectWrapper resultCallback) = 9;
  void removeBrowserPersistenceStorage(in String[] ids,
                                       in IObjectWrapper resultCallback) = 10;
  void prepareForPossibleCrossOriginNavigation() = 11;

  // Added in Version 86.
  void getCachedFaviconForPageUri(in String uri,
                                  in IObjectWrapper resultCallback) = 12;

  // Added in Version 87.
  void setUserIdentityCallbackClient(IUserIdentityCallbackClient client) = 13;
  IPrerenderController getPrerenderController() = 15;
  boolean isIncognito() = 16;
  void setClient(in IProfileClient client) = 17;
  void destroyAndDeleteDataFromDiskSoon(in IObjectWrapper completeCallback) = 18;
}
