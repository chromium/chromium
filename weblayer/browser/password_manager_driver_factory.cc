// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/password_manager_driver_factory.h"

#include "components/password_manager/content/browser/bad_message.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace weblayer {

// A minimal implementation of autofill::mojom::PasswordManagerDriver which just
// listens for the user to type into a password field.
class PasswordManagerDriverFactory::PasswordManagerDriver
    : public autofill::mojom::PasswordManagerDriver {
 public:
  explicit PasswordManagerDriver(content::RenderFrameHost* render_frame_host)
      : render_frame_host_(render_frame_host) {}

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
          pending_receiver) {
    password_manager_receiver_.Bind(std::move(pending_receiver));
  }

 private:
  // autofill::mojom::PasswordManagerDriver:
  // Note that these messages received from a potentially compromised renderer.
  // For that reason, any access to form data should be validated via
  // bad_message::CheckChildProcessSecurityPolicy.
  void PasswordFormsParsed(
      const std::vector<autofill::FormData>& forms_data) override {}
  void PasswordFormsRendered(
      const std::vector<autofill::FormData>& visible_forms_data,
      bool did_stop_loading) override {}
  void PasswordFormSubmitted(const autofill::FormData& form_data) override {}
  void InformAboutUserInput(const autofill::FormData& form_data) override {
    if (!password_manager::bad_message::CheckChildProcessSecurityPolicyForURL(
            render_frame_host_, form_data.url,
            password_manager::BadMessageReason::
                CPMD_BAD_ORIGIN_UPON_USER_INPUT_CHANGE)) {
      return;
    }

    if (FormHasNonEmptyPasswordField(form_data) &&
        site_isolation::SiteIsolationPolicy::
            IsIsolationForPasswordSitesEnabled()) {
      // This function signals that a password field has been filled (whether by
      // the user, JS, autofill, or some other means) or a password form has
      // been submitted. Use this as a heuristic to start site-isolating the
      // form's site. This is intended to be used primarily when full site
      // isolation is not used, such as on Android.
      content::SiteInstance::StartIsolatingSite(
          render_frame_host_->GetSiteInstance()->GetBrowserContext(),
          form_data.url);
    }
  }
  void SameDocumentNavigation(autofill::mojom::SubmissionIndicatorEvent
                                  submission_indication_event) override {}
  void RecordSavePasswordProgress(const std::string& log) override {}
  void UserModifiedPasswordField() override {}
  void UserModifiedNonPasswordField(autofill::FieldRendererId renderer_id,
                                    const base::string16& value) override {}
  void ShowPasswordSuggestions(base::i18n::TextDirection text_direction,
                               const base::string16& typed_username,
                               int options,
                               const gfx::RectF& bounds) override {}
  void ShowTouchToFill() override {}
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override {}
  void FocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type) override {}
  void LogFirstFillingResult(autofill::FormRendererId form_renderer_id,
                             int32_t result) override {}

  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver>
      password_manager_receiver_{this};
  content::RenderFrameHost* render_frame_host_;
};

PasswordManagerDriverFactory::PasswordManagerDriverFactory(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PasswordManagerDriverFactory::~PasswordManagerDriverFactory() = default;

// static
void PasswordManagerDriverFactory::BindPasswordManagerDriver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  PasswordManagerDriverFactory* factory =
      PasswordManagerDriverFactory::FromWebContents(web_contents);
  if (!factory)
    return;

  factory->GetDriverForFrame(render_frame_host)
      ->BindPendingReceiver(std::move(pending_receiver));
}

PasswordManagerDriverFactory::PasswordManagerDriver*
PasswordManagerDriverFactory::GetDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_EQ(web_contents(),
            content::WebContents::FromRenderFrameHost(render_frame_host));
  DCHECK(render_frame_host->IsRenderFrameCreated());

  // TryEmplace() will return an iterator to the driver corresponding to
  // `render_frame_host`. It creates a new one if required.
  return &base::TryEmplace(frame_driver_map_, render_frame_host,
                           render_frame_host)
              .first->second;
}

void PasswordManagerDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordManagerDriverFactory)

}  // namespace weblayer
