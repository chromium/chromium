**Do not manually add files to this generated subdirectory.**

The `web_platform` directory contains a copy of HTML files from the WebGPU
conformance test suite.
This copy is updated using `run_regenerate_internal_cts_html.py`, which will
wipe any extra files from subdirectories that get regenerated, like `reftests`.

The copies are kept in `wpt_internal` because they are not part of the upstream
WPT (in `external/wpt`) even though they are "external" (from the WebGPU CTS).
The JavaScript files referenced by these tests are generated at build-time.
These HTML files could theoretically be generated too, but it's a bit harder.
