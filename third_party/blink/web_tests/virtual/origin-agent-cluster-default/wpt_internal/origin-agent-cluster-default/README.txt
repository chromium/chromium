Tests for the OriginAgentClusterDefaultWarning feature.

The OriginAgentClusterDefaultWarning feature issues deprecation warnings
when document.domain is set in order to relax same-site restrictions. This
supports the deprecation of this mis-feature.

The tests rely on default isolation behaviour. The test runner modifies this
by default with auto-wpt-origin-isolation; that is, the test runner behaves
differently from the actual browser. This is incompatible with this test
suite. Hence this virtual test suite enables the
iOriginAgentClusterDefaultWarning feature, and disables
auto-wpt-origin-isolation. These tests are expected to fail without these
flags, and are disabled (via NeverFixTests) in the default configuration.
