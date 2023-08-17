# This suite runs tests with --enable-features=DeprecatedNonStreamingDeclarativeShadowDOM.

While the feature is enabled by default in runtime_enabled_features.json5, it is disabled via fieldtrial_testing_config.json, so tests run with the feature disabled by default. This virtual test suite ensures that things function correctly when the feature is enabled.

