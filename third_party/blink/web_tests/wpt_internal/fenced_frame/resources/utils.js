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

  "document.referrer"                           : "00000000-0000-0000-0000-000000000001",
  "document.referrer ACK"                       : "00000000-0000-0000-0000-000000000002",

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

  "window.prompt"                               : "00000000-0000-0000-0000-000000000012",

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
  // Add keys above this list, incrementing the key UUID in hexadecimal
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
