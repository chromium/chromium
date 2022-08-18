const executor_path = '/common/dispatcher/executor.html?pipe=';

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

let parseCookies = function(headers_json) {
  if (!headers_json["cookie"])
    return {};

  return headers_json["cookie"]
    .split(';')
    .map(v => v.split('='))
    .reduce((acc, v) => {
      acc[v[0].trim()] = v[1].trim();
      return acc;
    }, {});
}

// Create an iframe, return the token to communicate with it.
const newIframe = (origin, anonymous) => {
  const iframe_token = token();
  const iframe = document.createElement('iframe');
  iframe.src = origin + executor_path + `&uuid=${iframe_token}`;
  iframe.anonymous = anonymous;
  document.body.appendChild(iframe);
  return iframe_token;
}
