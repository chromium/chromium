This directory is for testing Referrer Policy without the PolicyContainer.

This test suite runs the tests with --disable-features=PolicyContainer. The
experimental flag to enable setting/inheriting Referrer Policy via the Policy
Container has been turned on by default, but we keep the flag and this test for
some time to ensure that we can switch back in case something should not work as
expected.
