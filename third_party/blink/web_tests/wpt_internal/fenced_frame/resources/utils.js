const STORE_URL = '/wpt_internal/fenced_frame/resources/key-value-store.py';

// This is a dictionary of stash keys to access a specific piece of the
// server-side stash. In order to communicate between browsing contexts that
// cannot otherwise talk, the two browsing contexts (the producer and consumer)
// must use the same key, which is impossible to obtain as you normally would
// via the common API's token() method (which returns a UUID). Therefore in this
// file, for each piece of data we're interested in communicating between the
// fenced frame's embedder and the fenced frame itself, we have to fix a key so
// that both frames can reference it. We need a separate stash key for each
// test that passes data, since multiple tests can run in parallel and would
// otherwise interfere with each other's server state.
const KEYS = {
  // This key is only used to test that the server-side stash works properly.
  "dummy"                                       : "00000000-0000-0000-0000-000000000000",

  "window.top"                                  : "00000000-0000-0000-0000-000000000003",
  "window.top ACK"                              : "00000000-0000-0000-0000-000000000004",

  "window.parent"                               : "00000000-0000-0000-0000-000000000005",
  "window.parent ACK"                           : "00000000-0000-0000-0000-000000000006",

  "location.ancestorOrigins"                    : "00000000-0000-0000-0000-000000000007",
  "location.ancestorOrigins ACK"                : "00000000-0000-0000-0000-000000000008",

  "data: URL"                                   : "00000000-0000-0000-0000-000000000009",
  "204 response"                                : "00000000-0000-0000-0000-00000000000A",

  "keyboard.lock"                               : "00000000-0000-0000-0000-00000000000B",

  "credentials.create"                          : "00000000-0000-0000-0000-00000000000C",
  "credentials.create ACK"                      : "00000000-0000-0000-0000-00000000000D",

  "keyboard.lock"                               : "00000000-0000-0000-0000-00000000000E",

  "navigation_success"                          : "00000000-0000-0000-0000-00000000000F",
  "ready_for_navigation"                        : "00000000-0000-0000-0000-000000000010",

  "secFetchDest.value"                          : "00000000-0000-0000-0000-000000000011",

  "fenced_navigation_complete"                  : "00000000-0000-0000-0000-000000000013",
  "outer_page_ready_for_next_fenced_navigation" : "00000000-0000-0000-0000-000000000014",

  "focus-changed"                               : "00000000-0000-0000-0000-000000000015",
  "focus-ready"                                 : "00000000-0000-0000-0000-000000000016",

  "navigate_ancestor"                           : "00000000-0000-0000-0000-000000000017",
  "navigate_ancestor_from_nested"               : "00000000-0000-0000-0000-000000000018",

  "window.frameElement"                         : "00000000-0000-0000-0000-000000000019",

  "keyboard.getLayoutMap"                       : "00000000-0000-0000-0000-00000000001A",

  "permission.notification"                     : "00000000-0000-0000-0000-00000000001B",

  "serviceWorker.frameType"                     : "00000000-0000-0000-0000-00000000001C",
  "serviceWorker.frameType ACK"                 : "00000000-0000-0000-0000-00000000001D",

  "popup_noopener"                              : "00000000-0000-0000-0000-00000000001E",
  "popup_openee"                                : "00000000-0000-0000-0000-00000000001F",

  "permission.geolocation"                      : "00000000-0000-0000-0000-000000000020",

  "presentation.receiver"                       : "00000000-0000-0000-0000-000000000021",

  "background-sync"                             : "00000000-0000-0000-0000-000000000022",

  "prerender READY"                             : "00000000-0000-0000-0000-000000000023",
  "prerender LOADED"                            : "00000000-0000-0000-0000-000000000024",
  "prerender ACTIVATED"                         : "00000000-0000-0000-0000-000000000025",

  "pointer-lock"                                : "00000000-0000-0000-0000-000000000026",

  "referrer.value"                              : "00000000-0000-0000-0000-000000000027",
  "referrer.value ACK"                          : "00000000-0000-0000-0000-000000000028",

  "bluetooth.requestDevice"                     : "00000000-0000-0000-0000-000000000029",

  "usb.requestDevice"                           : "00000000-0000-0000-0000-00000000002A",

  "navigator.share"                             : "00000000-0000-0000-0000-00000000002B",

  "background-fetch"                            : "00000000-0000-0000-0000-00000000002C",

  "window.outersize"                            : "00000000-0000-0000-0000-00000000002D",
  "window.innersize"                            : "00000000-0000-0000-0000-00000000002E",

  "fenced_history_length"                       : "00000000-0000-0000-0000-00000000002F",
  "outer_page_ready_for_next_navigation"        : "00000000-0000-0000-0000-000000000030",

  "embed_coep_require_corp"                     : "00000000-0000-0000-0000-000000000031",
  "embed_no_coep"                               : "00000000-0000-0000-0000-000000000032",

  "hid.getDevice"                               : "00000000-0000-0000-0000-000000000033",

  "ndef.write"                                  : "00000000-0000-0000-0000-000000000034",
  "ndef.scan"                                   : "00000000-0000-0000-0000-000000000035",

  "history_navigation_performed"                : "00000000-0000-0000-0000-000000000036",
  "outer_page_ready"                            : "00000000-0000-0000-0000-000000000037",

  "resize_lock_inner_page_is_ready"             : "00000000-0000-0000-0000-000000000038",
  "resize_lock_resize_is_done"                  : "00000000-0000-0000-0000-000000000039",
  "resize_lock_report_inner_dimensions"         : "00000000-0000-0000-0000-00000000004A",

  "csp"                                         : "00000000-0000-0000-0000-00000000004B",

  "cookie_value"                                : "00000000-0000-0000-0000-00000000004C",

  "csp-fenced-frame-src-blocked"                : "00000000-0000-0000-0000-00000000004D",
  "csp-fenced-frame-src-allowed"                : "00000000-0000-0000-0000-00000000004E",
  "csp-frame-src-blocked"                       : "00000000-0000-0000-0000-00000000004F",
  "csp-frame-src-allowed"                       : "00000000-0000-0000-0000-000000000050",

  "frame_navigation"                            : "00000000-0000-0000-0000-000000000051",
  "frame_navigation ACK"                        : "00000000-0000-0000-0000-000000000052",

  "maxframes_response"                          : "00000000-0000-0000-0000-000000000053",
  // Add keys above this list, incrementing the key UUID in hexadecimal
}

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
  return ret_url.toString().replace(/%2C/g,",");
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

// Simulates a user gesture and calls `callback` when `mouseup` happens.
function simulateGesture(callback) {
  // Get or create the target element.
  let target = document.getElementById('target');
  if (!target) {
    target = document.createElement('button');
    target.textContent = '\u2573';
    target.id = 'target';
    document.body.appendChild(target);
  }
  target.addEventListener('mouseup', callback);

  requestAnimationFrame(() => {
    if (eventSender) {
      eventSender.mouseMoveTo(target.getBoundingClientRect().x,
                              target.getBoundingClientRect().y);
      eventSender.mouseDown();
      eventSender.mouseUp();
    }
  });
}
