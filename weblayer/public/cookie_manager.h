// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_COOKIE_MANAGER_H_
#define WEBLAYER_PUBLIC_COOKIE_MANAGER_H_

#include <string>

#include "base/callback_list.h"

class GURL;

namespace net {
struct CookieChangeInfo;
}

namespace weblayer {

class CookieManager {
 public:
  virtual ~CookieManager() = default;

  // Sets a cookie for the given URL.
  using SetCookieCallback = base::OnceCallback<void(bool)>;
  virtual void SetCookie(const GURL& url,
                         const std::string& value,
                         SetCookieCallback callback) = 0;

  // Gets the cookies for the given URL.
  using GetCookieCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetCookie(const GURL& url, GetCookieCallback callback) = 0;

  // Gets the cookies for the given URL in the form of the 'Set-Cookie' HTTP
  // response header.
  using GetResponseCookiesCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  virtual void GetResponseCookies(const GURL& url,
                                  GetResponseCookiesCallback callback) = 0;

  // Adds a callback to listen for changes to cookies for the given URL.
  using CookieChangedCallbackList =
      base::RepeatingCallbackList<void(const net::CookieChangeInfo&)>;
  using CookieChangedCallback = CookieChangedCallbackList::CallbackType;
  virtual base::CallbackListSubscription AddCookieChangedCallback(
      const GURL& url,
      const std::string* name,
      CookieChangedCallback callback) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_COOKIE_MANAGER_H_
