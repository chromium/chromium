# Mock camera virtual test suite

This suite runs inspector-protocol tests for Browser.addMockCamera in a
controlled camera environment.

It uses --video-capture-use-virtual-devices-only so that the tests do not see
cameras connected to the computer, fake cameras that use a video file as their
input, or cameras created by Chromium's existing
--use-fake-device-for-media-stream option. This allows the tests to check the
exact set of mock cameras added during the test.

It also uses --use-fake-ui-for-media-stream to automatically allow camera
access. This lets the test call getUserMedia() and start a video stream from
the mock camera without requiring a user to accept a permission prompt.

The tests run only in this virtual suite because they verify the exact list of
cameras visible to the page.

See crbug.com/489736656.
