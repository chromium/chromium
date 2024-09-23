
// Busy waits for `ms`.
function spin(ms) {
  const start = performance.now();
  while (performance.now() - start < ms);
}

// Returns a promise that is resolved in rAF.
function requestAnimationFramePromise() {
  return new Promise((resolve) => requestAnimationFrame(resolve));
}

// Adds a catch handler to the promise to prevent unhandled rejections from
// spamming the console.
function ignoreUnhandledRejection(p) {
  p.catch(() => {});
}
