test(t => {
  let count = 0;
  const controller = new AbortController();
  const signal = controller.signal;
  addEventListener('test', () => { ++count; }, {signal});

  // GC should not affect the event dispatch or listener removal below.
  gc();

  dispatchEvent(new Event('test'));
  dispatchEvent(new Event('test'));

  assert_equals(count, 2);

  controller.abort();
  dispatchEvent(new Event('test'));
  assert_equals(count, 2);
}, 'AbortSignalRegistry tracks algorithm handles for event listeners');
