This directory contains tests with --enable-blink-features=OmitSubframeDetachmentEventsOnRemoval.
It tests the behavior where subframe detachment (e.g. removing <iframe> or <object> from the DOM)
does not fire pagehide, visibilitychange, or unload events in the detached subframes.
