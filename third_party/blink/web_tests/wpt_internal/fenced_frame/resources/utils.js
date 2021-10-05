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
  "dummy"                        : "00000000-0000-0000-0000-000000000000",

  "document.referrer"            : "00000000-0000-0000-0000-000000000001",
  "document.referrer ACK"        : "00000000-0000-0000-0000-000000000002",

  "window.top"                   : "00000000-0000-0000-0000-000000000003",
  "window.top ACK"               : "00000000-0000-0000-0000-000000000004",

  "window.parent"                : "00000000-0000-0000-0000-000000000005",
  "window.parent ACK"            : "00000000-0000-0000-0000-000000000006",

  "location.ancestorOrigins"     : "00000000-0000-0000-0000-000000000007",
  "location.ancestorOrigins ACK" : "00000000-0000-0000-0000-000000000008",
  // Add keys below this list, incrementing the key UUID in hexadecimal:
}

function attachFencedFrame(url) {
  assert_not_equals(window.HTMLFencedFrameElement, undefined,
                    "The HTMLFencedFrameElement should be exposed on the " +
                    "window object");

  const fenced_frame = document.createElement('fencedframe');
  fenced_frame.src = url;
  document.body.append(fenced_frame);
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
async function writeValueToServer(key, value) {
  const serverUrl = `${STORE_URL}?key=${key}&value=${value}`;
  await fetch(serverUrl);
}
