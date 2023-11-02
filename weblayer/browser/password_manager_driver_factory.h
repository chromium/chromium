// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PASSWORD_MANAGER_DRIVER_FACTORY_H_
#define WEBLAYER_BROWSER_PASSWORD_MANAGER_DRIVER_FACTORY_H_

#include <map>

#include "base/supports_user_data.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class WebContents;
}

namespace weblayer {

// WebLayer uses the system autofill and not the autofill used by Chrome. This
// factory and the corresponding driver are only used to listen for the
// notification that a password was typed into a form, since this is used as a
// signal to start isolating that site.
// TODO(crbug.com/1088446): Find a way to easily share this with Chrome.
class PasswordManagerDriverFactory
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PasswordManagerDriverFactory> {
 public:
  ~PasswordManagerDriverFactory() override;

  PasswordManagerDriverFactory(const PasswordManagerDriverFactory&) = delete;
  PasswordManagerDriverFactory& operator=(const PasswordManagerDriverFactory&) =
      delete;

  static void BindPasswordManagerDriver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
          pending_receiver,
      content::RenderFrameHost* render_frame_host);

 private:
  class PasswordManagerDriver;
  friend class content::WebContentsUserData<PasswordManagerDriverFactory>;

  explicit PasswordManagerDriverFactory(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  PasswordManagerDriver* GetDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  std::map<content::RenderFrameHost*, PasswordManagerDriver> frame_driver_map_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PASSWORD_MANAGER_DRIVER_FACTORY_H_
