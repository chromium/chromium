// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=./resources/common.js

// A basic test checking partitioned cookies are enabled for anonymous iframe
// origin trial.

const enableAnonymousIframeOriginTrial = () => {
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  // Generated using:
  // ./tools/origin_trials/generate_token.py \
  //  --expire-days 5000 \
  //  --version 3  \
  //  https://www.web-platform.test:8444/ \
  //  AnonymousIframe
  meta.content = 'AwdHTTICbNy8uTXRBoXUyIR2BqCZTs2wEYHEChRfeyzgFI06chb5ud7lfDB3it3gFS5X9z4H/vxF0M58xWmLfwMAAABgeyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQW5vbnltb3VzSWZyYW1lIiwgImV4cGlyeSI6IDIwODMzMTQ1MTR9';
  document.getElementsByTagName('head')[0].appendChild(meta);
};

const executor_path = '/common/dispatcher/executor.html?pipe=';

// Add a |cookie| on an |origin|.
// Note: cookies visibility depends on the path of the document. Those are set
// from a document from: /common/dispatcher/. So the cookie is visible to every
// path underneath.
const setCookie = async (origin, cookie) => {
  const popup_token = token();
  const popup_url = origin + executor_path + `&uuid=${popup_token}`;
  const popup = window.open(popup_url);

  const reply_token = token();
  send(popup_token, `
    document.cookie = "${cookie}";
    send("${reply_token}", "done");
  `);
  assert_equals(await receive(reply_token), "done");
  popup.close();
}

// Returns the cookies accessible from an iframe.
const cookiesFromIframe = (anonymous) => {
  const iframe_token = token();
  const iframe = document.createElement('iframe');
  iframe.src = window.origin + executor_path + `&uuid=${iframe_token}`;
  iframe.anonymous = anonymous;
  document.body.appendChild(iframe);
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
