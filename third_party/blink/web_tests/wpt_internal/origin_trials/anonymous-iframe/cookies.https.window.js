// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=./resources/common.js

// A basic test checking partitioned cookies are enabled for anonymous iframe
// origin trial. This is a lightweight version of:
// wpt/html/anonymous-iframe/cookie.tentative.https.window.js

// Returns the cookies accessible from an iframe.
const cookiesFromIframe = (anonymous) => {
  const iframe_token = newIframe(window.origin, anonymous);
  const reply_token = token();
  send(iframe_token, `send("${reply_token}", document.cookie);`);
  return receive(reply_token);
}

promise_test(async test => {
  const parent_cookie = token() + "=" + token();
  await setCookie(window.origin, parent_cookie);

  // Sanity check. Before the OT, anonymous iframe behave like a normal iframe:
  assert_false('anonymouslyFramed' in window,
    "Anonymous iframe is disabled by default");
  assert_equals(await cookiesFromIframe(/*anonymous=*/true), parent_cookie,
    "OT disabled, anonymous iframe shares cookies");
  assert_equals(await cookiesFromIframe(/*anonymous=*/false), parent_cookie,
    "OT disabled, normal iframe shares cookies");

  // Verify the same-origin anonymous iframe do not share cookies:
  enableAnonymousIframeOriginTrial();
  assert_true('anonymouslyFramed' in window,
    "Anonymous iframe can be enabled using the OT token");
  assert_equals(await cookiesFromIframe(/*anonymous=*/true), "",
    "OT enabled, anonymous iframe do not shares cookies");
  assert_equals(await cookiesFromIframe(/*anonymous=*/false), parent_cookie,
    "OT enabled, normal iframe shares cookies");
});
