// Blink requires entering a ScriptState::Scope in order to enqueue a value in a
// stream, because of use of the incumbent context in v8::Object::New() and the
// incumbent isolate in v8::Exception::TypeError(). The standard doesn't have
// this requirement, so we test Blink's behavior here and the standard behaviour
// in the external wpt tests.

// See https://crbug.com/1395588.

// Adds an iframe to the document and returns it.
function addIframe() {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  return iframe;
}

promise_test(async t => {
  const iframe = addIframe();
  const stream = new iframe.contentWindow.TextDecoderStream();
  const NestedTypeError = iframe.contentWindow.TypeError;
  const readPromise = stream.readable.getReader().read();
  readPromise.then(t.unreached_func("read should not resolve"),
                   t.unreached_func("read should not reject"));
  const writer = stream.writable.getWriter();
  await writer.ready;
  iframe.remove();
  return promise_rejects_js(t, NestedTypeError,
                            writer.write(new Uint8Array([65])),
                            'TypeError should be thrown');
}, 'TextDecoderStream: write in detached realm should fail');

promise_test(async t => {
  const iframe = addIframe();
  const stream = new iframe.contentWindow.TextEncoderStream();
  const NestedTypeError = iframe.contentWindow.TypeError;
  const readPromise = stream.readable.getReader().read();
  readPromise.then(t.unreached_func("read should not resolve"),
                   t.unreached_func("read should not reject"));
  const writer = stream.writable.getWriter();
  await writer.ready;
  iframe.remove();
  return promise_rejects_js(t, NestedTypeError, writer.write('A'),
                            'TypeError should be thrown');
}, 'TextEncoderStream: write in detached realm should fail');

for (const type of ['TextEncoderStream', 'TextDecoderStream']) {
  promise_test(async t => {
    const iframe = addIframe();
    const stream = new iframe.contentWindow[type]();
    const NestedTypeError = iframe.contentWindow.TypeError;
    iframe.remove();
    return promise_rejects_js(t, NestedTypeError, stream.writable.close(),
                              'TypeError should be thrown');
  }, `${type}: close in detached realm should fail`);
}
