self.addEventListener('fetch', event => {
  // Actually does nothing, but trick to make this not recognized as
  // an empty fetch handler. Otherwise, the test timeout waiting
  // kServiceWorkerInterceptedRequestFromOriginDirtyStyleSheet in
  // usecounter-request-from-no-cors-style-sheet.html
  let a = 0;
});
