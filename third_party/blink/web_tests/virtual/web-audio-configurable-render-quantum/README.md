# Configurable Render Quantum Size Tests

This virtual test suite runs with
"--enable-features=WebAudioConfigurableRenderQuantum" set. This allows testing
that running at different render quantum sizes produces the same output.
TODO(crbug.com/40637820) Remove this test suite once the feature is shipped to
stable.