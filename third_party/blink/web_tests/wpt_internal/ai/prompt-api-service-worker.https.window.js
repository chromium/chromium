promise_test(async () => {
  const result = await new Promise(async resolve => {
    navigator.serviceWorker.register('test-service-worker.js');
    navigator.serviceWorker.ready.then(() => {
      navigator.serviceWorker.onmessage = e => {
        resolve(e.data);
      }
    });
  });

  assert_true(result.success, result.error);
});
