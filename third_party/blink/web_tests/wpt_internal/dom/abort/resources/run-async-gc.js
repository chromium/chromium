async function runAsyncGC(highPriority) {
  // Run gc in a loop to ensure anything needing more than one cycle can be
  // collected, e.g. due to dependencies. Note this is similar to
  // ThreadState::CollectAllGarbageForTesting, but async and with 2 less
  // iterations.
  for (let i = 0; i < 3; i++) {
    if (highPriority) {
      await scheduler.postTask(() => { gc(); }, {priority: 'user-blocking'});
    } else {
      await gc({type: 'major', execution: 'async'});
    }
  }
}
