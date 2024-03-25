// This is loaded as a Worker in a WPT. When postMessaged to, this attempts to
// forward whatever was sent to the `key-value-store.py` endpoint via a fetch()
// API call. If the fetch() fails, it postMessages back to the WPT alerting it
// to the failure.
const STORE_URL = '/wpt_internal/fenced_frame/resources/key-value-store.py';

// This is functionally the same as writeValueToServer() in utils.js.
this.onmessage = async function(e) {
  const serverUrl = `${STORE_URL}?key=${e.data[0]}&value=${e.data[1]}`;
  try {
    await fetch(serverUrl, {"mode": "no-cors"});
  } catch (error) {
    this.postMessage("fetch failed");
  }
}
