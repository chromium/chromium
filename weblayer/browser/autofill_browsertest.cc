// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

// Method that is passed to the autofill system to be invoked on detection of
// autofill forms.
// NOTE: This method can currently be invoked only once within the context of
// a given test. If that restriction ever needs to be relaxed, it could be
// done by changing |quit_closure| to a global that could be reset between
// expected invocations of the method.
void OnReceivedFormDataFromRenderer(base::OnceClosure quit_closure,
                                    autofill::FormData* output,
                                    const autofill::FormData& form) {
  ASSERT_TRUE(quit_closure);

  *output = form;
  std::move(quit_closure).Run();
}

}  // namespace

// Cross-platform tests of autofill parsing in the renderer and communication
// to the browser. Does not test integration with any platform's underlying
// system autofill mechanisms.
class AutofillBrowserTest : public WebLayerBrowserTest {
 public:
  AutofillBrowserTest() = default;

  AutofillBrowserTest(const AutofillBrowserTest&) = delete;
  AutofillBrowserTest& operator=(const AutofillBrowserTest&) = delete;

  ~AutofillBrowserTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    TabImpl::DisableAutofillSystemIntegrationForTesting();
#endif

    WebLayerBrowserTest::SetUp();
  }
};

// Tests that the renderer detects a password form and passes the appropriate
// data to the browser.
IN_PROC_BROWSER_TEST_F(AutofillBrowserTest, TestPasswordFormDetection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::RunLoop run_loop;
  autofill::FormData observed_form;

  InitializeAutofillWithEventForwarding(
      shell(), base::BindRepeating(&OnReceivedFormDataFromRenderer,
                                   run_loop.QuitClosure(), &observed_form));

  GURL password_form_url =
      embedded_test_server()->GetURL("/simple_password_form.html");
  NavigateAndWaitForCompletion(password_form_url, shell());

  // Focus the username field (note that a user gesture is necessary for
  // autofill to trigger) ...
  ExecuteScriptWithUserGesture(
      shell(), "document.getElementById('username_field').focus();");

  // ... and wait for the parsed data to be passed to the browser.
  run_loop.Run();

  // Verify that that the form data matches that of the document.
  EXPECT_EQ(u"testform", observed_form.name);
  EXPECT_EQ(password_form_url.spec(), observed_form.url);

  auto fields = observed_form.fields;
  EXPECT_EQ(2u, fields.size());
  autofill::FormFieldData username_field = fields[0];
  EXPECT_EQ(u"username_field", username_field.name);
  autofill::FormFieldData password_field = fields[1];
  EXPECT_EQ(u"password_field", password_field.name);
}

}  // namespace weblayer
