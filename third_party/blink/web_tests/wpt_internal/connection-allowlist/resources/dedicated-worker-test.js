// META: script=/common/get-host-info.sub.js

const port = get_host_info().HTTPS_PORT_ELIDED;
const SUCCESS = true;
const FAILURE = false;

console.log(window.location.href);

// The worker content will attempt to fetch a URL and post the result back.
const worker_content = `
  onmessage = async (e) => {
    const url = e.data;
    try {
      const r = await fetch(url, { mode: 'cors', credentials: 'omit' });
      postMessage({ url: url, success: r.ok });
    } catch (err) {
      postMessage({ url: url, success: false, error: err.name });
    }
  };
`;
const dataUrl = 'data:text/javascript,' + encodeURIComponent(worker_content);

function worker_fetch_test(origin, expectation, description) {
  promise_test(async t => {
    const worker = new Worker(dataUrl, {type: 'module'});
    const fetch_url = `${origin}/common/blank-with-cors.html`;

    worker.postMessage(fetch_url);

    const msgEvent = await new Promise((resolve, reject) => {
      worker.onmessage = resolve;
      worker.onerror = (e) => reject(new Error('Worker Error'));
    });

    if (expectation === SUCCESS) {
      assert_true(msgEvent.data.success, `Fetch to ${origin} should succeed.`);
    } else {
      assert_false(
          msgEvent.data.success, `Fetch to ${origin} should be blocked.`);
    }
  }, description);
}

function worker_script_fetch_test(origin, expectation, description) {
  promise_test(async t => {
    const script_url = `${
        origin}/wpt_internal/connection-allowlist/resources/worker-fetch-script.js`;
    let worker;
    try {
      worker = new Worker(script_url);
    } catch (e) {
      assert_equals(
          expectation, FAILURE, 'Worker constructor threw unexpectedly');
      return;
    }

    const promise = new Promise((resolve, reject) => {
      worker.onmessage = () => resolve(SUCCESS);
      worker.onerror = (e) => {
        e.preventDefault();
        reject(new Error('Worker Load Error'));
      };
      // Send a message to the worker. If it loaded successfully, it will
      // respond and onmessage will fire. If it failed to load, onerror
      // should fire.
      worker.postMessage(
          `${get_host_info().HTTPS_ORIGIN}/common/blank-with-cors.html`);
    });

    if (expectation === SUCCESS) {
      const result = await promise;
      assert_equals(result, expectation, description);
    } else {
      await promise_rejects_js(t, Error, promise, description);
    }
  }, description);
}
