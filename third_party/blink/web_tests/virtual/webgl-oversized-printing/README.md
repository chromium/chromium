# webgl-oversized-printing

This directory and virtual test suite are used only to isolate the single test
printing/webgl-oversized-printing.html into its own content_shell instance. This
test makes a large memory allocation which frequently causes OOM when run after
other tests in the same content_shell.

All tests in this directory are run with the flag
--enable-webgl-developer-extensions ; this is effectively a no-op.
