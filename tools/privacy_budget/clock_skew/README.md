# Clock Skew Tool

The `:clock_skew_tool` binary is a tool for measuring the local device's clock
skew. Specifically, the `ClockSkewTool` class drives the
[`NetworkTimeTracker`](https://source.chromium.org/chromium/chromium/src/+/main:components/network_time/network_time_tracker.h)
class, causing it to repeatedly fetch the current time from a time server,
compare it to the system clock, and record results in histograms. On each
iteration, `ClockSkewTool` prints a report containing the relevant histograms to
stdout.

This tool was created to help investigate the distribution and privacy
implications of clock skew. See the ([design
doc](https://docs.google.com/document/d/1BgYfFB1UzIEBDArVGG7u60d2D18kT7se9_zCIvDBB6s))
and tracking bug: https://crbug.com/1258624.
