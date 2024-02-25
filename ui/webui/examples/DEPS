include_rules = [
  # The WebUI examples currently depends on HTML in the chrome directory.
  # Production code outside of //chrome should not depend on //chrome.
  # This is a one-time exception as the WebUI examples is a non-production app.
  "+chrome/grit/webui_gallery_resources.h",
  "+chrome/grit/webui_gallery_resources_map.h",

  # The GuestView supports the WebView element.
  "+components/guest_view",

  # The WebUI examples is a simple embedder, so it only uses content's public API.
  "+content/public",

  # Devtools relies on IPC definitions.
  "+ipc/ipc_channel.h",

  # The WebUI examples uses mojo to communicate with the browser process.
  "+mojo/public",

  # Devtools relies on various net APIs.
  "+net",

  # Sandbox is part of the main initialization.
  "+sandbox",

  # The renderer support code uses Blink public APIs.
  "+third_party/blink/public",

  # The WebUI examples uses Chromium's UI libraries.
  "+ui/aura",
  "+ui/display",
  "+ui/platform_window",
  "+ui/wm",

  # The WebUI examples is an embedder so it must work with resource bundles.
  "+ui/base/l10n",
  "+ui/base/resource",

  # The renderer support code uses V8.
  "+v8/include",
]
