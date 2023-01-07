// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IUserIdentityCallbackClient;
import org.chromium.weblayer_private.interfaces.IGoogleAccountAccessTokenFetcherClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IOpenUrlCallbackClient;
import org.chromium.weblayer_private.interfaces.IPrerenderController;
import org.chromium.weblayer_private.interfaces.IProfileClient;

interface IProfile {
  void destroy() = 0;

  void clearBrowsingData(in int[] dataTypes, long fromMillis, long toMillis,
          in IObjectWrapper completionCallback) = 1;

  String getName() = 2;

  void setDownloadDirectory(String directory) = 3;

  void destroyAndDeleteDataFromDisk(in IObjectWrapper completionCallback) = 4;

  void setDownloadCallbackClient(IDownloadCallbackClient client) = 5;

  ICookieManager getCookieManager() = 6;

  void setBooleanSetting(int type, boolean value) = 7;
  boolean getBooleanSetting(int type) = 8;

  void getBrowserPersistenceIds(in IObjectWrapper resultCallback) = 9;
  void removeBrowserPersistenceStorage(in String[] ids,
                                       in IObjectWrapper resultCallback) = 10;
  void prepareForPossibleCrossOriginNavigation() = 11;

  void getCachedFaviconForPageUri(in String uri,
                                  in IObjectWrapper resultCallback) = 12;

  void setUserIdentityCallbackClient(IUserIdentityCallbackClient client) = 13;
  IPrerenderController getPrerenderController() = 15;
  boolean isIncognito() = 16;
  void setClient(in IProfileClient client) = 17;
  void destroyAndDeleteDataFromDiskSoon(in IObjectWrapper completeCallback) = 18;

  // Added in 89.
  void setGoogleAccountAccessTokenFetcherClient(IGoogleAccountAccessTokenFetcherClient client) = 19;

  // Added in 91.
  void setTablessOpenUrlCallbackClient(IOpenUrlCallbackClient client) = 20;
}
