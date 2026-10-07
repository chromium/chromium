// Worker script for mediasource-worker-terminate-during-attach.html.
const params = new URLSearchParams(location.search);
const lockName = params.get('lock');

navigator.locks.request(lockName, () => {
  const source = new MediaSource();
  const handle = source.handle;
  postMessage(handle, [handle]);
  if (params.has('close')) {
    self.close();
  }
  // Hold the lock until the worker's ExecutionContext is destroyed.
  return new Promise(() => {});
});
