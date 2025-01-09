# Tests for negotiating H265 relating to level-id

Regardless of which level-id is offered, it should be possible to "downgrade"
the level in the answer. These tests include negotiating the lowest possible
Level 1.0 in the SDP answer.

This is a virtual test suite because the H265 codec has not been shipped yet and
require the following flags:
--enable-features=WebRtcAllowH265Send,WebRtcAllowH265Receive
--force-fieldtrials=WebRTC-Video-H26xPacketBuffer/Enabled/

When these are enabled-by-default a virtual test suite will no longer be needed,
but the tests will only run if H265 is supported in HW. Otherwise the tests are
expected to PRECONDITION_FAILED.
