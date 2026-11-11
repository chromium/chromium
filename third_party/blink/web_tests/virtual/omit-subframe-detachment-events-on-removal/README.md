This directory contains tests with --disable-blink-features=OmitSubframeDetachmentEventsOnRemoval.
It maintains tests for the legacy behavior of firing pagehide, visibilitychange, and unload events
when a subframe is removed from the DOM, with the intention to remove or update the expectations
for these tests when the behavior change becomes stable.
