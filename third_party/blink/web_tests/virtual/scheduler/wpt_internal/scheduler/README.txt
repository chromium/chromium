This tests a scheduler.postTask feature. It runs tests in wpt_internal/scheduler
directory with the BlinkSchedulerDisableAntiStarvationForPriorities flag, which
guarantees tasks are run in strict priority order.
