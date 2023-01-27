# Internal AbortSignal Tests

This directory contains a number of tests that check AbortSignal memory
management using `gc()` to force a synchronous major GC.

Most tests (see ./resources/) are parameterized with a signal and controller
interface so that the can be used by TaskSignal tests also.
