# AI Require Transient User Activation Virtual Test Suite

This virtual test suite tests the behavior of Built-In AI APIs when the
`AIRelaxUserActivationReqs` feature flag is DISABLED.

Specifically, it verifies that certain operations (like LanguageModel.create())
require *transient* user activation to succeed, and will fail if only *sticky*
user activation is present. This is the behavior when the
`AIRelaxUserActivationReqs` feature flag is DISABLED.

See crbug.com/454435239 for more context.
