This directory contains odds and ends that *directly* interface between Cocoa,
the native macOS UI toolkit and Chrome's [Views toolkit](../README.md).

If you're looking for the bigger picture of how Cocoa and Views interact, you
are looking for Remote Cocoa, the system that bridges Cocoa objects like
NSWindows and NSViews with their Views counterparts either in-process (the
browser) or out-of-process (PWA app shims). The [Remote MacViews Overview](https://docs.google.com/document/d/1tjYeRQJreSU_-uJP1LaoI9Oe_uPZKbVhWr9fx_bOvkk) is a good place to
start and [`components/remote_cocoa`](../../../components/remote_cocoa/) is the best place to start code exploration.