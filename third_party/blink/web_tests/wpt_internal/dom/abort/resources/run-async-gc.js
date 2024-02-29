async function runAsyncGC(args = {}) {
  // Run gc in a loop to ensure anything needing more than one cycle can be
  // collected, e.g. due to dependencies. Note this is similar to
  // ThreadState::CollectAllGarbageForTesting, but async and with 2 less
  // iterations.
  for (let i = 0; i < 3; i++) {
    // crbug.com/1474629: invoking gc({execution: 'async'}) trips leak
    // detection, so use postTask and run sync gc() to do async GC.
    await scheduler.postTask(() => { gc(); }, args);
  }
}
