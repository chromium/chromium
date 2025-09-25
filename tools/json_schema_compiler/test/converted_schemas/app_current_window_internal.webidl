// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Null or undefined indicates that a value should not change.
dictionary Bounds {
  long left;
  long top;
  long width;
  long height;
};

// Null or undefined indicates that a value should not change. A value of 0
// will clear the constraints.
dictionary SizeConstraints {
  long minWidth;
  long minHeight;
  long maxWidth;
  long maxHeight;
};

dictionary RegionRect {
  required long left;
  required long top;
  required long width;
  required long height;
};

dictionary Region {
  sequence<RegionRect> rects;
};

callback OnClosedListener = undefined();

interface OnClosedEvent : ExtensionEvent {
  static undefined addListener(OnClosedListener listener);
  static undefined removeListener(OnClosedListener listener);
  static boolean hasListener(OnClosedListener listener);
};

callback OnBoundsChangedListener = undefined();

interface OnBoundsChangedEvent : ExtensionEvent {
  static undefined addListener(OnBoundsChangedListener listener);
  static undefined removeListener(OnBoundsChangedListener listener);
  static boolean hasListener(OnBoundsChangedListener listener);
};

callback OnFullscreenedListener = undefined();

interface OnFullscreenedEvent : ExtensionEvent {
  static undefined addListener(OnFullscreenedListener listener);
  static undefined removeListener(OnFullscreenedListener listener);
  static boolean hasListener(OnFullscreenedListener listener);
};

callback OnMinimizedListener = undefined();

interface OnMinimizedEvent : ExtensionEvent {
  static undefined addListener(OnMinimizedListener listener);
  static undefined removeListener(OnMinimizedListener listener);
  static boolean hasListener(OnMinimizedListener listener);
};

callback OnMaximizedListener = undefined();

interface OnMaximizedEvent : ExtensionEvent {
  static undefined addListener(OnMaximizedListener listener);
  static undefined removeListener(OnMaximizedListener listener);
  static boolean hasListener(OnMaximizedListener listener);
};

callback OnRestoredListener = undefined();

interface OnRestoredEvent : ExtensionEvent {
  static undefined addListener(OnRestoredListener listener);
  static undefined removeListener(OnRestoredListener listener);
  static boolean hasListener(OnRestoredListener listener);
};

callback OnAlphaEnabledChangedListener = undefined();

interface OnAlphaEnabledChangedEvent : ExtensionEvent {
  static undefined addListener(OnAlphaEnabledChangedListener listener);
  static undefined removeListener(OnAlphaEnabledChangedListener listener);
  static boolean hasListener(OnAlphaEnabledChangedListener listener);
};

// This is used by the app window API internally to pass through messages to
// the shell window.
interface CurrentWindowInternal {
  static undefined focus();
  static undefined fullscreen();
  static undefined minimize();
  static undefined maximize();
  static undefined restore();
  static undefined drawAttention();
  static undefined clearAttention();
  static undefined show(optional boolean focused);
  static undefined hide();
  static undefined setBounds(DOMString boundsType, Bounds bounds);
  static undefined setSizeConstraints(DOMString boundsType,
                                      SizeConstraints constraints);
  static undefined setIcon(DOMString icon_url);
  static undefined setShape(Region region);
  static undefined setAlwaysOnTop(boolean always_on_top);
  static undefined setVisibleOnAllWorkspaces(boolean always_visible);
  static undefined setActivateOnPointer(boolean activate_on_pointer);

  static attribute OnClosedEvent onClosed;
  static attribute OnBoundsChangedEvent onBoundsChanged;
  static attribute OnFullscreenedEvent onFullscreened;
  static attribute OnMinimizedEvent onMinimized;
  static attribute OnMaximizedEvent onMaximized;
  static attribute OnRestoredEvent onRestored;
  static attribute OnAlphaEnabledChangedEvent onAlphaEnabledChanged;
};

partial interface App {
  static attribute CurrentWindowInternal currentWindowInternal;
};

partial interface Browser {
  static attribute App app;
};
