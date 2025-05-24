// Schedules consecutive async GCs to run in the future. Returns a promise which
// is resolved when the GC tasks have finished.
function runAsyncGC() {
  // Run gc multiple times to ensure anything needing more than one cycle can be
  // collected, e.g. due to dependencies. Note that:
  //  1. This is similar to ThreadState::CollectAllGarbageForTesting, but async
  //     and with two less iterations.
  //  2. The tasks are scheduled so that they run consecutively, which is
  //     helpful if you need to schedule GC before performing another async
  //     operation that runs at normal priority.
  return Promise.all([1,2,3].map(
      () => gc({type: 'major', execution: 'async'})));
}
