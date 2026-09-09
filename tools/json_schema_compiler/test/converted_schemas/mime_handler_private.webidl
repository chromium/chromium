// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary StreamInfo {
  // The MIME type of the intercepted URL request.
  required DOMString mimeType;

  // The original URL that was intercepted.
  required DOMString originalUrl;

  // The URL that the stream can be read from.
  required DOMString streamUrl;

  // The ID of the tab that opened the stream. If the stream is not opened in
  // a tab, it will be -1.
  required long tabId;

  // The HTTP response headers of the intercepted request stored as a
  // dictionary mapping header name to header value. If a header name appears
  // multiple times, the header values are merged in the dictionary and
  // separated by a ", ". Non-ASCII headers are dropped.
  required object responseHeaders;

  // Whether the stream is embedded within another document.
  required boolean embedded;
};

dictionary PdfPluginAttributes {
  // The background color in ARGB format for painting. Since the background
  // color is an unsigned 32-bit integer which can be outside the range of
  // "long" type, define it as a "double" type here.
  required double backgroundColor;

  // Indicates whether the plugin allows to execute JavaScript and maybe XFA.
  // Loading XFA for PDF forms will automatically be disabled if this flag is
  // false.
  required boolean allowJavascript;
};

callback GetStreamDetailsCallback = undefined (StreamInfo streamInfo);
callback SetShowBeforeUnloadDialogCallback = undefined ();

// |streamUrl|: Unique ID for the instance that should perform the save.
callback OnSaveListener = undefined (DOMString streamUrl);

interface OnSaveEvent : ExtensionEvent {
  static undefined addListener(OnSaveListener listener);
  static undefined removeListener(OnSaveListener listener);
  static boolean hasListener(OnSaveListener listener);
};

// Mime handler API.
[nodoc]
interface MimeHandlerPrivate {
  // Returns the StreamInfo for the stream for this context if there is one.
  [nocompile]
  static undefined getStreamInfo(GetStreamDetailsCallback callback);

  // Sets PDF plugin attributes in the stream for this context if there is
  // one.
  [nocompile]
  static undefined setPdfPluginAttributes(
      PdfPluginAttributes pdfPluginAttributes);

  // Instructs the PluginDocument, if running in one, to show a dialog in
  // response to beforeunload events.
  [nocompile]
  static undefined setShowBeforeUnloadDialog(
      boolean showDialog,
      optional SetShowBeforeUnloadDialogCallback callback);

  // Fired when the browser wants the listener to perform a save.
  static attribute OnSaveEvent onSave;
};

partial interface Browser {
  static attribute MimeHandlerPrivate mimeHandlerPrivate;
};
