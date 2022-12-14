async function getDetachedSerial(t) {
  let iframe = document.createElement('iframe');
  const watcher = new EventWatcher(t, iframe, ['load']);
  iframe.src = '';
  const loaded = watcher.wait_for(['load']);
  document.body.appendChild(iframe);
  await loaded;

  // Save navigator.serial from the iframe and call getPorts() to ensure that
  // it is initialized.
  const iframeSerial = iframe.contentWindow.navigator.serial;
  const ports = await iframeSerial.getPorts();
  assert_equals(ports.length, 0);

  document.body.removeChild(iframe);
  // Set iframe to null to ensure that the GC cleans up as much as possible.
  iframe = null;
  GCController.collect();

  return iframeSerial;
}

promise_test(async (t) => {
  const detachedSerial = await getDetachedSerial(t);

  try {
    await detachedSerial.getPorts();
    assert_unreached();
  } catch (e) {
    // Cannot use promise_rejects_dom() because |e| is thrown from a different
    // global.
    assert_equals(e.name, 'NotSupportedError');
  }
}, 'getPorts() rejects in a detached context');

promise_test(async (t) => {
  const detachedSerial = await getDetachedSerial(t);

  try {
    await detachedSerial.requestPort();
    assert_unreached();
  } catch (e) {
    // Cannot use promise_rejects_dom() because |e| is thrown from a different
    // global.
    assert_equals(e.name, 'NotSupportedError');
  }
}, 'requestPort() rejects in a detached context');

promise_test(async (t) => {
  const detachedSerial = await getDetachedSerial(t);
  detachedSerial.addEventListener('connect', () => {});
}, 'adding an event listener does nothing in a detached context');
