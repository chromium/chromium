// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_TAB_H_
#define WEBLAYER_PUBLIC_TAB_H_

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "build/build_config.h"

namespace base {
class Value;
}

#if !BUILDFLAG(IS_ANDROID)
namespace views {
class WebView;
}
#endif

namespace weblayer {
class Browser;
class ErrorPageDelegate;
class FaviconFetcher;
class FaviconFetcherDelegate;
class FullscreenDelegate;
class GoogleAccountsDelegate;
class NavigationController;
class NewTabDelegate;
class TabObserver;
class WebMessageHostFactory;

// Represents a tab that is navigable.
class Tab {
 public:
  virtual ~Tab() = default;

  // Returns the Browser that owns this.
  virtual Browser* GetBrowser() = 0;

  // Sets the ErrorPageDelegate. If none is set, a default action will be taken
  // for any given interaction with an error page.
  virtual void SetErrorPageDelegate(ErrorPageDelegate* delegate) = 0;

  // Sets the FullscreenDelegate. Setting a non-null value implicitly enables
  // fullscreen.
  virtual void SetFullscreenDelegate(FullscreenDelegate* delegate) = 0;

  // Sets the NewBrowserDelegate. Setting a null value implicitly disables
  // popups.
  virtual void SetNewTabDelegate(NewTabDelegate* delegate) = 0;

  virtual void SetGoogleAccountsDelegate(GoogleAccountsDelegate* delegate) = 0;

  virtual void AddObserver(TabObserver* observer) = 0;

  virtual void RemoveObserver(TabObserver* observer) = 0;

  virtual NavigationController* GetNavigationController() = 0;

  using JavaScriptResultCallback = base::OnceCallback<void(base::Value)>;

  // Executes the script, and returns the result to the callback if provided. If
  // |use_separate_isolate| is true, runs the script in a separate v8 Isolate.
  // This uses more memory, but separates the injected scrips from scripts in
  // the page. This prevents any potentially malicious interaction between
  // first-party scripts in the page, and injected scripts. Use with caution,
  // only pass false for this argument if you know this isn't an issue or you
  // need to interact with first-party scripts.
  virtual void ExecuteScript(const std::u16string& script,
                             bool use_separate_isolate,
                             JavaScriptResultCallback callback) = 0;

  // Returns the tab's guid.
  virtual const std::string& GetGuid() = 0;

  // Allows the embedder to get and set arbitrary data on the tab. This will be
  // saved and restored with the browser, so it is important to keep this data
  // as small as possible.
  virtual void SetData(const std::map<std::string, std::string>& data) = 0;
  virtual const std::map<std::string, std::string>& GetData() = 0;

  // Adds a new WebMessageHostFactory. For any urls that match
  // |allowed_origin_rules| a JS object is registered using the name
  // |js_object_name| (in the global namespace). Script may use the object to
  // send and receive messages and is available at page load time.
  //
  // The page is responsible for initiating the connection. That is,
  // WebMessageHostFactory::CreateHost() is called once the page posts a
  // message to the JS object.
  //
  // |allowed_origin_rules| is a set of rules used to determine which pages
  // this applies to. '*' may be used to match anything. If not '*' the format
  // is 'scheme://host:port':
  // . scheme: The scheme, which can not be empty or contain '*'.
  // . host: The host to match against. Can not contain '/' and may start with
  //   '*.' to match against a specific subdomain.
  // . port (optional): matches a specific port.
  //
  // Returns an empty string on success. On failure, the return string gives
  // an error message.
  virtual std::u16string AddWebMessageHostFactory(
      std::unique_ptr<WebMessageHostFactory> factory,
      const std::u16string& js_object_name,
      const std::vector<std::string>& allowed_origin_rules) = 0;

  // Removes the WebMessageHostFactory registered under |js_object_name|.
  virtual void RemoveWebMessageHostFactory(
      const std::u16string& js_object_name) = 0;

  // Creates a FaviconFetcher that notifies a FaviconFetcherDelegate when
  // the favicon changes.
  // A page may provide any number of favicons. The preferred image size
  // used depends upon the platform. If a previously cached icon is available,
  // it is used, otherwise the icon is downloaded.
  // |delegate| may be called multiple times for the same navigation. This
  // happens when the page dynamically updates the favicon, but may also happen
  // if a cached icon is determined to be out of date.
  virtual std::unique_ptr<FaviconFetcher> CreateFaviconFetcher(
      FaviconFetcherDelegate* delegate) = 0;

  // Sets the target language for translation such that whenever the translate
  // UI shows in this Tab, the target language will be |targetLanguage|. Notes:
  // - |targetLanguage| should be specified as the language code (e.g., "de" for
  //   German).
  // - Passing an empty string causes behavior to revert to default.
  // - Specifying a non-empty target language will also result in the following
  //   behaviors (all of which are intentional as part of the semantics of
  //   having a target language):
  //   - Translation is initiated automatically (note that the infobar UI is
  //      present)
  //   - Translation occurs even for languages/sites that the user has
  //     blocklisted
  //   - Translation occurs even for pages in the user's default locale
  //   - Translation does *not* occur nor is the infobar UI shown for pages in
  //     the specified target language
  virtual void SetTranslateTargetLanguage(
      const std::string& translate_target_lang) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // TODO: this isn't a stable API, so use it now for expediency in the C++ API,
  // but if we ever want to have backward or forward compatibility in C++ this
  // will have to be something else.
  virtual void AttachToView(views::WebView* web_view) = 0;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_TAB_H_
