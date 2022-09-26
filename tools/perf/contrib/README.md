**Note**: The code is this directory is neither endorsed nor supported
by the Chromium benchmarking team.

To add code to this directory:

1. First check the list of available benchmarking harnesses at
   https://bit.ly/chrome-benchmark-harnesses to check whether your
   use case fits any of the supported harnesses. If there is a
   related harness, please work with us to extend it.

2. If your test case does not fit into an existing harness, create a
   sub-directory with yourself as the OWNER. Send a CL containing ONLY
   the new OWNERS file to the owners of tools/perf/. Your CL description
   should describe what the code you plan to add will do. If the code is
   for an ephemeral benchmark used in a perf project, you will need an
   accompanying bug (assigned to you) to clean up the benchmarks after
   your perf project is launched.

**NOTE**

1. Benchmarks in this directory will not be scheduled for running on the
   perf waterfall.

2. The Chromium benchmarking team will NOT review nor maintain any code
   under this directory. It is the responsibility of the OWNERS of each
   sub-directory to maintain their own code.
