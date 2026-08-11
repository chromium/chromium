// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum TemplateType {
  // Contains an icon, title, message, expandedMessage, and up to two buttons.
  "basic",

  // Contains an icon, title, message, expandedMessage, image, and up
  //  to two buttons.
  "image",

  // Contains an icon, title, message, items, and up to two buttons. Users on
  // Mac OS X only see the first item.
  "list",

  // Contains an icon, title, message, progress, and up to two buttons.
  "progress"
};

enum PermissionLevel {
  // Specifies that the user has elected to show notifications from the app or
  // extension. This is the default at install time.
  "granted",

  // Specifies that the user has elected not to show notifications from the app
  // or extension.
  "denied"
};

dictionary NotificationItem {
  // Title of one item of a list notification.
  required DOMString title;

  // Additional details about this item.
  required DOMString message;
};

[nodoc] dictionary NotificationBitmap {
  [nodoc] required long width;
  [nodoc] required long height;
  [nodoc] ArrayBuffer data;
};

dictionary NotificationButton {
  required DOMString title;
  [deprecated="Button icons not visible for Mac OS X users."]
  DOMString iconUrl;
  [nodoc] NotificationBitmap iconBitmap;
};

dictionary NotificationOptions {
  // Which type of notification to display.
  // <em>Required for $(ref:notifications.create)</em> method.
  TemplateType type;

  // A URL to the sender's avatar, app icon, or a thumbnail for image
  // notifications.
  //
  // URLs can be a data URL, a blob URL, or a URL relative to a resource within
  // this extension's .crx file
  // <aside class="note"><b>Note:</b>This value is required for the
  // $(ref:notifications.create)<code>()</code> method.</aside>
  DOMString iconUrl;
  [nodoc] NotificationBitmap iconBitmap;

  // A URL to the app icon mask. URLs have the same restrictions as
  // $(ref:notifications.NotificationOptions.iconUrl iconUrl).
  //
  // The app icon mask should be in alpha channel, as only the alpha channel of
  // the image will be considered.
  [deprecated="The app icon mask is not visible for Mac OS X users."]
  DOMString appIconMaskUrl;
  [nodoc] NotificationBitmap appIconMaskBitmap;

  // Title of the notification (e.g. sender name for email).
  // <aside class="note"><b>Note:</b>This value is required for the
  // $(ref:notifications.create)<code>()</code> method.</aside>
  DOMString title;

  // Main notification content.
  // <aside class="note"><b>Note:</b>This value is required for the
  // $(ref:notifications.create)<code>()</code> method.</aside>
  DOMString message;

  // Alternate notification content with a lower-weight font.
  DOMString contextMessage;

  // Priority ranges from -2 to 2. -2 is lowest priority. 2 is highest. Zero is
  // default.  On platforms that don't support a notification center (Windows,
  // Linux & Mac), -2 and -1 result in an error as notifications with those
  // priorities will not be shown at all.
  long priority;

  // A timestamp associated with the notification, in milliseconds past the
  // epoch (e.g. <code>Date.now() + n</code>).
  double eventTime;

  // Text and icons for up to two notification action buttons.
  sequence<NotificationButton> buttons;

  // Secondary notification content.
  [nodoc] DOMString expandedMessage;

  // A URL to the image thumbnail for image-type notifications.
  // URLs have the same restrictions as
  // $(ref:notifications.NotificationOptions.iconUrl iconUrl).
  [deprecated="The image is not visible for Mac OS X users."]
  DOMString imageUrl;
  [nodoc] NotificationBitmap imageBitmap;

  // Items for multi-item notifications. Users on Mac OS X only see the first
  // item.
  sequence<NotificationItem> items;

  // Current progress ranges from 0 to 100.
  long progress;

  [deprecated="This UI hint is ignored as of Chrome 67"]
  boolean isClickable;

  // Indicates that the notification should remain visible on screen until the
  // user activates or dismisses the notification. This defaults to false.
  boolean requireInteraction;

  // Indicates that no sounds or vibrations should be made when the notification
  // is being shown. This defaults to false.
  boolean silent;
};

callback OnClosedListener = undefined (
    DOMString notificationId, boolean byUser);

interface OnClosedEvent : ExtensionEvent {
  static undefined addListener(OnClosedListener listener);
  static undefined removeListener(OnClosedListener listener);
  static boolean hasListener(OnClosedListener listener);
};

callback OnClickedListener = undefined (DOMString notificationId);

interface OnClickedEvent : ExtensionEvent {
  static undefined addListener(OnClickedListener listener);
  static undefined removeListener(OnClickedListener listener);
  static boolean hasListener(OnClickedListener listener);
};

callback OnButtonClickedListener = undefined (
    DOMString notificationId, long buttonIndex);

interface OnButtonClickedEvent : ExtensionEvent {
  static undefined addListener(OnButtonClickedListener listener);
  static undefined removeListener(OnButtonClickedListener listener);
  static boolean hasListener(OnButtonClickedListener listener);
};

callback OnPermissionLevelChangedListener = undefined (PermissionLevel level);

interface OnPermissionLevelChangedEvent : ExtensionEvent {
  static undefined addListener(OnPermissionLevelChangedListener listener);
  static undefined removeListener(OnPermissionLevelChangedListener listener);
  static boolean hasListener(OnPermissionLevelChangedListener listener);
};

callback OnShowSettingsListener = undefined ();

interface OnShowSettingsEvent : ExtensionEvent {
  static undefined addListener(OnShowSettingsListener listener);
  static undefined removeListener(OnShowSettingsListener listener);
  static boolean hasListener(OnShowSettingsListener listener);
};


// Use the <code>chrome.notifications</code> API to create rich notifications
// using templates and show these notifications to users in the system tray.
interface Notifications {
  // Creates and displays a notification.
  // |notificationId|: Identifier of the notification. If not set or empty, an
  // ID will automatically be generated. If it matches an existing
  // notification, this method first clears that notification before
  // proceeding with the create operation. The identifier may not be longer
  // than 500 characters.
  //
  // The <code>notificationId</code> parameter is required before Chrome 42.
  // |options|: Contents of the notification.
  // |Returns|: Returns a Promise which resolves with the notification id
  // (either supplied or generated) that represents the created
  // notification.
  // |PromiseValue|: notificationId
  static Promise<DOMString> create(
      optional DOMString notificationId, NotificationOptions options);

  // Updates an existing notification.
  // |notificationId|: The id of the notification to be updated. This is
  // returned by $(ref:notifications.create) method.
  // |options|: Contents of the notification to update to.
  // |Returns|: Returns a Promise which resolves to indicate whether a
  // matching notification existed.
  // |PromiseValue|: wasUpdated
  static Promise<boolean> update(
      DOMString notificationId, NotificationOptions options);

  // Clears the specified notification.
  // |notificationId|: The id of the notification to be cleared. This is
  // returned by $(ref:notifications.create) method.
  // |Returns|: Returns a Promise which resolves to indicate whether a
  // matching notification existed.
  // |PromiseValue|: wasCleared
  static Promise<boolean> clear(DOMString notificationId);

  // Retrieves all the notifications of this app or extension.
  // |Returns|: Returns a Promise which resolves with the set of
  // notification_ids currently in the system.
  // |PromiseValue|: notifications
  static Promise<object> getAll();

  // Retrieves whether the user has enabled notifications from this app
  // or extension.
  // |Returns|: Returns a Promise which resolves with the current permission
  // level.
  // |PromiseValue|: level
  static Promise<PermissionLevel> getPermissionLevel();

  // The notification closed, either by the system or by user action.
  static attribute OnClosedEvent onClosed;

  // The user clicked in a non-button area of the notification.
  static attribute OnClickedEvent onClicked;

  // The user pressed a button in the notification.
  static attribute OnButtonClickedEvent onButtonClicked;

  // The user changes the permission level.  As of Chrome 47, only ChromeOS
  // has UI that dispatches this event.
  static attribute OnPermissionLevelChangedEvent onPermissionLevelChanged;

  // The user clicked on a link for the app's notification settings.  As of
  // Chrome 47, only ChromeOS has UI that dispatches this event.
  // As of Chrome 65, that UI has been removed from ChromeOS, too.
  [deprecated="Custom notification settings button is no longer supported."]
  static attribute OnShowSettingsEvent onShowSettings;
};

partial interface Browser {
  static attribute Notifications notifications;
};
