// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/message_center_public_export.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {
class ColorProvider;
}

namespace message_center {

// Represents an individual item in NOTIFICATION_TYPE_MULTIPLE notifications.
class MESSAGE_CENTER_PUBLIC_EXPORT NotificationItem {
 public:
  NotificationItem(const std::u16string& title,
                   const std::u16string& message,
                   ui::ImageModel icon = ui::ImageModel());

  NotificationItem();
  NotificationItem(const NotificationItem& other);
  NotificationItem(NotificationItem&& other);
  NotificationItem& operator=(const NotificationItem& other);
  NotificationItem& operator=(NotificationItem&& other);
  ~NotificationItem();

  const std::u16string& title() const { return title_; }
  const std::u16string& message() const { return message_; }
  const std::optional<ui::ImageModel>& icon() const { return icon_; }

 private:
  std::u16string title_;
  std::u16string message_;
  std::optional<ui::ImageModel> icon_;
};

enum class SettingsButtonHandler {
  NONE = 0,      // No button. This is the default. Does not affect native
                 // settings button (like on Android).
  INLINE = 1,    // Button shown, settings inline.
  DELEGATE = 2,  // Button shown, notification's delegate handles action.
};

enum class SystemNotificationWarningLevel { NORMAL, WARNING, CRITICAL_WARNING };

enum class NotificationScenario {
  DEFAULT = 0,
  INCOMING_CALL = 1,  // When created by an installed origin, the notification
                      // should have increased priority, colored buttons, a
                      // ringtone, and a default "close" button. If the origin
                      // is not installed, it should behave like `DEFAULT`, but
                      // with the added "Close" button.
};

enum class ButtonType {
  DEFAULT = 0,      // Default notification button.
  ACKNOWLEDGE = 1,  // Incoming call acknowledge button.
  DISMISS = 2,      // Incoming call dismiss button.
};

// Represents a button to be shown as part of a notification.
struct MESSAGE_CENTER_PUBLIC_EXPORT ButtonInfo {
  explicit ButtonInfo(const std::u16string& title);
  ButtonInfo(const gfx::VectorIcon* vector_icon,
             const std::u16string& accessible_name);
  ButtonInfo();
  ButtonInfo(const ButtonInfo& other);
  ButtonInfo(ButtonInfo&& other);
  ButtonInfo& operator=(const ButtonInfo& other);
  ButtonInfo& operator=(ButtonInfo&& other);
  ~ButtonInfo();

  // Title that should be displayed on the notification button.
  std::u16string title;

  // TODO(b/324953777): Consider removing this member variable in favor of
  // replacing it with `vector_icon`.
  // Icon that should be displayed on the notification button. Optional. On some
  // platforms, a mask will be applied to the icon, to match the visual
  // requirements of the notification. As with Android, MD notifications don't
  // display this icon.
  gfx::Image icon;

  // Vector icon to that's used for icon-only notification buttons.
  raw_ptr<const gfx::VectorIcon> vector_icon = &gfx::kNoneIcon;

  // Accessible name to be used for the button's tooltip. Required when creating
  // an icon-only notification button.
  std::u16string accessible_name;

  // The placeholder string that should be displayed in the input field for
  // text input type buttons until the user has entered a response themselves.
  // If the value is null, there is no input field associated with the button.
  std::optional<std::u16string> placeholder;

  // Describes the button intended usage. This is used by the underlying
  // platform to take behavioral and stylistic decisions.
  ButtonType type = ButtonType::DEFAULT;
};

enum class FullscreenVisibility {
  NONE = 0,       // Don't show the notification over fullscreen (default).
  OVER_USER = 1,  // Show over the current fullscreened client window.
                  // windows (like Chrome OS login).
};

// Represents rich features available for notifications.
class MESSAGE_CENTER_PUBLIC_EXPORT RichNotificationData {
 public:
  RichNotificationData();
  RichNotificationData(const RichNotificationData& other);
  ~RichNotificationData();

  // Priority of the notification. This must be one of the NotificationPriority
  // values defined in notification_types.h.
  int priority = DEFAULT_PRIORITY;

  // Whether the notification should remain on screen indefinitely.
  bool never_timeout = false;

  // Time indicating when the notification was shown. Defaults to the time at
  // which the RichNotificationData instance is constructed.
  base::Time timestamp;

  // Context message to display below the notification's content. Optional. May
  // not be used for notifications that have an explicit origin URL set.
  std::u16string context_message;

  // Large image to display on the notification. Optional.
  gfx::Image image;

#if BUILDFLAG(IS_CHROMEOS)
  // The path to the file that backs `image`. Set if `image` is file backed.
  std::optional<base::FilePath> image_path;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Small badge to display on the notification to illustrate the source of the
  // notification. Optional.
  gfx::Image small_image;

  // If true, the small image should be masked with the foreground and then
  // added on top of the background. Masking is delayed until the notification
  // is in the views hierarchy or about to be passed to the OS.
  bool small_image_needs_additional_masking = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If true, we simply use the raw |small_image| icon, ignoring accent color
  // styling. For example, this is used with raw icons received from Android.
  bool ignore_accent_color_for_small_image = false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Vector version of |small_image|.
  // Used by Notification::GenerateMaskedSmallIcon.
  // If not available, |small_image| will be used by the method. Optional.
  //
  // Due to the restriction of CreateVectorIcon, this should be a pointer to
  // globally defined VectorIcon instance e.g. kNotificationCapsLockIcon.
  // gfx::Image created by gfx::CreateVectorIcon internally stores reference to
  // VectorIcon, so the VectorIcon should live longer than gfx::Image instance.
  // As a temporary solution to this problem, we make this variable a pointer
  // and only pass globally defined constants.
  // TODO(tetsui): Remove the pointer, after fixing VectorIconSource not to
  // retain VectorIcon reference.  https://crbug.com/760866
  // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always points to a
  // global), so there is no benefit to using a raw_ptr, only cost.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* vector_small_image = &gfx::kNoneIcon;

  // Vector image to display on the parent notification of this notification,
  // illustrating the source of the group notification that this notification
  // belongs to. Optional. Note that all notification belongs to the same group
  // should have the same `parent_vector_small_image`.
  // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always points to a
  // global), so there is no benefit to using a raw_ptr, only cost.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* parent_vector_small_image =
      &gfx::kNoneIcon;

  // Items to display on the notification. Only applicable for notifications
  // that have type NOTIFICATION_TYPE_MULTIPLE.
  std::vector<NotificationItem> items;

  // Progress, in range of [0-100], of NOTIFICATION_TYPE_PROGRESS notifications.
  // Values outside of the range (e.g. -1) will show an infinite loading
  // progress bar.
  int progress = 0;

  // Status text string shown in NOTIFICATION_TYPE_PROGRESS notifications.
  // If MD style notification is not enabled, this attribute is ignored.
  std::u16string progress_status;

  // Buttons that should show up on the notification. A maximum of 16 buttons
  // is supported by the current implementation, but this may differ between
  // platforms.
  std::vector<ButtonInfo> buttons;

  // Whether updates to the visible notification should be announced to users
  // depending on visual assistance systems.
  bool should_make_spoken_feedback_for_popup_updates = true;

#if BUILDFLAG(IS_CHROMEOS)
  // Flag if the notification is pinned. If true, the notification is pinned
  // and the user can't remove it.
  bool pinned = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Vibration pattern to play when displaying the notification. There must be
  // an odd number of entries in this pattern when it's set: numbers of
  // milliseconds to vibrate separated by numbers of milliseconds to pause.
  std::vector<int> vibration_pattern;

  // Whether the vibration pattern and other applicable announcement mechanisms
  // should be considered when updating the notification.
  bool renotify = false;

  // Whether all announcement mechansims should be suppressed when displaying
  // the notification.
  bool silent = false;

  // An accessible description of the notification's contents.
  std::u16string accessible_name;

  // Unified theme color used in new style notification.
  // Usually, it should not be set directly.
  // For system notification, ash::CreateSystemNotification with
  // SystemNotificationWarningLevel should be used.
  std::optional<SkColor> accent_color;

  // Similar to `accent_color`, but store a ColorId instead of SkColor so that
  // the notification view can use this id to correctly handle theme change. In
  // CrOS notification, if `accent_color_id` is provided, `accent_color` will
  // not be used.
  std::optional<ui::ColorId> accent_color_id;

  // Controls whether a settings button should appear on the notification. See
  // enum definition. TODO(estade): turn this into a boolean. See
  // crbug.com/780342
  SettingsButtonHandler settings_button_handler = SettingsButtonHandler::NONE;

  // Controls whether a snooze button should appear on the notification.
  bool should_show_snooze_button = false;

  // If true, instead of using an app-themed accent color for views containing
  // text, use the style's default. This affects the action buttons and title.
  bool ignore_accent_color_for_text = false;

  FullscreenVisibility fullscreen_visibility = FullscreenVisibility::NONE;

  // Whether the notification should be removed from the MessageCenter when it's
  // clicked after the delegate has been executed (if any).
  bool remove_on_click = false;

  // Changes notification behavior and look depending on the selected scenario
  // and on whether the notification was created by an installed origin.
  NotificationScenario scenario = NotificationScenario::DEFAULT;
};

class MESSAGE_CENTER_PUBLIC_EXPORT Notification {
 public:
  // Creates a new notification.
  //
  // |type|: Type of the notification that dictates the layout.
  // |id|: Identifier of the notification. Showing a notification that shares
  //       its profile and identifier with an already visible notification will
  //       replace the former one
  // |title|: Title of the notification.
  // |message|: Body text of the notification. May not be used for certain
  //            values of |type|, for example list-style notifications.
  // |icon|: Icon to show alongside of the notification.
  // |display_source|: Textual representation of who's shown the notification.
  // |origin_url|: URL of the website responsible for showing the notification.
  // |notifier_id|: NotifierId instance representing the system responsible for
  //                showing the notification.
  // |optional_fields|: Rich data that can be used to assign more elaborate
  //                    features to notifications.
  // |delegate|: Delegate that will influence the behaviour of this notification
  //             and receives events on its behalf. May be omitted.
  Notification(NotificationType type,
               const std::string& id,
               const std::u16string& title,
               const std::u16string& message,
               const ui::ImageModel& icon,
               const std::u16string& display_source,
               const GURL& origin_url,
               const NotifierId& notifier_id,
               const RichNotificationData& optional_fields,
               scoped_refptr<NotificationDelegate> delegate);

  // Creates a copy of the |other| notification. The delegate, if any, will be
  // identical for both the Notification instances. The |id| of the notification
  // will be replaced by the given value.
  Notification(const std::string& id, const Notification& other);

  // Creates a copy of the |other| notification. The delegate will be replaced
  // by |delegate|.
  Notification(scoped_refptr<NotificationDelegate> delegate,
               const Notification& other);

  // Creates a copy of the |other| notification. The delegate, if any, will be
  // identical for both the Notification instances.
  Notification(const Notification& other);

  Notification(Notification&& other);
  Notification& operator=(const Notification& other);
  Notification& operator=(Notification&& other);

  virtual ~Notification();

  // Performs a deep copy of |notification|, including images and (optionally)
  // the body image, small image, and icon images which are not supported on all
  // platforms.
  static std::unique_ptr<Notification> DeepCopy(
      const Notification& notification,
      const ui::ColorProvider* color_provider,
      bool include_body_image,
      bool include_small_image,
      bool include_icon_images);

  NotificationType type() const { return type_; }
  void set_type(NotificationType type) { type_ = type; }

  // Uniquely identifies a notification in the message center. For
  // notification front ends that support multiple profiles, this id should
  // identify a unique profile + frontend_notification_id combination. You can
  // Use this id against the MessageCenter interface but not the
  // NotificationUIManager interface.
  const std::string& id() const { return id_; }

  const std::u16string& title() const { return title_; }
  void set_title(const std::u16string& title) { title_ = title; }

  const std::u16string& message() const { return message_; }
  void set_message(const std::u16string& message) { message_ = message; }

  // The origin URL of the script which requested the notification.
  // Can be empty if the notification is requested by an extension or
  // Chrome app.
  const GURL& origin_url() const { return origin_url_; }
  void set_origin_url(const GURL& origin_url) { origin_url_ = origin_url; }

  // A display string for the source of the notification.
  const std::u16string& display_source() const { return display_source_; }
  void set_display_source(const std::u16string& display_source) {
    display_source_ = display_source;
  }

  bool allow_group() const { return allow_group_; }
  void set_allow_group(bool allow_group) { allow_group_ = allow_group; }

  bool group_child() const { return group_child_; }
  bool group_parent() const { return group_parent_; }

  const NotifierId& notifier_id() const { return notifier_id_; }

  void set_profile_id(const std::string& profile_id) {
    notifier_id_.profile_id = profile_id;
  }

  // Begin unpacked values from optional_fields.
  int priority() const { return optional_fields_.priority; }
  void set_priority(int priority) { optional_fields_.priority = priority; }

  // This vibration_pattern property currently has no effect on
  // non-Android platforms.
  const std::vector<int>& vibration_pattern() const {
    return optional_fields_.vibration_pattern;
  }
  void set_vibration_pattern(const std::vector<int>& vibration_pattern) {
    optional_fields_.vibration_pattern = vibration_pattern;
  }

  // This property currently only works in platforms that support native
  // notifications.
  // It determines whether the sound and vibration effects should signal
  // if the notification is replacing another notification.
  bool renotify() const { return optional_fields_.renotify; }
  void set_renotify(bool renotify) { optional_fields_.renotify = renotify; }

  // This property currently has no effect on non-Android platforms.
  bool silent() const { return optional_fields_.silent; }
  void set_silent(bool silent) { optional_fields_.silent = silent; }

  base::Time timestamp() const { return optional_fields_.timestamp; }
  void set_timestamp(const base::Time& timestamp) {
    optional_fields_.timestamp = timestamp;
  }

  const std::u16string context_message() const {
    return optional_fields_.context_message;
  }

  void set_context_message(const std::u16string& context_message) {
    optional_fields_.context_message = context_message;
  }

  // Decides if the notification origin should be used as a context message
  bool UseOriginAsContextMessage() const;

  const std::vector<NotificationItem>& items() const {
    return optional_fields_.items;
  }
  void set_items(const std::vector<NotificationItem>& items) {
    optional_fields_.items = items;
  }

  int progress() const { return optional_fields_.progress; }
  void set_progress(int progress) { optional_fields_.progress = progress; }

  std::u16string progress_status() const {
    return optional_fields_.progress_status;
  }
  void set_progress_status(const std::u16string& progress_status) {
    optional_fields_.progress_status = progress_status;
  }

  // End unpacked values.

  // Images fetched asynchronously.
  ui::ImageModel icon() const { return icon_; }
  void set_icon(const ui::ImageModel& icon) { icon_ = icon; }

  const gfx::Image& image() const { return optional_fields_.image; }
  void SetImage(const gfx::Image& image);

#if BUILDFLAG(IS_CHROMEOS)
  void set_image_path(const base::FilePath& image_path) {
    optional_fields_.image_path = image_path;
  }
#endif

  const gfx::Image& small_image() const { return optional_fields_.small_image; }
  void SetSmallImage(const gfx::Image& image);

  bool small_image_needs_additional_masking() const {
    return optional_fields_.small_image_needs_additional_masking;
  }
  void set_small_image_needs_additional_masking(bool needs_additional_masking) {
    optional_fields_.small_image_needs_additional_masking =
        needs_additional_masking;
  }

  const gfx::VectorIcon& vector_small_image() const {
    return *optional_fields_.vector_small_image;
  }
  // Due to the restriction of CreateVectorIcon, this should be a pointer to
  // globally defined VectorIcon instance e.g. kNotificationCapsLockIcon.
  // See detailed comment in RichNotificationData::vector_small_image.
  void set_vector_small_image(const gfx::VectorIcon& image) {
    optional_fields_.vector_small_image = &image;
  }

  const gfx::VectorIcon& parent_vector_small_image() const {
    return *optional_fields_.parent_vector_small_image;
  }
  void set_parent_vector_small_image(const gfx::VectorIcon& image) {
    optional_fields_.parent_vector_small_image = &image;
  }

  // Mask the color of |small_image| to the given |color|.
  // If |vector_small_image| is available, it returns the vector image
  // filled by the |color|.
  // Otherwise, it uses alpha channel of the rasterized |small_image| for
  // masking.
  gfx::Image GenerateMaskedSmallIcon(int dip_size,
                                     SkColor mask_color,
                                     SkColor background_color,
                                     SkColor foreground_color) const;

  gfx::Image GetMaskedSmallImage(const gfx::ImageSkia& small_image,
                                 SkColor background_color,
                                 SkColor foreground_color) const;

  // Buttons, with icons fetched asynchronously.
  const std::vector<ButtonInfo>& buttons() const {
    return optional_fields_.buttons;
  }
  void set_buttons(const std::vector<ButtonInfo>& buttons) {
    optional_fields_.buttons = buttons;
  }
  void SetButtonIcon(size_t index, const gfx::Image& icon);

  // Used to keep the order of notifications with the same timestamp.
  // The notification with lesser serial_number is considered 'older'.
  unsigned serial_number() { return serial_number_; }

  // Gets and sets whether the notifiction should remain onscreen permanently.
  bool never_timeout() const { return optional_fields_.never_timeout; }
  void set_never_timeout(bool never_timeout) {
    optional_fields_.never_timeout = never_timeout;
  }

  bool pinned() const {
#if BUILDFLAG(IS_CHROMEOS)
    return optional_fields_.pinned;
#else
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
#if BUILDFLAG(IS_CHROMEOS)
  void set_pinned(bool pinned) { optional_fields_.pinned = pinned; }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Gets a text for spoken feedback.
  const std::u16string& accessible_name() const {
    return optional_fields_.accessible_name;
  }

  std::optional<SkColor> accent_color() const {
    return optional_fields_.accent_color;
  }
  void set_accent_color(SkColor accent_color) {
    optional_fields_.accent_color = accent_color;
  }

  std::optional<ui::ColorId> accent_color_id() const {
    return optional_fields_.accent_color_id;
  }
  void set_accent_color_id(ui::ColorId accent_color_id) {
    optional_fields_.accent_color_id = accent_color_id;
  }

  bool should_show_settings_button() const {
    return optional_fields_.settings_button_handler !=
           SettingsButtonHandler::NONE;
  }

  void set_settings_button_handler(SettingsButtonHandler handler) {
    optional_fields_.settings_button_handler = handler;
  }

  bool should_show_snooze_button() const {
    return optional_fields_.should_show_snooze_button;
  }

  FullscreenVisibility fullscreen_visibility() const {
    return optional_fields_.fullscreen_visibility;
  }
  void set_fullscreen_visibility(FullscreenVisibility visibility) {
    optional_fields_.fullscreen_visibility = visibility;
  }

  NotificationScenario scenario() const { return optional_fields_.scenario; }
  void set_scenario(NotificationScenario scenario) {
    optional_fields_.scenario = scenario;
  }

  NotificationDelegate* delegate() const { return delegate_.get(); }

  const RichNotificationData& rich_notification_data() const {
    return optional_fields_;
  }

  void set_delegate(scoped_refptr<NotificationDelegate> delegate) {
    DCHECK(!delegate_);
    delegate_ = delegate;
  }

  // Set the priority to SYSTEM. The system priority user needs to call this
  // method explicitly, to avoid setting it accidentally.
  void SetSystemPriority();

  // Set the notification as a group child. This means it can only be displayed
  // inside a group notification.
  void SetGroupChild();

  // Set the notification as a group parent. This means the message view
  // associated with this notification will act as a container for all
  // notifications that are part of its group.
  void SetGroupParent();

  // Set `group_child_` to false so it's back to it's
  // default state.
  void ClearGroupChild();

  // Set `group_parent_` to false so it's back to it's
  // default state.
  void ClearGroupParent();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void set_system_notification_warning_level(
      SystemNotificationWarningLevel warning_level) {
    system_notification_warning_level_ = warning_level;
  }

  SystemNotificationWarningLevel system_notification_warning_level() const {
    return system_notification_warning_level_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const std::string& custom_view_type() const { return custom_view_type_; }
  void set_custom_view_type(const std::string& custom_view_type) {
    DCHECK_EQ(type(), NotificationType::NOTIFICATION_TYPE_CUSTOM);
    custom_view_type_ = custom_view_type;
  }

  // Gets the element ID that should be used for the view that hosts this
  // notification.
  ui::ElementIdentifier host_view_element_id() const {
    return host_view_element_id_;
  }
  void set_host_view_element_id(
      const ui::ElementIdentifier host_view_element_id) {
    host_view_element_id_ = host_view_element_id;
  }

 protected:
  // The type of notification we'd like displayed.
  NotificationType type_;

  std::string id_;
  std::u16string title_;
  std::u16string message_;

  // Image data for the associated icon, used by Ash when available.
  ui::ImageModel icon_;

  // The display string for the source of the notification.  Could be
  // the same as |origin_url_|, or the name of an extension.
  // Expected to be a localized user facing string.
  std::u16string display_source_;

 private:
  // The origin URL of the script which requested the notification.
  // Can be empty if requested through a chrome app or extension or if
  // it's a system notification.
  GURL origin_url_;
  NotifierId notifier_id_;
  RichNotificationData optional_fields_;

  // TODO(estade): these book-keeping fields should be moved into
  // NotificationList.
  unsigned serial_number_;

  // If set to true the notification can be displayed inside a group
  // notification.
  bool allow_group_ = false;

  // If set to true the notification should not be displayed separately
  // but inside a group notification.
  bool group_child_ = false;

  // If set to true the message view associated with this notification will
  // be responsible to display all notifications that are part of its group.
  bool group_parent_ = false;

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationDelegate> delegate_;

  // For custom notifications this determines which factory will be used for
  // creating the view for this notification. The type should match the type
  // used to register the factory in MessageViewFactory.
  std::string custom_view_type_;

  // The value that should be used for the element ID of the view that hosts
  // this notification.
  ui::ElementIdentifier host_view_element_id_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The warning level of a system notification.
  SystemNotificationWarningLevel system_notification_warning_level_ =
      SystemNotificationWarningLevel::NORMAL;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_H_
