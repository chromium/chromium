This directory is for testing Referrer Policy within the PolicyContainer.

This test suite runs the tests with --enable-features=PolicyContainer. The
experimental flag enables setting/inheriting Referrer Policy via the Policy
Container, a new mechanism which uses policies stored in the browser on the
RenderFrameHost as authoritative policies for creating new frames.
