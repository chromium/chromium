# Browser Clipboard

Platform-neutral clipboard abstractions, to access platform-specific clipboards
(copy/paste) without platform-specific code.

## Clipboard Model

The clipboard can be thought of as an ordered dictionary keyed on format, with
the value as the payload. This dictionary is implemented and accessed
differently on different operating systems (OS’s).

## Interfaces

Interfaces include:
* `Clipboard`: reading/pasting from the clipboard.
* `ScopedClipboardWriter`: writing/copying to the clipboard.
* `ClipboardObserver`: notifications of clipboard events.
* `ClipboardFormatType`: specifying clipboard formats.

## Platform-specific behavior

While most platform-specific behavior should be abstracted away, some may
still be exposed. For some notable platform-specific behavior exposed by these
interfaces:
* `ClipboardAndroid` has a more limited set of supported formats.
* `ClipboardObserver` is only supported on some platforms, as other platforms
  may require (inefficient) polling to implement.
* Every platform may have different combinations of clipboard formats written,
  or metadata written, for each clipboard format. For example, text in Windows
  is written with a carriage return accompanying newlines, and in Linux requires
  multiple MIME types to represent.
* `ClipboardX11` supports both the usual clipboard buffer (CLIPBOARD selection),
  as well as the middle-click paste buffer (PRIMARY selection). X11 selections
  are documented in more detail in
  [X11 documentation](https://tronche.com/gui/x/icccm/sec-2.html#s-2.6.1).
* `DataTransferPolicyController` is only currently exercised in ChromeOS.
* `ClipboardWin` and `ClipboardX11` have limits to the amount of registered
  clipboard formats. Windows has the smallest limit on the number of formats, at
  [16384](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclipboardformata#remarks),
  and explored in
  [this article](https://devblogs.microsoft.com/oldnewthing/20150319-00/?p=44433).
  After these system resources are exhausted, the underlying OS may be rendered
  unusable.