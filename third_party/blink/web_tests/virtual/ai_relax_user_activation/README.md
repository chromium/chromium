# AI Relax User Activation Virtual Test Suite

This virtual test suite tests the behavior of Built-In AI APIs when the
`AIRelaxUserActivationReqs` feature flag is ENABLED.

Specifically, it verifies that certain operations (like LanguageModel.create())
only require *sticky* user activation rather than *transient* user activation
when this flag is active.

See crbug.com/454435239 for more context.
