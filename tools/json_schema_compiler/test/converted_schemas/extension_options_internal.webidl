// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary SizeChangedOptions {
  required long oldWidth;
  required long oldHeight;
  required long newWidth;
  required long newHeight;
};

dictionary PreferredSizeChangedOptions {
  required double width;
  required double height;
};

// Listener callback for the onClose event.
callback OnCloseListener = undefined ();

interface OnCloseEvent : ExtensionEvent {
  static undefined addListener(OnCloseListener listener);
  static undefined removeListener(OnCloseListener listener);
  static boolean hasListener(OnCloseListener listener);
};

// Listener callback for the onLoad event.
callback OnLoadListener = undefined ();

interface OnLoadEvent : ExtensionEvent {
  static undefined addListener(OnLoadListener listener);
  static undefined removeListener(OnLoadListener listener);
  static boolean hasListener(OnLoadListener listener);
};

// Listener callback for the onPreferredSizeChanged event.
callback OnPreferredSizeChangedListener = undefined (
    PreferredSizeChangedOptions options);

interface OnPreferredSizeChangedEvent : ExtensionEvent {
  static undefined addListener(OnPreferredSizeChangedListener listener);
  static undefined removeListener(OnPreferredSizeChangedListener listener);
  static boolean hasListener(OnPreferredSizeChangedListener listener);
};

// Internal API for the &lt;extensiontoptions&gt; tag
interface ExtensionOptionsInternal {
  static attribute OnCloseEvent onClose;
  static attribute OnLoadEvent onLoad;
  static attribute OnPreferredSizeChangedEvent onPreferredSizeChanged;
};

partial interface Browser {
  static attribute ExtensionOptionsInternal extensionOptionsInternal;
};
