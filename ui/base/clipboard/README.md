Platform-neutral clipboard abstractions, to access platform-specific clipboards
(copy/paste) without platform-specific code.

Interfaces include:
* `Clipboard`: reading/pasting from the clipboard.
* `ScopedClipboardWriter`: writing/copying to the clipboard.
* `ClipboardObserver`: notifications of clipboard events.
* `ClipboardFormatType`: specifying clipboard formats.

While most platform-specific behavior should be abstracted away, some may
still be exposed. For some notable platform-specific behavior exposed by these
interfaces:
* `ClipboardAndroid` has a more limited set of supported formats.
* `ClipboardObserver` is only supported on some platforms, as other platforms
  may require (inefficient) polling to implement.
* `ClipboardX11` supports both the usual clipboard buffer, as well as the
  selection (middle-click) paste buffer.
* `DataTransferPolicyController` is only currently exercised in ChromeOS.
