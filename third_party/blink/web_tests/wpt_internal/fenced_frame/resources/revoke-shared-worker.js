// This is loaded as a SharedWorker in a WPT. When postMessaged to, this
// attempts to forward whatever was sent to the `key-value-store.py` endpoint
// via a fetch() API call. If the fetch() fails, it postMessages back to the WPT
// alerting it to the failure.
const STORE_URL = '/wpt_internal/fenced_frame/resources/key-value-store.py';

onconnect = function (event) {
  const port = event.ports[0];
  // This is functionally the same as writeValueToServer() in utils.js.
  port.onmessage = async function(e) {
    const serverUrl = `${STORE_URL}?key=${e.data[0]}&value=${e.data[1]}`;
    try {
      await fetch(serverUrl, {"mode": "no-cors"});
    } catch (error) {
      port.postMessage("fetch failed");
    }
  }
}