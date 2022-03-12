const STORE_URL = '/wpt_internal/fenced_frame/resources/key-value-store.py';
const REMOTE_EXECUTOR_URL = '/wpt_internal/fenced_frame/resources/remote-context-executor.https.html';

// Creates a URL that includes a list of stash key UUIDs that are being used
// in the test. This allows us to generate UUIDs on the fly and let anything
// (iframes, fenced frames, pop-ups, etc...) that wouldn't have access to the
// original UUID variable know what the UUIDs are.
// @param {string} href - The base url of the page being navigated to
// @param {string list} keylist - The list of key UUIDs to be used. Note that
//                                order matters when extracting the keys
function generateURL(href, keylist) {
  const ret_url = new URL(href, location.href);
  ret_url.searchParams.append("keylist", keylist.join(','));
  return ret_url;
}

// Extracts a list of UUIDs from the from the current page's URL.
// @returns {string list} - The list of UUIDs extracted from the page. This can
//                          be read into multiple variables using the
//                          [key1, key2, etc...] = parseKeyList(); pattern.
function parseKeylist() {
  const url = new URL(location.href);
  const keylist = url.searchParams.get("keylist");
  return keylist.split(',');
}

// Converts a same-origin URL to a cross-origin URL
// @param {URL} url - The URL object whose origin is being converted
// @param {boolean} [https=true] - Whether or not to use the HTTPS origin
//
// @returns {URL} The new cross-origin URL
function getRemoteOriginURL(url, https=true) {
  const same_origin = location.origin;
  const cross_origin = https ? get_host_info().HTTPS_REMOTE_ORIGIN
      : get_host_info().HTTP_REMOTE_ORIGIN;
  return new URL(url.toString().replace(same_origin, cross_origin));
}

// Attaches a frame that waits for scripts to execute from RemoteContext.
// Returns a proxy for the frame that first resolves to the frame HTML element,
// then resolves to the RemoteContext if the property isn't found.
// The proxy also has an extra attribute `execute`, which is an alias for the
// remote context's `execute_script(fn, args=[])`.
function attachFrameContext(element_name, html, headers, attributes) {

  // Create the frame, passing the unique id for the parent/child channel.
  const frame = document.createElement(element_name);
  const uuid = token();

  // Use the absolute path of the remote context executor source file, so that
  // nested frames will work.
  const url = new URL(REMOTE_EXECUTOR_URL, location.origin);
  url.searchParams.append('uuid', uuid);

  // Add the header to allow loading in a fenced frame.
  headers.push(["Supports-Loading-Mode", "fenced-frame"]);

  // Transform the headers into the expected format.
  // https://web-platform-tests.org/writing-tests/server-pipes.html#headers
  const formatted_headers = headers.map((header) => {
    return `header(${header[0]}, ${header[1]})`;
  });
  url.searchParams.append('pipe', formatted_headers.join("|"));

  attributes.forEach(attribute => {
    frame.setAttribute(attribute[0], attribute[1]);
  });

  frame.src = url;
  document.body.append(frame);

  // https://github.com/web-platform-tests/wpt/blob/master/common/dispatcher/README.md
  const context = new RemoteContext(uuid);
  if (html) {
    context.execute_script(
      (html_source) => {
        document.body.insertAdjacentHTML('beforebegin', html_source);
      },
    [html]);
  }

  // We need a little bit of boilerplate in the handlers because Proxy doesn't
  // work so nicely with HTML elements.
  const handler = {
    get: (target, key) => {
      if (key == "execute") {
        return context.execute_script;
      }
      if (key == "element") {
        return frame;
      }
      if (key in target) {
        return target[key];
      }
      return context[key];
    },
    set: (target, key, value) => {
      target[key] = value;
      return value;
    }
  };

  const proxy = new Proxy(frame, handler);
  return proxy;
}

// Attach a fenced frame that waits for scripts to execute.
// Takes as input a(n optional) dictionary of configs:
// - html: extra HTML source code to inject into the loaded frame
// - headers: an array of header pairs [[key, value], ...]
// - attributes: an array of attribute pairs to set on the frame [[key, value], ...]
// Returns a proxy that acts like the frame HTML element, but with an extra
// function `execute`. See `attachFrameContext` or the README for more details.
function attachFencedFrameContext({html = "", headers=[], attributes=[]} = {}) {
  return attachFrameContext('fencedframe', html, headers, attributes);
}

// Attach an iframe that waits for scripts to execute.
// See `attachFencedFrameContext` for more details.
function attachIFrameContext({html = "", headers=[], attributes=[]} = {}) {
  return attachFrameContext('iframe', html, headers, attributes);
}

// Converts a key string into a key uuid using a cryptographic hash function.
// This function only works in secure contexts (HTTPS).
async function stringToStashKey(string) {
  // Compute a SHA-256 hash of the input string, and convert it to hex.
  const data = new TextEncoder().encode(string);
  const digest = await crypto.subtle.digest('SHA-256', data);
  const digest_array = Array.from(new Uint8Array(digest));
  const digest_as_hex = digest_array.map(b => b.toString(16).padStart(2, '0')).join('');

  // UUIDs are structured as 8X-4X-4X-4X-12X.
  // Use the first 32 hex digits and ignore the rest.
  const digest_slices = [digest_as_hex.slice(0,8),
                         digest_as_hex.slice(8,12),
                         digest_as_hex.slice(12,16),
                         digest_as_hex.slice(16,20),
                         digest_as_hex.slice(20,32)];
  return digest_slices.join('-');
}

function attachFencedFrame(url) {
  assert_implements(
      window.HTMLFencedFrameElement,
      'The HTMLFencedFrameElement should be exposed on the window object');

  const fenced_frame = document.createElement('fencedframe');
  fenced_frame.src = url;
  document.body.append(fenced_frame);
  return fenced_frame;
}

function attachIFrame(url) {
  const iframe = document.createElement('iframe');
  iframe.src = url;
  document.body.append(iframe);
  return iframe;
}

// Reads the value specified by `key` from the key-value store on the server.
async function readValueFromServer(key) {
  // Resolve the key if it is a Promise.
  key = await key;

  const serverUrl = `${STORE_URL}?key=${key}`;
  const response = await fetch(serverUrl);
  if (!response.ok)
    throw new Error('An error happened in the server');
  const value = await response.text();

  // The value is not stored in the server.
  if (value === "<Not set>")
    return { status: false };

  return { status: true, value: value };
}

// Convenience wrapper around the above getter that will wait until a value is
// available on the server.
async function nextValueFromServer(key) {
  // Resolve the key if it is a Promise.
  key = await key;

  while (true) {
    // Fetches the test result from the server.
    const { status, value } = await readValueFromServer(key);
    if (!status) {
      // The test result has not been stored yet. Retry after a while.
      await new Promise(resolve => setTimeout(resolve, 20));
      continue;
    }

    return value;
  }
}

// Writes `value` for `key` in the key-value store on the server.
async function writeValueToServer(key, value, origin = '') {
  // Resolve the key if it is a Promise.
  key = await key;

  const serverUrl = `${origin}${STORE_URL}?key=${key}&value=${value}`;
  await fetch(serverUrl, {"mode": "no-cors"});
}

// Simulates a user gesture.
async function simulateGesture() {
  // Wait until the window size is initialized.
  while (window.innerWidth == 0) {
    await new Promise(resolve => requestAnimationFrame(resolve));
  }
  await test_driver.bless('simulate gesture');
}
