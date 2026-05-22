self.addEventListener('message', async e => {
  try {
    await self.registration.backgroundFetch.fetch('test-id', '/background-fetch/resources/feature-name.txt');
    e.source.postMessage({
      result: 'success',
      errorName: null,
      errorMessage: null
    });
  } catch (err) {
    e.source.postMessage({
      result: 'error',
      errorName: err.name,
      errorMessage: err.message
    });
  }
});
