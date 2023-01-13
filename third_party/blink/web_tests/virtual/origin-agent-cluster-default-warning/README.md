Tests for the OriginAgentClusterDefaultWarning feature.

The OriginAgentClusterDefaultWarning feature issues deprecation warnings
when document.domain is set in order to relax same-site restrictions. This
supports the deprecation of this mis-feature.

Note: These tests require isolation behaviour as in the actual browser.
They are therefore incompatible with auto-wpt-origin-isolation, which the test
runner enabled by default. Hence this virtual test suite enables the
OriginAgentClusterDefaultWarning feature, and disables
auto-wpt-origin-isolation. These tests are expected to fail without these
flags, and are disabled (via NeverFixTests) in the default configuration.
