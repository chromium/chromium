// META: script=/common/get-host-info.sub.js
//
// The following tests assume that no Connection-Allowlist or Origin-Trial
// header is set for the top-level document. These tests fetch a dedicated
// worker script which provides its own Connection-Allowlist header and
// Origin-Trial token.

const port = get_host_info().HTTPS_PORT_ELIDED;
const SUCCESS = true;
const FAILURE = false;

const ORIGIN_TRIAL_ENABLED = true;
const ORIGIN_TRIAL_DISABLED = false;

function worker_declares_origin_trial_test(fetch_origin, ot_enabled, expectation, description) {
  promise_test(async (t) => {
    const script_to_fetch = ot_enabled
      ? '/wpt_internal/connection-allowlist/resources/worker-fetch-script-origin-trial.js?pipe=header(Origin-Trial,AwntOHpOLwZVFop2dRTwojIi9nXAJnr9/t34mh87jyve9oS35s0tq6gIyp4H3cwMlKntLUR8GyDfA0HWjVlobgwAAABkeyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQ29ubmVjdGlvbkFsbG93bGlzdCIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==,True)'
      : '/wpt_internal/connection-allowlist/resources/worker-fetch-script-origin-trial.js';
    // The origin of our top-level document is HTTPS_ORIGIN, so ensure the
    // dedicated worker has the same origin.
    const script_url = `${get_host_info().HTTPS_ORIGIN}${script_to_fetch}`;

    let worker;
    try {
      worker = new Worker(script_url);
    } catch (e) {
      assert_unreached('Creating a same-origin dedicated worker should always succeed.');
    }

    const promise = new Promise((resolve, reject) => {
      worker.onmessage = (e) => {
        resolve(e.data.success);
      }
      worker.onerror = (e) => {
        e.preventDefault();
        reject(new Error('Worker Load Error'));
      };
      // Send a message to the worker. It will attempt to fetch the URL and post
      // a success/failure result in a response message.
      worker.postMessage(
          `${fetch_origin}/common/blank-with-cors.html`);
    });

    const fetch_result = await promise;
    assert_equals(fetch_result, expectation,
                  'Fetch result should match expectation.')
  }, description);
}

worker_declares_origin_trial_test(
  get_host_info().HTTPS_ORIGIN, ORIGIN_TRIAL_DISABLED, SUCCESS,
  'Same-origin fetch succeeds without Origin-Trial header.');

worker_declares_origin_trial_test(
  get_host_info().HTTPS_REMOTE_ORIGIN, ORIGIN_TRIAL_DISABLED, SUCCESS,
  'Cross-origin fetch succeeds without Origin-Trial header.');

worker_declares_origin_trial_test(
  get_host_info().HTTPS_ORIGIN, ORIGIN_TRIAL_ENABLED, SUCCESS,
  'Same-origin fetch succeeds with Origin-Trial header.');

worker_declares_origin_trial_test(
  get_host_info().HTTPS_REMOTE_ORIGIN, ORIGIN_TRIAL_ENABLED, FAILURE,
  'Cross-origin fetch fails with Origin-Trial header.');
