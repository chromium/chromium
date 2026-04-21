This virtual test enables `kPrefetchOffTheMainThreadForceForTesting` and
forcibly triggers off-the-main-thread prefetch code path to anyway increase the
test coverage of off-the-main-thread prefetch. https://crbug.com/452389538
`kPrefetchOffTheMainThreadForceForTesting` is anyway expected to be incomplete,
and expected deviations are explicitly tracked in `TestExpectations`.
