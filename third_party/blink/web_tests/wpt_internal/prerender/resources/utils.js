const STORE_URL = '/wpt_internal/prerender/resources/key_value_store.py';

// Starts prerendering for `url`.
function startPrerendering(url) {
  // Adds <link rel=prerender> for the URL.
  // TODO(https://crbug.com/1174978): <link rel=prerender> may not start
  // prerendering for some reason (e.g., resource limit). Implement a WebDriver
  // API to force prerendering.
  const link = document.createElement('link');
  link.rel = 'prerender';
  link.href = url;
  document.head.appendChild(link);
}

// Reads the value specified by `key` from the key-value store on the server.
async function readValueFromServer(key) {
  const serverUrl = `${STORE_URL}?key=${key}`;
  const response = await fetch(serverUrl);
  if (!response.ok)
    throw new Error('An error happened in the server');
  const value = await response.text();

  // The value is not stored in the server.
  if (value === "")
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
      await new Promise(resolve => setTimeout(resolve, 100));
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
