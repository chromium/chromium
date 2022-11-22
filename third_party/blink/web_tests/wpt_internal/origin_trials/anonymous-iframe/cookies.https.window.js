// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=./resources/common.js

// A basic test checking partitioned cookies are enabled for credentialless
// iframe origin trial. This is a lightweight version of:
// wpt/html/anonymous-iframe/cookie.tentative.https.window.js

// Returns the cookies accessible from an iframe.
const cookiesFromIframe = (credentialless) => {
  const iframe_token = newIframe(window.origin, credentialless);
  const reply_token = token();
  send(iframe_token, `send("${reply_token}", document.cookie);`);
  return receive(reply_token);
}

promise_test(async test => {
  const parent_cookie = token() + "=" + token();
  await setCookie(window.origin, parent_cookie);

  // Sanity check. Before the OT, credentialless iframe behave like a normal iframe:
  assert_false('credentialless' in window,
    "credentialless iframe is disabled by default");
  assert_equals(await cookiesFromIframe(/*credentialless=*/true), parent_cookie,
    "OT disabled, credentialless iframe shares cookies");
  assert_equals(await cookiesFromIframe(/*credentialless=*/false), parent_cookie,
    "OT disabled, normal iframe shares cookies");

  // Verify the same-origin credentialless iframe do not share cookies:
  enableIframeCredentiallessOriginTrial();
  assert_true('credentialless' in window,
    "credentialless iframe can be enabled using the OT token");
  assert_equals(await cookiesFromIframe(/*credentialless=*/true), "",
    "OT enabled, credentialless iframe do not shares cookies");
  assert_equals(await cookiesFromIframe(/*credentialless=*/false), parent_cookie,
    "OT enabled, normal iframe shares cookies");
});
