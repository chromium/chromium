'use strict';

// Tests that attempting to load a Response body from a detached iframe fails
// correctly. Blink's behavior of rejecting the promise is non-standard, so this
// is not included in the upstream wpts.

// Each case exercises a different method on the body. `method` is the name of
// the method to call. `type` is the Content-Type for the response. `body` is
// the body of the response. It should parse correctly into the specified
// format. Because it is interpolated into JavaScript it should not contain
// quote or backslash characters.
const CASES = [
  {
    method: 'arrayBuffer',
    type: 'application/octet-stream',
    body: 'anything',
    expected_type: 'ArrayBuffer',
  },
  {
    method: 'blob',
    type: 'application/octet-stream',
    body: 'anything',
    expected_type: 'Blob',
  },
  {
    method: 'formData',
    type: 'application/x-www-form-urlencoded',
    body: 'a=b',
    expected_type: 'FormData',
  },
  {
    method: 'json',
    type: 'text/json',
    body: '[3]',
    expected_type: 'Array',
  },
  {
    method: 'text',
    text: 'text/plain',
    body: 'word',
    expected_type: 'string',
  },
];

function assert_type_is(value, expected_type, description) {
  let actual_type = typeof value;
  if (actual_type === 'object') {
    actual_type = value.constructor.name;
  }
  assert_equals(actual_type, expected_type, description);
}

// First just check that the test cases work at all, so we don't fail for the
// wrong reasons.
for (const {method, type, body, expected_type} of CASES) {
  promise_test(async () => {
    const response = new Response(body, { headers: { 'Content-Type': type } });
    const result = await response[method]();
    assert_not_equals(result, undefined, 'result should be defined');
    assert_not_equals(result, null, 'result should not be null');
    assert_type_is(result, expected_type, `result should be ${expected_type}`);
  }, `decoding a response with method ${method} should work`);
}

// Now verify that they reject correctly when the body was created in an iframe
// that has been detached.

// We can wait for this to make sure the document has a body.
const loaded = new Promise(resolve => {
  window.onload = resolve;
});

async function waitForDoneMessage(t) {
  const watcher = new EventWatcher(t, window, [ 'message' ]);
  const evt = await watcher.wait_for([ 'message' ]);
  if (evt.data != 'done') {
    throw RangeError('bad message');
  }
}

async function appendIFrame(t, body, type) {
  await loaded;
  const iframe = document.createElement('iframe');
  iframe.srcdoc = `
<script>
    window.response = new Response('${body}',
                                   { headers: { 'Content-Type': '${type}' } });
    parent.postMessage('done', '*');
</script>
`;
  document.body.appendChild(iframe);
  await waitForDoneMessage(t);
  return iframe;
}

for (const {method, type, body} of CASES) {
  promise_test(async t => {
    const iframe = await appendIFrame(t, body, type);
    const response = iframe.contentWindow.response;
    const IFrameDOMException = iframe.contentWindow.DOMException;
    iframe.remove();
    await promise_rejects_dom(t, 'InvalidStateError', IFrameDOMException,
                              response[method](), `${method}() should reject`);
  }, `decoding a response from a detached frame with method ${method} ` +
               'should reject');
}

promise_test(async t => {
  const iframe = await appendIFrame(t, 'chunk', 'text/plain');
  const response = iframe.contentWindow.response;
  const IFrameTypeError = iframe.contentWindow.TypeError;
  iframe.remove();
  const reader = response.body.getReader();
  await promise_rejects_js(t, IFrameTypeError, reader.read(),
                           'read() should reject');
}, 'reading a response from a detached frame as a stream should reject');
