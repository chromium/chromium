let releaseCompletion;
const holdPromise = new Promise(resolve => {
  releaseCompletion = resolve;
});

self.addEventListener('backgroundfetchsuccess', event => {
  event.waitUntil(holdPromise);
});

self.addEventListener('message', event => {
  if (event.data === 'release') {
    const port = event.ports[0];
    releaseCompletion();
    holdPromise.then(() => port?.postMessage('released'));
  }
});
