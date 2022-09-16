// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/password_manager_driver_factory.h"

#include "base/memory/raw_ptr.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/form_meta_data.h"
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
      const std::vector<autofill::FormData>& raw_forms_data) override {}
  void PasswordFormsRendered(
      const std::vector<autofill::FormData>& raw_visible_forms_data) override {}
  void PasswordFormSubmitted(const autofill::FormData& raw_form_data) override {
  }
  void InformAboutUserInput(const autofill::FormData& raw_form_data) override {
    autofill::FormData form_data =
        password_manager::GetFormWithFrameAndFormMetaData(render_frame_host_,
                                                          raw_form_data);
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
          form_data.url,
          content::ChildProcessSecurityPolicy::IsolatedOriginSource::
              USER_TRIGGERED);
    }
  }
  void DynamicFormSubmission(autofill::mojom::SubmissionIndicatorEvent
                                 submission_indication_event) override {}
  void PasswordFormCleared(const autofill::FormData& raw_form_data) override {}
  void RecordSavePasswordProgress(const std::string& log) override {}
  void UserModifiedPasswordField() override {}
  void UserModifiedNonPasswordField(autofill::FieldRendererId renderer_id,
                                    const std::u16string& field_name,
                                    const std::u16string& value) override {}
  void ShowPasswordSuggestions(base::i18n::TextDirection text_direction,
                               const std::u16string& typed_username,
                               int options,
                               const gfx::RectF& bounds) override {}

#if BUILDFLAG(IS_ANDROID)
  void ShowTouchToFill(
      autofill::mojom::SubmissionReadinessState submission_readiness) override {
  }
#endif

  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override {}
  void FocusedInputChanged(
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override {}
  void LogFirstFillingResult(autofill::FormRendererId form_renderer_id,
                             int32_t result) override {}

  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver>
      password_manager_receiver_{this};
  raw_ptr<content::RenderFrameHost> render_frame_host_;
};

PasswordManagerDriverFactory::PasswordManagerDriverFactory(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PasswordManagerDriverFactory>(
          *web_contents) {}

PasswordManagerDriverFactory::~PasswordManagerDriverFactory() = default;

// static
void PasswordManagerDriverFactory::BindPasswordManagerDriver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  // TODO(https://crbug.com/1233858): Similarly to the
  // ContentPasswordManagerDriver implementation. Do not bind the interface when
  // the RenderFrameHost is in an anonymous iframe.
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
  DCHECK(render_frame_host->IsRenderFrameLive());

  auto [it, inserted] =
      frame_driver_map_.try_emplace(render_frame_host, render_frame_host);
  return &it->second;
}

void PasswordManagerDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordManagerDriverFactory);

}  // namespace weblayer
