// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"

namespace weblayer {

class FaviconFetcherDelegate;

// FaviconTabHelper is responsible for creating favicon::ContentFaviconDriver
// when necessary. FaviconTabHelper is used by FaviconFetcherImpl and notifies
// FaviconFetcherDelegate when the favicon changes.
class FaviconTabHelper : public content::WebContentsUserData<FaviconTabHelper>,
                         public content::WebContentsObserver,
                         public favicon::FaviconDriverObserver {
 public:
  // Used to track calls to RegisterFaviconFetcherDelegate(). When destroyed
  // the FaviconFetcherDelegate is removed.
  class ObserverSubscription {
   public:
    ObserverSubscription(const ObserverSubscription&) = delete;
    ObserverSubscription& operator=(const ObserverSubscription&) = delete;
    ~ObserverSubscription();

   private:
    friend class FaviconTabHelper;

    ObserverSubscription(FaviconTabHelper* helper,
                         FaviconFetcherDelegate* delegate);

    raw_ptr<FaviconTabHelper> helper_;
    raw_ptr<FaviconFetcherDelegate> delegate_;
  };

  FaviconTabHelper(const FaviconTabHelper&) = delete;
  FaviconTabHelper& operator=(const FaviconTabHelper&) = delete;
  ~FaviconTabHelper() override;

  // Called when FaviconFetcherImpl is created. This ensures the necessary
  // wiring is in place and notifies |delegate| when the favicon changes.
  std::unique_ptr<ObserverSubscription> RegisterFaviconFetcherDelegate(
      FaviconFetcherDelegate* delegate);

  // Returns the favicon for the current navigation.
  const gfx::Image& favicon() const { return favicon_; }

 private:
  friend class content::WebContentsUserData<FaviconTabHelper>;

  explicit FaviconTabHelper(content::WebContents* contents);

  void AddDelegate(FaviconFetcherDelegate* delegate);
  void RemoveDelegate(FaviconFetcherDelegate* delegate);

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  raw_ptr<content::WebContents> web_contents_;
  // Number of observers attached.
  int observer_count_ = 0;
  base::ObserverList<FaviconFetcherDelegate> delegates_;
  gfx::Image favicon_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
