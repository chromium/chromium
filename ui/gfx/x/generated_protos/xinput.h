// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#ifndef UI_GFX_X_GENERATED_PROTOS_XINPUT_H_
#define UI_GFX_X_GENERATED_PROTOS_XINPUT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "ui/gfx/x/xproto_types.h"
#include "xfixes.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Input {
 public:
  static constexpr unsigned major_version = 2;
  static constexpr unsigned minor_version = 4;

  Input(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class EventClass : uint32_t {};

  enum class KeyCode : uint8_t {};

  enum class Fp1616 : int32_t {};

  enum class DeviceUse : int {
    IsXPointer = 0,
    IsXKeyboard = 1,
    IsXExtensionDevice = 2,
    IsXExtensionKeyboard = 3,
    IsXExtensionPointer = 4,
  };

  enum class InputClass : int {
    Key = 0,
    Button = 1,
    Valuator = 2,
    Feedback = 3,
    Proximity = 4,
    Focus = 5,
    Other = 6,
  };

  enum class ValuatorMode : int {
    Relative = 0,
    Absolute = 1,
  };

  enum class EventTypeBase : uint8_t {};

  enum class PropagateMode : int {
    AddToList = 0,
    DeleteFromList = 1,
  };

  enum class ModifierDevice : int {
    UseXKeyboard = 255,
  };

  enum class DeviceInputMode : int {
    AsyncThisDevice = 0,
    SyncThisDevice = 1,
    ReplayThisDevice = 2,
    AsyncOtherDevices = 3,
    AsyncAll = 4,
    SyncAll = 5,
  };

  enum class FeedbackClass : int {
    Keyboard = 0,
    Pointer = 1,
    String = 2,
    Integer = 3,
    Led = 4,
    Bell = 5,
  };

  enum class ChangeFeedbackControlMask : int {
    KeyClickPercent = 1 << 0,
    Percent = 1 << 1,
    Pitch = 1 << 2,
    Duration = 1 << 3,
    Led = 1 << 4,
    LedMode = 1 << 5,
    Key = 1 << 6,
    AutoRepeatMode = 1 << 7,
    String = 1 << 0,
    Integer = 1 << 0,
    AccelNum = 1 << 0,
    AccelDenom = 1 << 1,
    Threshold = 1 << 2,
  };

  enum class ValuatorStateModeMask : int {
    DeviceModeAbsolute = 1 << 0,
    OutOfProximity = 1 << 1,
  };

  enum class DeviceControl : int {
    resolution = 1,
    abs_calib = 2,
    core = 3,
    enable = 4,
    abs_area = 5,
  };

  enum class PropertyFormat : int {
    c_8Bits = 8,
    c_16Bits = 16,
    c_32Bits = 32,
  };

  enum class DeviceId : uint16_t {
    All = 0,
    AllMaster = 1,
  };

  enum class HierarchyChangeType : int {
    AddMaster = 1,
    RemoveMaster = 2,
    AttachSlave = 3,
    DetachSlave = 4,
  };

  enum class ChangeMode : int {
    Attach = 1,
    Float = 2,
  };

  enum class XIEventMask : int {
    DeviceChanged = 1 << 1,
    KeyPress = 1 << 2,
    KeyRelease = 1 << 3,
    ButtonPress = 1 << 4,
    ButtonRelease = 1 << 5,
    Motion = 1 << 6,
    Enter = 1 << 7,
    Leave = 1 << 8,
    FocusIn = 1 << 9,
    FocusOut = 1 << 10,
    Hierarchy = 1 << 11,
    Property = 1 << 12,
    RawKeyPress = 1 << 13,
    RawKeyRelease = 1 << 14,
    RawButtonPress = 1 << 15,
    RawButtonRelease = 1 << 16,
    RawMotion = 1 << 17,
    TouchBegin = 1 << 18,
    TouchUpdate = 1 << 19,
    TouchEnd = 1 << 20,
    TouchOwnership = 1 << 21,
    RawTouchBegin = 1 << 22,
    RawTouchUpdate = 1 << 23,
    RawTouchEnd = 1 << 24,
    BarrierHit = 1 << 25,
    BarrierLeave = 1 << 26,
  };

  enum class DeviceClassType : int {
    Key = 0,
    Button = 1,
    Valuator = 2,
    Scroll = 3,
    Touch = 8,
    Gesture = 9,
  };

  enum class DeviceType : int {
    MasterPointer = 1,
    MasterKeyboard = 2,
    SlavePointer = 3,
    SlaveKeyboard = 4,
    FloatingSlave = 5,
  };

  enum class ScrollFlags : int {
    NoEmulation = 1 << 0,
    Preferred = 1 << 1,
  };

  enum class ScrollType : int {
    Vertical = 1,
    Horizontal = 2,
  };

  enum class TouchMode : int {
    Direct = 1,
    Dependent = 2,
  };

  enum class GrabOwner : int {
    NoOwner = 0,
    Owner = 1,
  };

  enum class EventMode : int {
    AsyncDevice = 0,
    SyncDevice = 1,
    ReplayDevice = 2,
    AsyncPairedDevice = 3,
    AsyncPair = 4,
    SyncPair = 5,
    AcceptTouch = 6,
    RejectTouch = 7,
  };

  enum class GrabMode22 : int {
    Sync = 0,
    Async = 1,
    Touch = 2,
  };

  enum class GrabType : int {
    Button = 0,
    Keycode = 1,
    Enter = 2,
    FocusIn = 3,
    TouchBegin = 4,
    GesturePinchBegin = 5,
    GestureSwipeBegin = 6,
  };

  enum class ModifierMask : int {
    Any = 1 << 31,
  };

  enum class MoreEventsMask : int {
    MoreEvents = 1 << 7,
  };

  enum class ClassesReportedMask : int {
    OutOfProximity = 1 << 7,
    DeviceModeAbsolute = 1 << 6,
    ReportingValuators = 1 << 2,
    ReportingButtons = 1 << 1,
    ReportingKeys = 1 << 0,
  };

  enum class ChangeDevice : int {
    NewPointer = 0,
    NewKeyboard = 1,
  };

  enum class DeviceChange : int {
    Added = 0,
    Removed = 1,
    Enabled = 2,
    Disabled = 3,
    Unrecoverable = 4,
    ControlChanged = 5,
  };

  enum class ChangeReason : int {
    SlaveSwitch = 1,
    DeviceChange = 2,
  };

  enum class KeyEventFlags : int {
    KeyRepeat = 1 << 16,
  };

  enum class PointerEventFlags : int {
    PointerEmulated = 1 << 16,
  };

  enum class NotifyMode : int {
    Normal = 0,
    Grab = 1,
    Ungrab = 2,
    WhileGrabbed = 3,
    PassiveGrab = 4,
    PassiveUngrab = 5,
  };

  enum class NotifyDetail : int {
    Ancestor = 0,
    Virtual = 1,
    Inferior = 2,
    Nonlinear = 3,
    NonlinearVirtual = 4,
    Pointer = 5,
    PointerRoot = 6,
    None = 7,
  };

  enum class HierarchyMask : int {
    MasterAdded = 1 << 0,
    MasterRemoved = 1 << 1,
    SlaveAdded = 1 << 2,
    SlaveRemoved = 1 << 3,
    SlaveAttached = 1 << 4,
    SlaveDetached = 1 << 5,
    DeviceEnabled = 1 << 6,
    DeviceDisabled = 1 << 7,
  };

  enum class PropertyFlag : int {
    Deleted = 0,
    Created = 1,
    Modified = 2,
  };

  enum class TouchEventFlags : int {
    TouchPendingEnd = 1 << 16,
    TouchEmulatingPointer = 1 << 17,
  };

  enum class TouchOwnershipFlags : int {
    None = 0,
  };

  enum class BarrierFlags : int {
    PointerReleased = 1 << 0,
    DeviceIsGrabbed = 1 << 1,
  };

  enum class GesturePinchEventFlags : int {
    GesturePinchCancelled = 1 << 0,
  };

  enum class GestureSwipeEventFlags : int {
    GestureSwipeCancelled = 1 << 0,
  };

  struct Fp3232 {
    bool operator==(const Fp3232& other) const {
      return integral == other.integral && frac == other.frac;
    }

    int32_t integral{};
    uint32_t frac{};
  };

  struct DeviceInfo {
    bool operator==(const DeviceInfo& other) const {
      return device_type == other.device_type && device_id == other.device_id &&
             num_class_info == other.num_class_info &&
             device_use == other.device_use;
    }

    Atom device_type{};
    uint8_t device_id{};
    uint8_t num_class_info{};
    DeviceUse device_use{};
  };

  struct KeyInfo {
    bool operator==(const KeyInfo& other) const {
      return class_id == other.class_id && len == other.len &&
             min_keycode == other.min_keycode &&
             max_keycode == other.max_keycode && num_keys == other.num_keys;
    }

    InputClass class_id{};
    uint8_t len{};
    KeyCode min_keycode{};
    KeyCode max_keycode{};
    uint16_t num_keys{};
  };

  struct ButtonInfo {
    bool operator==(const ButtonInfo& other) const {
      return class_id == other.class_id && len == other.len &&
             num_buttons == other.num_buttons;
    }

    InputClass class_id{};
    uint8_t len{};
    uint16_t num_buttons{};
  };

  struct AxisInfo {
    bool operator==(const AxisInfo& other) const {
      return resolution == other.resolution && minimum == other.minimum &&
             maximum == other.maximum;
    }

    uint32_t resolution{};
    int32_t minimum{};
    int32_t maximum{};
  };

  struct ValuatorInfo {
    bool operator==(const ValuatorInfo& other) const {
      return class_id == other.class_id && len == other.len &&
             mode == other.mode && motion_size == other.motion_size &&
             axes == other.axes;
    }

    InputClass class_id{};
    uint8_t len{};
    ValuatorMode mode{};
    uint32_t motion_size{};
    std::vector<AxisInfo> axes{};
  };

  struct InputInfo {
    uint8_t len{};
    struct Key {
      KeyCode min_keycode{};
      KeyCode max_keycode{};
      uint16_t num_keys{};
    };
    struct Button {
      uint16_t num_buttons{};
    };
    struct Valuator {
      ValuatorMode mode{};
      uint32_t motion_size{};
      std::vector<AxisInfo> axes{};
    };
    std::optional<Key> key{};
    std::optional<Button> button{};
    std::optional<Valuator> valuator{};
  };

  struct DeviceName {
    bool operator==(const DeviceName& other) const {
      return string == other.string;
    }

    std::string string{};
  };

  struct InputClassInfo {
    bool operator==(const InputClassInfo& other) const {
      return class_id == other.class_id &&
             event_type_base == other.event_type_base;
    }

    InputClass class_id{};
    EventTypeBase event_type_base{};
  };

  struct DeviceTimeCoord {
    bool operator==(const DeviceTimeCoord& other) const {
      return time == other.time && axisvalues == other.axisvalues;
    }

    Time time{};
    std::vector<int32_t> axisvalues{};
  };

  struct KbdFeedbackState {
    bool operator==(const KbdFeedbackState& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && pitch == other.pitch &&
             duration == other.duration && led_mask == other.led_mask &&
             led_values == other.led_values &&
             global_auto_repeat == other.global_auto_repeat &&
             click == other.click && percent == other.percent &&
             auto_repeats == other.auto_repeats;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint16_t pitch{};
    uint16_t duration{};
    uint32_t led_mask{};
    uint32_t led_values{};
    uint8_t global_auto_repeat{};
    uint8_t click{};
    uint8_t percent{};
    std::array<uint8_t, 32> auto_repeats{};
  };

  struct PtrFeedbackState {
    bool operator==(const PtrFeedbackState& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && accel_num == other.accel_num &&
             accel_denom == other.accel_denom && threshold == other.threshold;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint16_t accel_num{};
    uint16_t accel_denom{};
    uint16_t threshold{};
  };

  struct IntegerFeedbackState {
    bool operator==(const IntegerFeedbackState& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && resolution == other.resolution &&
             min_value == other.min_value && max_value == other.max_value;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint32_t resolution{};
    int32_t min_value{};
    int32_t max_value{};
  };

  struct StringFeedbackState {
    bool operator==(const StringFeedbackState& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && max_symbols == other.max_symbols &&
             keysyms == other.keysyms;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint16_t max_symbols{};
    std::vector<KeySym> keysyms{};
  };

  struct BellFeedbackState {
    bool operator==(const BellFeedbackState& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && percent == other.percent &&
             pitch == other.pitch && duration == other.duration;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint8_t percent{};
    uint16_t pitch{};
    uint16_t duration{};
  };

  struct LedFeedbackState {
    bool operator==(const LedFeedbackState& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && led_mask == other.led_mask &&
             led_values == other.led_values;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint32_t led_mask{};
    uint32_t led_values{};
  };

  struct FeedbackState {
    uint8_t feedback_id{};
    uint16_t len{};
    struct Keyboard {
      uint16_t pitch{};
      uint16_t duration{};
      uint32_t led_mask{};
      uint32_t led_values{};
      uint8_t global_auto_repeat{};
      uint8_t click{};
      uint8_t percent{};
      std::array<uint8_t, 32> auto_repeats{};
    };
    struct Pointer {
      uint16_t accel_num{};
      uint16_t accel_denom{};
      uint16_t threshold{};
    };
    struct String {
      uint16_t max_symbols{};
      std::vector<KeySym> keysyms{};
    };
    struct Integer {
      uint32_t resolution{};
      int32_t min_value{};
      int32_t max_value{};
    };
    struct Led {
      uint32_t led_mask{};
      uint32_t led_values{};
    };
    struct Bell {
      uint8_t percent{};
      uint16_t pitch{};
      uint16_t duration{};
    };
    std::optional<Keyboard> keyboard{};
    std::optional<Pointer> pointer{};
    std::optional<String> string{};
    std::optional<Integer> integer{};
    std::optional<Led> led{};
    std::optional<Bell> bell{};
  };

  struct KbdFeedbackCtl {
    bool operator==(const KbdFeedbackCtl& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && key == other.key &&
             auto_repeat_mode == other.auto_repeat_mode &&
             key_click_percent == other.key_click_percent &&
             bell_percent == other.bell_percent &&
             bell_pitch == other.bell_pitch &&
             bell_duration == other.bell_duration &&
             led_mask == other.led_mask && led_values == other.led_values;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    KeyCode key{};
    uint8_t auto_repeat_mode{};
    int8_t key_click_percent{};
    int8_t bell_percent{};
    int16_t bell_pitch{};
    int16_t bell_duration{};
    uint32_t led_mask{};
    uint32_t led_values{};
  };

  struct PtrFeedbackCtl {
    bool operator==(const PtrFeedbackCtl& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && num == other.num && denom == other.denom &&
             threshold == other.threshold;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    int16_t num{};
    int16_t denom{};
    int16_t threshold{};
  };

  struct IntegerFeedbackCtl {
    bool operator==(const IntegerFeedbackCtl& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && int_to_display == other.int_to_display;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    int32_t int_to_display{};
  };

  struct StringFeedbackCtl {
    bool operator==(const StringFeedbackCtl& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && keysyms == other.keysyms;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    std::vector<KeySym> keysyms{};
  };

  struct BellFeedbackCtl {
    bool operator==(const BellFeedbackCtl& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && percent == other.percent &&
             pitch == other.pitch && duration == other.duration;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    int8_t percent{};
    int16_t pitch{};
    int16_t duration{};
  };

  struct LedFeedbackCtl {
    bool operator==(const LedFeedbackCtl& other) const {
      return class_id == other.class_id && feedback_id == other.feedback_id &&
             len == other.len && led_mask == other.led_mask &&
             led_values == other.led_values;
    }

    FeedbackClass class_id{};
    uint8_t feedback_id{};
    uint16_t len{};
    uint32_t led_mask{};
    uint32_t led_values{};
  };

  struct FeedbackCtl {
    uint8_t feedback_id{};
    uint16_t len{};
    struct Keyboard {
      KeyCode key{};
      uint8_t auto_repeat_mode{};
      int8_t key_click_percent{};
      int8_t bell_percent{};
      int16_t bell_pitch{};
      int16_t bell_duration{};
      uint32_t led_mask{};
      uint32_t led_values{};
    };
    struct Pointer {
      int16_t num{};
      int16_t denom{};
      int16_t threshold{};
    };
    struct String {
      std::vector<KeySym> keysyms{};
    };
    struct Integer {
      int32_t int_to_display{};
    };
    struct Led {
      uint32_t led_mask{};
      uint32_t led_values{};
    };
    struct Bell {
      int8_t percent{};
      int16_t pitch{};
      int16_t duration{};
    };
    std::optional<Keyboard> keyboard{};
    std::optional<Pointer> pointer{};
    std::optional<String> string{};
    std::optional<Integer> integer{};
    std::optional<Led> led{};
    std::optional<Bell> bell{};
  };

  struct KeyState {
    bool operator==(const KeyState& other) const {
      return class_id == other.class_id && len == other.len &&
             num_keys == other.num_keys && keys == other.keys;
    }

    InputClass class_id{};
    uint8_t len{};
    uint8_t num_keys{};
    std::array<uint8_t, 32> keys{};
  };

  struct ButtonState {
    bool operator==(const ButtonState& other) const {
      return class_id == other.class_id && len == other.len &&
             num_buttons == other.num_buttons && buttons == other.buttons;
    }

    InputClass class_id{};
    uint8_t len{};
    uint8_t num_buttons{};
    std::array<uint8_t, 32> buttons{};
  };

  struct ValuatorState {
    bool operator==(const ValuatorState& other) const {
      return class_id == other.class_id && len == other.len &&
             mode == other.mode && valuators == other.valuators;
    }

    InputClass class_id{};
    uint8_t len{};
    ValuatorStateModeMask mode{};
    std::vector<int32_t> valuators{};
  };

  struct InputState {
    uint8_t len{};
    struct Key {
      uint8_t num_keys{};
      std::array<uint8_t, 32> keys{};
    };
    struct Button {
      uint8_t num_buttons{};
      std::array<uint8_t, 32> buttons{};
    };
    struct Valuator {
      ValuatorStateModeMask mode{};
      std::vector<int32_t> valuators{};
    };
    std::optional<Key> key{};
    std::optional<Button> button{};
    std::optional<Valuator> valuator{};
  };

  struct DeviceResolutionState {
    bool operator==(const DeviceResolutionState& other) const {
      return control_id == other.control_id && len == other.len &&
             resolution_values == other.resolution_values &&
             resolution_min == other.resolution_min &&
             resolution_max == other.resolution_max;
    }

    DeviceControl control_id{};
    uint16_t len{};
    std::vector<uint32_t> resolution_values{};
    std::vector<uint32_t> resolution_min{};
    std::vector<uint32_t> resolution_max{};
  };

  struct DeviceAbsCalibState {
    bool operator==(const DeviceAbsCalibState& other) const {
      return control_id == other.control_id && len == other.len &&
             min_x == other.min_x && max_x == other.max_x &&
             min_y == other.min_y && max_y == other.max_y &&
             flip_x == other.flip_x && flip_y == other.flip_y &&
             rotation == other.rotation &&
             button_threshold == other.button_threshold;
    }

    DeviceControl control_id{};
    uint16_t len{};
    int32_t min_x{};
    int32_t max_x{};
    int32_t min_y{};
    int32_t max_y{};
    uint32_t flip_x{};
    uint32_t flip_y{};
    uint32_t rotation{};
    uint32_t button_threshold{};
  };

  struct DeviceAbsAreaState {
    bool operator==(const DeviceAbsAreaState& other) const {
      return control_id == other.control_id && len == other.len &&
             offset_x == other.offset_x && offset_y == other.offset_y &&
             width == other.width && height == other.height &&
             screen == other.screen && following == other.following;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint32_t offset_x{};
    uint32_t offset_y{};
    uint32_t width{};
    uint32_t height{};
    uint32_t screen{};
    uint32_t following{};
  };

  struct DeviceCoreState {
    bool operator==(const DeviceCoreState& other) const {
      return control_id == other.control_id && len == other.len &&
             status == other.status && iscore == other.iscore;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint8_t status{};
    uint8_t iscore{};
  };

  struct DeviceEnableState {
    bool operator==(const DeviceEnableState& other) const {
      return control_id == other.control_id && len == other.len &&
             enable == other.enable;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint8_t enable{};
  };

  struct DeviceState {
    uint16_t len{};
    struct Resolution {
      std::vector<uint32_t> resolution_values{};
      std::vector<uint32_t> resolution_min{};
      std::vector<uint32_t> resolution_max{};
    };
    struct AbsCalib {
      int32_t min_x{};
      int32_t max_x{};
      int32_t min_y{};
      int32_t max_y{};
      uint32_t flip_x{};
      uint32_t flip_y{};
      uint32_t rotation{};
      uint32_t button_threshold{};
    };
    struct Core {
      uint8_t status{};
      uint8_t iscore{};
    };
    struct Enable {
      uint8_t enable{};
    };
    struct AbsArea {
      uint32_t offset_x{};
      uint32_t offset_y{};
      uint32_t width{};
      uint32_t height{};
      uint32_t screen{};
      uint32_t following{};
    };
    std::optional<Resolution> resolution{};
    std::optional<AbsCalib> abs_calib{};
    std::optional<Core> core{};
    std::optional<Enable> enable{};
    std::optional<AbsArea> abs_area{};
  };

  struct DeviceResolutionCtl {
    bool operator==(const DeviceResolutionCtl& other) const {
      return control_id == other.control_id && len == other.len &&
             first_valuator == other.first_valuator &&
             resolution_values == other.resolution_values;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint8_t first_valuator{};
    std::vector<uint32_t> resolution_values{};
  };

  struct DeviceAbsCalibCtl {
    bool operator==(const DeviceAbsCalibCtl& other) const {
      return control_id == other.control_id && len == other.len &&
             min_x == other.min_x && max_x == other.max_x &&
             min_y == other.min_y && max_y == other.max_y &&
             flip_x == other.flip_x && flip_y == other.flip_y &&
             rotation == other.rotation &&
             button_threshold == other.button_threshold;
    }

    DeviceControl control_id{};
    uint16_t len{};
    int32_t min_x{};
    int32_t max_x{};
    int32_t min_y{};
    int32_t max_y{};
    uint32_t flip_x{};
    uint32_t flip_y{};
    uint32_t rotation{};
    uint32_t button_threshold{};
  };

  struct DeviceAbsAreaCtrl {
    bool operator==(const DeviceAbsAreaCtrl& other) const {
      return control_id == other.control_id && len == other.len &&
             offset_x == other.offset_x && offset_y == other.offset_y &&
             width == other.width && height == other.height &&
             screen == other.screen && following == other.following;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint32_t offset_x{};
    uint32_t offset_y{};
    int32_t width{};
    int32_t height{};
    int32_t screen{};
    uint32_t following{};
  };

  struct DeviceCoreCtrl {
    bool operator==(const DeviceCoreCtrl& other) const {
      return control_id == other.control_id && len == other.len &&
             status == other.status;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint8_t status{};
  };

  struct DeviceEnableCtrl {
    bool operator==(const DeviceEnableCtrl& other) const {
      return control_id == other.control_id && len == other.len &&
             enable == other.enable;
    }

    DeviceControl control_id{};
    uint16_t len{};
    uint8_t enable{};
  };

  struct DeviceCtl {
    uint16_t len{};
    struct Resolution {
      uint8_t first_valuator{};
      std::vector<uint32_t> resolution_values{};
    };
    struct AbsCalib {
      int32_t min_x{};
      int32_t max_x{};
      int32_t min_y{};
      int32_t max_y{};
      uint32_t flip_x{};
      uint32_t flip_y{};
      uint32_t rotation{};
      uint32_t button_threshold{};
    };
    struct Core {
      uint8_t status{};
    };
    struct Enable {
      uint8_t enable{};
    };
    struct AbsArea {
      uint32_t offset_x{};
      uint32_t offset_y{};
      int32_t width{};
      int32_t height{};
      int32_t screen{};
      uint32_t following{};
    };
    std::optional<Resolution> resolution{};
    std::optional<AbsCalib> abs_calib{};
    std::optional<Core> core{};
    std::optional<Enable> enable{};
    std::optional<AbsArea> abs_area{};
  };

  struct GroupInfo {
    bool operator==(const GroupInfo& other) const {
      return base == other.base && latched == other.latched &&
             locked == other.locked && effective == other.effective;
    }

    uint8_t base{};
    uint8_t latched{};
    uint8_t locked{};
    uint8_t effective{};
  };

  struct ModifierInfo {
    bool operator==(const ModifierInfo& other) const {
      return base == other.base && latched == other.latched &&
             locked == other.locked && effective == other.effective;
    }

    uint32_t base{};
    uint32_t latched{};
    uint32_t locked{};
    uint32_t effective{};
  };

  struct AddMaster {
    bool operator==(const AddMaster& other) const {
      return type == other.type && len == other.len &&
             send_core == other.send_core && enable == other.enable &&
             name == other.name;
    }

    HierarchyChangeType type{};
    uint16_t len{};
    uint8_t send_core{};
    uint8_t enable{};
    std::string name{};
  };

  struct RemoveMaster {
    bool operator==(const RemoveMaster& other) const {
      return type == other.type && len == other.len &&
             deviceid == other.deviceid && return_mode == other.return_mode &&
             return_pointer == other.return_pointer &&
             return_keyboard == other.return_keyboard;
    }

    HierarchyChangeType type{};
    uint16_t len{};
    DeviceId deviceid{};
    ChangeMode return_mode{};
    DeviceId return_pointer{};
    DeviceId return_keyboard{};
  };

  struct AttachSlave {
    bool operator==(const AttachSlave& other) const {
      return type == other.type && len == other.len &&
             deviceid == other.deviceid && master == other.master;
    }

    HierarchyChangeType type{};
    uint16_t len{};
    DeviceId deviceid{};
    DeviceId master{};
  };

  struct DetachSlave {
    bool operator==(const DetachSlave& other) const {
      return type == other.type && len == other.len &&
             deviceid == other.deviceid;
    }

    HierarchyChangeType type{};
    uint16_t len{};
    DeviceId deviceid{};
  };

  struct HierarchyChange {
    uint16_t len{};
    struct AddMaster {
      uint8_t send_core{};
      uint8_t enable{};
      std::string name{};
    };
    struct RemoveMaster {
      DeviceId deviceid{};
      ChangeMode return_mode{};
      DeviceId return_pointer{};
      DeviceId return_keyboard{};
    };
    struct AttachSlave {
      DeviceId deviceid{};
      DeviceId master{};
    };
    struct DetachSlave {
      DeviceId deviceid{};
    };
    std::optional<AddMaster> add_master{};
    std::optional<RemoveMaster> remove_master{};
    std::optional<AttachSlave> attach_slave{};
    std::optional<DetachSlave> detach_slave{};
  };

  struct EventMask {
    bool operator==(const EventMask& other) const {
      return deviceid == other.deviceid && mask == other.mask;
    }

    DeviceId deviceid{};
    std::vector<XIEventMask> mask{};
  };

  struct ButtonClass {
    bool operator==(const ButtonClass& other) const {
      return type == other.type && len == other.len &&
             sourceid == other.sourceid && state == other.state &&
             labels == other.labels;
    }

    DeviceClassType type{};
    uint16_t len{};
    DeviceId sourceid{};
    std::vector<uint32_t> state{};
    std::vector<Atom> labels{};
  };

  struct KeyClass {
    bool operator==(const KeyClass& other) const {
      return type == other.type && len == other.len &&
             sourceid == other.sourceid && keys == other.keys;
    }

    DeviceClassType type{};
    uint16_t len{};
    DeviceId sourceid{};
    std::vector<uint32_t> keys{};
  };

  struct ScrollClass {
    bool operator==(const ScrollClass& other) const {
      return type == other.type && len == other.len &&
             sourceid == other.sourceid && number == other.number &&
             scroll_type == other.scroll_type && flags == other.flags &&
             increment == other.increment;
    }

    DeviceClassType type{};
    uint16_t len{};
    DeviceId sourceid{};
    uint16_t number{};
    ScrollType scroll_type{};
    ScrollFlags flags{};
    Fp3232 increment{};
  };

  struct TouchClass {
    bool operator==(const TouchClass& other) const {
      return type == other.type && len == other.len &&
             sourceid == other.sourceid && mode == other.mode &&
             num_touches == other.num_touches;
    }

    DeviceClassType type{};
    uint16_t len{};
    DeviceId sourceid{};
    TouchMode mode{};
    uint8_t num_touches{};
  };

  struct GestureClass {
    bool operator==(const GestureClass& other) const {
      return type == other.type && len == other.len &&
             sourceid == other.sourceid && num_touches == other.num_touches;
    }

    DeviceClassType type{};
    uint16_t len{};
    DeviceId sourceid{};
    uint8_t num_touches{};
  };

  struct ValuatorClass {
    bool operator==(const ValuatorClass& other) const {
      return type == other.type && len == other.len &&
             sourceid == other.sourceid && number == other.number &&
             label == other.label && min == other.min && max == other.max &&
             value == other.value && resolution == other.resolution &&
             mode == other.mode;
    }

    DeviceClassType type{};
    uint16_t len{};
    DeviceId sourceid{};
    uint16_t number{};
    Atom label{};
    Fp3232 min{};
    Fp3232 max{};
    Fp3232 value{};
    uint32_t resolution{};
    ValuatorMode mode{};
  };

  struct DeviceClass {
    uint16_t len{};
    DeviceId sourceid{};
    struct Key {
      std::vector<uint32_t> keys{};
    };
    struct Button {
      std::vector<uint32_t> state{};
      std::vector<Atom> labels{};
    };
    struct Valuator {
      uint16_t number{};
      Atom label{};
      Fp3232 min{};
      Fp3232 max{};
      Fp3232 value{};
      uint32_t resolution{};
      ValuatorMode mode{};
    };
    struct Scroll {
      uint16_t number{};
      ScrollType scroll_type{};
      ScrollFlags flags{};
      Fp3232 increment{};
    };
    struct Touch {
      TouchMode mode{};
      uint8_t num_touches{};
    };
    struct Gesture {
      uint8_t num_touches{};
    };
    std::optional<Key> key{};
    std::optional<Button> button{};
    std::optional<Valuator> valuator{};
    std::optional<Scroll> scroll{};
    std::optional<Touch> touch{};
    std::optional<Gesture> gesture{};
  };

  struct XIDeviceInfo {
    DeviceId deviceid{};
    DeviceType type{};
    DeviceId attachment{};
    uint8_t enabled{};
    std::string name{};
    std::vector<DeviceClass> classes{};
  };

  struct GrabModifierInfo {
    bool operator==(const GrabModifierInfo& other) const {
      return modifiers == other.modifiers && status == other.status;
    }

    uint32_t modifiers{};
    GrabStatus status{};
  };

  struct BarrierReleasePointerInfo {
    bool operator==(const BarrierReleasePointerInfo& other) const {
      return deviceid == other.deviceid && barrier == other.barrier &&
             eventid == other.eventid;
    }

    DeviceId deviceid{};
    XFixes::Barrier barrier{};
    uint32_t eventid{};
  };

  struct DeviceValuatorEvent {
    static constexpr uint8_t type_id = 12;
    static constexpr uint8_t opcode = 0;
    uint8_t device_id{};
    uint16_t sequence{};
    uint16_t device_state{};
    uint8_t num_valuators{};
    uint8_t first_valuator{};
    std::array<int32_t, 6> valuators{};
  };

  struct LegacyDeviceEvent {
    static constexpr uint8_t type_id = 13;
    enum Opcode {
      DeviceKeyPress = 1,
      DeviceKeyRelease = 2,
      DeviceButtonPress = 3,
      DeviceButtonRelease = 4,
      DeviceMotionNotify = 5,
      ProximityIn = 8,
      ProximityOut = 9,
    } opcode{};
    uint8_t detail{};
    uint16_t sequence{};
    Time time{};
    Window root{};
    Window event{};
    Window child{};
    int16_t root_x{};
    int16_t root_y{};
    int16_t event_x{};
    int16_t event_y{};
    KeyButMask state{};
    uint8_t same_screen{};
    uint8_t device_id{};
  };

  struct DeviceFocusEvent {
    static constexpr uint8_t type_id = 14;
    enum Opcode {
      In = 6,
      Out = 7,
    } opcode{};
    x11::NotifyDetail detail{};
    uint16_t sequence{};
    Time time{};
    Window window{};
    x11::NotifyMode mode{};
    uint8_t device_id{};
  };

  struct DeviceStateNotifyEvent {
    static constexpr uint8_t type_id = 15;
    static constexpr uint8_t opcode = 10;
    uint8_t device_id{};
    uint16_t sequence{};
    Time time{};
    uint8_t num_keys{};
    uint8_t num_buttons{};
    uint8_t num_valuators{};
    ClassesReportedMask classes_reported{};
    std::array<uint8_t, 4> buttons{};
    std::array<uint8_t, 4> keys{};
    std::array<uint32_t, 3> valuators{};
  };

  struct DeviceMappingNotifyEvent {
    static constexpr uint8_t type_id = 16;
    static constexpr uint8_t opcode = 11;
    uint8_t device_id{};
    uint16_t sequence{};
    Mapping request{};
    KeyCode first_keycode{};
    uint8_t count{};
    Time time{};
  };

  struct ChangeDeviceNotifyEvent {
    static constexpr uint8_t type_id = 17;
    static constexpr uint8_t opcode = 12;
    uint8_t device_id{};
    uint16_t sequence{};
    Time time{};
    ChangeDevice request{};
  };

  struct DeviceKeyStateNotifyEvent {
    static constexpr uint8_t type_id = 18;
    static constexpr uint8_t opcode = 13;
    uint8_t device_id{};
    uint16_t sequence{};
    std::array<uint8_t, 28> keys{};
  };

  struct DeviceButtonStateNotifyEvent {
    static constexpr uint8_t type_id = 19;
    static constexpr uint8_t opcode = 14;
    uint8_t device_id{};
    uint16_t sequence{};
    std::array<uint8_t, 28> buttons{};
  };

  struct DevicePresenceNotifyEvent {
    static constexpr uint8_t type_id = 20;
    static constexpr uint8_t opcode = 15;
    uint16_t sequence{};
    Time time{};
    DeviceChange devchange{};
    uint8_t device_id{};
    uint16_t control{};
  };

  struct DevicePropertyNotifyEvent {
    static constexpr uint8_t type_id = 21;
    static constexpr uint8_t opcode = 16;
    Property state{};
    uint16_t sequence{};
    Time time{};
    Atom property{};
    uint8_t device_id{};
  };

  struct DeviceChangedEvent {
    static constexpr uint8_t type_id = 22;
    static constexpr uint8_t opcode = 1;
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    DeviceId sourceid{};
    ChangeReason reason{};
    std::vector<DeviceClass> classes{};
  };

  struct DeviceEvent {
    static constexpr uint8_t type_id = 23;
    enum Opcode {
      KeyPress = 2,
      KeyRelease = 3,
      ButtonPress = 4,
      ButtonRelease = 5,
      Motion = 6,
      TouchBegin = 18,
      TouchUpdate = 19,
      TouchEnd = 20,
    } opcode{};
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    uint32_t detail{};
    Window root{};
    Window event{};
    Window child{};
    Fp1616 root_x{};
    Fp1616 root_y{};
    Fp1616 event_x{};
    Fp1616 event_y{};
    DeviceId sourceid{};
    KeyEventFlags flags{};
    ModifierInfo mods{};
    GroupInfo group{};
    std::vector<uint32_t> button_mask{};
    std::vector<uint32_t> valuator_mask{};
    std::vector<Fp3232> axisvalues{};
  };

  struct CrossingEvent {
    static constexpr uint8_t type_id = 24;
    enum Opcode {
      Enter = 7,
      Leave = 8,
      FocusIn = 9,
      FocusOut = 10,
    } opcode{};
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    DeviceId sourceid{};
    NotifyMode mode{};
    NotifyDetail detail{};
    Window root{};
    Window event{};
    Window child{};
    Fp1616 root_x{};
    Fp1616 root_y{};
    Fp1616 event_x{};
    Fp1616 event_y{};
    uint8_t same_screen{};
    uint8_t focus{};
    ModifierInfo mods{};
    GroupInfo group{};
    std::vector<uint32_t> buttons{};
  };

  struct HierarchyInfo {
    bool operator==(const HierarchyInfo& other) const {
      return deviceid == other.deviceid && attachment == other.attachment &&
             type == other.type && enabled == other.enabled &&
             flags == other.flags;
    }

    DeviceId deviceid{};
    DeviceId attachment{};
    DeviceType type{};
    uint8_t enabled{};
    HierarchyMask flags{};
  };

  struct HierarchyEvent {
    static constexpr uint8_t type_id = 25;
    static constexpr uint8_t opcode = 11;
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    HierarchyMask flags{};
    std::vector<HierarchyInfo> infos{};
  };

  struct PropertyEvent {
    static constexpr uint8_t type_id = 26;
    static constexpr uint8_t opcode = 12;
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    Atom property{};
    PropertyFlag what{};
  };

  struct RawDeviceEvent {
    static constexpr uint8_t type_id = 27;
    enum Opcode {
      RawKeyPress = 13,
      RawKeyRelease = 14,
      RawButtonPress = 15,
      RawButtonRelease = 16,
      RawMotion = 17,
      RawTouchBegin = 22,
      RawTouchUpdate = 23,
      RawTouchEnd = 24,
    } opcode{};
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    uint32_t detail{};
    DeviceId sourceid{};
    KeyEventFlags flags{};
    std::vector<uint32_t> valuator_mask{};
    std::vector<Fp3232> axisvalues{};
    std::vector<Fp3232> axisvalues_raw{};
  };

  struct TouchOwnershipEvent {
    static constexpr uint8_t type_id = 28;
    static constexpr uint8_t opcode = 21;
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    uint32_t touchid{};
    Window root{};
    Window event{};
    Window child{};
    DeviceId sourceid{};
    TouchOwnershipFlags flags{};
  };

  struct BarrierEvent {
    static constexpr uint8_t type_id = 29;
    enum Opcode {
      Hit = 25,
      Leave = 26,
    } opcode{};
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    uint32_t eventid{};
    Window root{};
    Window event{};
    XFixes::Barrier barrier{};
    uint32_t dtime{};
    BarrierFlags flags{};
    DeviceId sourceid{};
    Fp1616 root_x{};
    Fp1616 root_y{};
    Fp3232 dx{};
    Fp3232 dy{};
  };

  struct GesturePinchEvent {
    static constexpr uint8_t type_id = 30;
    enum Opcode {
      Begin = 27,
      Update = 28,
      End = 29,
    } opcode{};
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    uint32_t detail{};
    Window root{};
    Window event{};
    Window child{};
    Fp1616 root_x{};
    Fp1616 root_y{};
    Fp1616 event_x{};
    Fp1616 event_y{};
    Fp1616 delta_x{};
    Fp1616 delta_y{};
    Fp1616 delta_unaccel_x{};
    Fp1616 delta_unaccel_y{};
    Fp1616 scale{};
    Fp1616 delta_angle{};
    DeviceId sourceid{};
    ModifierInfo mods{};
    GroupInfo group{};
    GesturePinchEventFlags flags{};
  };

  struct GestureSwipeEvent {
    static constexpr uint8_t type_id = 31;
    enum Opcode {
      Begin = 30,
      Update = 31,
      End = 32,
    } opcode{};
    uint16_t sequence{};
    DeviceId deviceid{};
    Time time{};
    uint32_t detail{};
    Window root{};
    Window event{};
    Window child{};
    Fp1616 root_x{};
    Fp1616 root_y{};
    Fp1616 event_x{};
    Fp1616 event_y{};
    Fp1616 delta_x{};
    Fp1616 delta_y{};
    Fp1616 delta_unaccel_x{};
    Fp1616 delta_unaccel_y{};
    DeviceId sourceid{};
    ModifierInfo mods{};
    GroupInfo group{};
    GestureSwipeEventFlags flags{};
  };

  using EventForSend = std::array<uint8_t, 32>;
  struct DeviceError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct EventError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ModeError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct DeviceBusyError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct ClassError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct GetExtensionVersionRequest {
    std::string name{};
  };

  struct GetExtensionVersionReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    uint16_t server_major{};
    uint16_t server_minor{};
    uint8_t present{};
  };

  using GetExtensionVersionResponse = Response<GetExtensionVersionReply>;

  Future<GetExtensionVersionReply> GetExtensionVersion(
      const GetExtensionVersionRequest& request);

  Future<GetExtensionVersionReply> GetExtensionVersion(
      const std::string& name = {});

  struct ListInputDevicesRequest {};

  struct ListInputDevicesReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<DeviceInfo> devices{};
    std::vector<InputInfo> infos{};
    std::vector<Str> names{};
  };

  using ListInputDevicesResponse = Response<ListInputDevicesReply>;

  Future<ListInputDevicesReply> ListInputDevices(
      const ListInputDevicesRequest& request);

  Future<ListInputDevicesReply> ListInputDevices();

  struct OpenDeviceRequest {
    uint8_t device_id{};
  };

  struct OpenDeviceReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<InputClassInfo> class_info{};
  };

  using OpenDeviceResponse = Response<OpenDeviceReply>;

  Future<OpenDeviceReply> OpenDevice(const OpenDeviceRequest& request);

  Future<OpenDeviceReply> OpenDevice(const uint8_t& device_id = {});

  struct CloseDeviceRequest {
    uint8_t device_id{};
  };

  using CloseDeviceResponse = Response<void>;

  Future<void> CloseDevice(const CloseDeviceRequest& request);

  Future<void> CloseDevice(const uint8_t& device_id = {});

  struct SetDeviceModeRequest {
    uint8_t device_id{};
    ValuatorMode mode{};
  };

  struct SetDeviceModeReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    GrabStatus status{};
  };

  using SetDeviceModeResponse = Response<SetDeviceModeReply>;

  Future<SetDeviceModeReply> SetDeviceMode(const SetDeviceModeRequest& request);

  Future<SetDeviceModeReply> SetDeviceMode(const uint8_t& device_id = {},
                                           const ValuatorMode& mode = {});

  struct SelectExtensionEventRequest {
    Window window{};
    std::vector<EventClass> classes{};
  };

  using SelectExtensionEventResponse = Response<void>;

  Future<void> SelectExtensionEvent(const SelectExtensionEventRequest& request);

  Future<void> SelectExtensionEvent(
      const Window& window = {},
      const std::vector<EventClass>& classes = {});

  struct GetSelectedExtensionEventsRequest {
    Window window{};
  };

  struct GetSelectedExtensionEventsReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<EventClass> this_classes{};
    std::vector<EventClass> all_classes{};
  };

  using GetSelectedExtensionEventsResponse =
      Response<GetSelectedExtensionEventsReply>;

  Future<GetSelectedExtensionEventsReply> GetSelectedExtensionEvents(
      const GetSelectedExtensionEventsRequest& request);

  Future<GetSelectedExtensionEventsReply> GetSelectedExtensionEvents(
      const Window& window = {});

  struct ChangeDeviceDontPropagateListRequest {
    Window window{};
    PropagateMode mode{};
    std::vector<EventClass> classes{};
  };

  using ChangeDeviceDontPropagateListResponse = Response<void>;

  Future<void> ChangeDeviceDontPropagateList(
      const ChangeDeviceDontPropagateListRequest& request);

  Future<void> ChangeDeviceDontPropagateList(
      const Window& window = {},
      const PropagateMode& mode = {},
      const std::vector<EventClass>& classes = {});

  struct GetDeviceDontPropagateListRequest {
    Window window{};
  };

  struct GetDeviceDontPropagateListReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<EventClass> classes{};
  };

  using GetDeviceDontPropagateListResponse =
      Response<GetDeviceDontPropagateListReply>;

  Future<GetDeviceDontPropagateListReply> GetDeviceDontPropagateList(
      const GetDeviceDontPropagateListRequest& request);

  Future<GetDeviceDontPropagateListReply> GetDeviceDontPropagateList(
      const Window& window = {});

  struct GetDeviceMotionEventsRequest {
    Time start{};
    Time stop{};
    uint8_t device_id{};
  };

  struct GetDeviceMotionEventsReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    uint8_t num_axes{};
    ValuatorMode device_mode{};
    std::vector<DeviceTimeCoord> events{};
  };

  using GetDeviceMotionEventsResponse = Response<GetDeviceMotionEventsReply>;

  Future<GetDeviceMotionEventsReply> GetDeviceMotionEvents(
      const GetDeviceMotionEventsRequest& request);

  Future<GetDeviceMotionEventsReply> GetDeviceMotionEvents(
      const Time& start = {},
      const Time& stop = {},
      const uint8_t& device_id = {});

  struct ChangeKeyboardDeviceRequest {
    uint8_t device_id{};
  };

  struct ChangeKeyboardDeviceReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    GrabStatus status{};
  };

  using ChangeKeyboardDeviceResponse = Response<ChangeKeyboardDeviceReply>;

  Future<ChangeKeyboardDeviceReply> ChangeKeyboardDevice(
      const ChangeKeyboardDeviceRequest& request);

  Future<ChangeKeyboardDeviceReply> ChangeKeyboardDevice(
      const uint8_t& device_id = {});

  struct ChangePointerDeviceRequest {
    uint8_t x_axis{};
    uint8_t y_axis{};
    uint8_t device_id{};
  };

  struct ChangePointerDeviceReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    GrabStatus status{};
  };

  using ChangePointerDeviceResponse = Response<ChangePointerDeviceReply>;

  Future<ChangePointerDeviceReply> ChangePointerDevice(
      const ChangePointerDeviceRequest& request);

  Future<ChangePointerDeviceReply> ChangePointerDevice(
      const uint8_t& x_axis = {},
      const uint8_t& y_axis = {},
      const uint8_t& device_id = {});

  struct GrabDeviceRequest {
    Window grab_window{};
    Time time{};
    GrabMode this_device_mode{};
    GrabMode other_device_mode{};
    uint8_t owner_events{};
    uint8_t device_id{};
    std::vector<EventClass> classes{};
  };

  struct GrabDeviceReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    GrabStatus status{};
  };

  using GrabDeviceResponse = Response<GrabDeviceReply>;

  Future<GrabDeviceReply> GrabDevice(const GrabDeviceRequest& request);

  Future<GrabDeviceReply> GrabDevice(
      const Window& grab_window = {},
      const Time& time = {},
      const GrabMode& this_device_mode = {},
      const GrabMode& other_device_mode = {},
      const uint8_t& owner_events = {},
      const uint8_t& device_id = {},
      const std::vector<EventClass>& classes = {});

  struct UngrabDeviceRequest {
    Time time{};
    uint8_t device_id{};
  };

  using UngrabDeviceResponse = Response<void>;

  Future<void> UngrabDevice(const UngrabDeviceRequest& request);

  Future<void> UngrabDevice(const Time& time = {},
                            const uint8_t& device_id = {});

  struct GrabDeviceKeyRequest {
    Window grab_window{};
    ModMask modifiers{};
    uint8_t modifier_device{};
    uint8_t grabbed_device{};
    uint8_t key{};
    GrabMode this_device_mode{};
    GrabMode other_device_mode{};
    uint8_t owner_events{};
    std::vector<EventClass> classes{};
  };

  using GrabDeviceKeyResponse = Response<void>;

  Future<void> GrabDeviceKey(const GrabDeviceKeyRequest& request);

  Future<void> GrabDeviceKey(const Window& grab_window = {},
                             const ModMask& modifiers = {},
                             const uint8_t& modifier_device = {},
                             const uint8_t& grabbed_device = {},
                             const uint8_t& key = {},
                             const GrabMode& this_device_mode = {},
                             const GrabMode& other_device_mode = {},
                             const uint8_t& owner_events = {},
                             const std::vector<EventClass>& classes = {});

  struct UngrabDeviceKeyRequest {
    Window grabWindow{};
    ModMask modifiers{};
    uint8_t modifier_device{};
    uint8_t key{};
    uint8_t grabbed_device{};
  };

  using UngrabDeviceKeyResponse = Response<void>;

  Future<void> UngrabDeviceKey(const UngrabDeviceKeyRequest& request);

  Future<void> UngrabDeviceKey(const Window& grabWindow = {},
                               const ModMask& modifiers = {},
                               const uint8_t& modifier_device = {},
                               const uint8_t& key = {},
                               const uint8_t& grabbed_device = {});

  struct GrabDeviceButtonRequest {
    Window grab_window{};
    uint8_t grabbed_device{};
    uint8_t modifier_device{};
    ModMask modifiers{};
    GrabMode this_device_mode{};
    GrabMode other_device_mode{};
    uint8_t button{};
    uint8_t owner_events{};
    std::vector<EventClass> classes{};
  };

  using GrabDeviceButtonResponse = Response<void>;

  Future<void> GrabDeviceButton(const GrabDeviceButtonRequest& request);

  Future<void> GrabDeviceButton(const Window& grab_window = {},
                                const uint8_t& grabbed_device = {},
                                const uint8_t& modifier_device = {},
                                const ModMask& modifiers = {},
                                const GrabMode& this_device_mode = {},
                                const GrabMode& other_device_mode = {},
                                const uint8_t& button = {},
                                const uint8_t& owner_events = {},
                                const std::vector<EventClass>& classes = {});

  struct UngrabDeviceButtonRequest {
    Window grab_window{};
    ModMask modifiers{};
    uint8_t modifier_device{};
    uint8_t button{};
    uint8_t grabbed_device{};
  };

  using UngrabDeviceButtonResponse = Response<void>;

  Future<void> UngrabDeviceButton(const UngrabDeviceButtonRequest& request);

  Future<void> UngrabDeviceButton(const Window& grab_window = {},
                                  const ModMask& modifiers = {},
                                  const uint8_t& modifier_device = {},
                                  const uint8_t& button = {},
                                  const uint8_t& grabbed_device = {});

  struct AllowDeviceEventsRequest {
    Time time{};
    DeviceInputMode mode{};
    uint8_t device_id{};
  };

  using AllowDeviceEventsResponse = Response<void>;

  Future<void> AllowDeviceEvents(const AllowDeviceEventsRequest& request);

  Future<void> AllowDeviceEvents(const Time& time = {},
                                 const DeviceInputMode& mode = {},
                                 const uint8_t& device_id = {});

  struct GetDeviceFocusRequest {
    uint8_t device_id{};
  };

  struct GetDeviceFocusReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    Window focus{};
    Time time{};
    InputFocus revert_to{};
  };

  using GetDeviceFocusResponse = Response<GetDeviceFocusReply>;

  Future<GetDeviceFocusReply> GetDeviceFocus(
      const GetDeviceFocusRequest& request);

  Future<GetDeviceFocusReply> GetDeviceFocus(const uint8_t& device_id = {});

  struct SetDeviceFocusRequest {
    Window focus{};
    Time time{};
    InputFocus revert_to{};
    uint8_t device_id{};
  };

  using SetDeviceFocusResponse = Response<void>;

  Future<void> SetDeviceFocus(const SetDeviceFocusRequest& request);

  Future<void> SetDeviceFocus(const Window& focus = {},
                              const Time& time = {},
                              const InputFocus& revert_to = {},
                              const uint8_t& device_id = {});

  struct GetFeedbackControlRequest {
    uint8_t device_id{};
  };

  struct GetFeedbackControlReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<FeedbackState> feedbacks{};
  };

  using GetFeedbackControlResponse = Response<GetFeedbackControlReply>;

  Future<GetFeedbackControlReply> GetFeedbackControl(
      const GetFeedbackControlRequest& request);

  Future<GetFeedbackControlReply> GetFeedbackControl(
      const uint8_t& device_id = {});

  struct ChangeFeedbackControlRequest {
    ChangeFeedbackControlMask mask{};
    uint8_t device_id{};
    uint8_t feedback_id{};
    FeedbackCtl feedback{};
  };

  using ChangeFeedbackControlResponse = Response<void>;

  Future<void> ChangeFeedbackControl(
      const ChangeFeedbackControlRequest& request);

  struct Keyboard {
    KeyCode key{};
    uint8_t auto_repeat_mode{};
    int8_t key_click_percent{};
    int8_t bell_percent{};
    int16_t bell_pitch{};
    int16_t bell_duration{};
    uint32_t led_mask{};
    uint32_t led_values{};
  };
  struct Pointer {
    int16_t num{};
    int16_t denom{};
    int16_t threshold{};
  };
  struct String {
    std::vector<KeySym> keysyms{};
  };
  struct Integer {
    int32_t int_to_display{};
  };
  struct Led {
    uint32_t led_mask{};
    uint32_t led_values{};
  };
  struct Bell {
    int8_t percent{};
    int16_t pitch{};
    int16_t duration{};
  };
  Future<void> ChangeFeedbackControl(const ChangeFeedbackControlMask& mask = {},
                                     const uint8_t& device_id = {},
                                     const uint8_t& feedback_id = {},
                                     const FeedbackCtl& feedback = {
                                         {},
                                         {},
                                         std::nullopt,
                                         std::nullopt,
                                         std::nullopt,
                                         std::nullopt,
                                         std::nullopt,
                                         std::nullopt});

  struct GetDeviceKeyMappingRequest {
    uint8_t device_id{};
    KeyCode first_keycode{};
    uint8_t count{};
  };

  struct GetDeviceKeyMappingReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    uint8_t keysyms_per_keycode{};
    std::vector<KeySym> keysyms{};
  };

  using GetDeviceKeyMappingResponse = Response<GetDeviceKeyMappingReply>;

  Future<GetDeviceKeyMappingReply> GetDeviceKeyMapping(
      const GetDeviceKeyMappingRequest& request);

  Future<GetDeviceKeyMappingReply> GetDeviceKeyMapping(
      const uint8_t& device_id = {},
      const KeyCode& first_keycode = {},
      const uint8_t& count = {});

  struct ChangeDeviceKeyMappingRequest {
    uint8_t device_id{};
    KeyCode first_keycode{};
    uint8_t keysyms_per_keycode{};
    uint8_t keycode_count{};
    std::vector<KeySym> keysyms{};
  };

  using ChangeDeviceKeyMappingResponse = Response<void>;

  Future<void> ChangeDeviceKeyMapping(
      const ChangeDeviceKeyMappingRequest& request);

  Future<void> ChangeDeviceKeyMapping(const uint8_t& device_id = {},
                                      const KeyCode& first_keycode = {},
                                      const uint8_t& keysyms_per_keycode = {},
                                      const uint8_t& keycode_count = {},
                                      const std::vector<KeySym>& keysyms = {});

  struct GetDeviceModifierMappingRequest {
    uint8_t device_id{};
  };

  struct GetDeviceModifierMappingReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    uint8_t keycodes_per_modifier{};
    std::vector<uint8_t> keymaps{};
  };

  using GetDeviceModifierMappingResponse =
      Response<GetDeviceModifierMappingReply>;

  Future<GetDeviceModifierMappingReply> GetDeviceModifierMapping(
      const GetDeviceModifierMappingRequest& request);

  Future<GetDeviceModifierMappingReply> GetDeviceModifierMapping(
      const uint8_t& device_id = {});

  struct SetDeviceModifierMappingRequest {
    uint8_t device_id{};
    uint8_t keycodes_per_modifier{};
    std::vector<uint8_t> keymaps{};
  };

  struct SetDeviceModifierMappingReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    MappingStatus status{};
  };

  using SetDeviceModifierMappingResponse =
      Response<SetDeviceModifierMappingReply>;

  Future<SetDeviceModifierMappingReply> SetDeviceModifierMapping(
      const SetDeviceModifierMappingRequest& request);

  Future<SetDeviceModifierMappingReply> SetDeviceModifierMapping(
      const uint8_t& device_id = {},
      const uint8_t& keycodes_per_modifier = {},
      const std::vector<uint8_t>& keymaps = {});

  struct GetDeviceButtonMappingRequest {
    uint8_t device_id{};
  };

  struct GetDeviceButtonMappingReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<uint8_t> map{};
  };

  using GetDeviceButtonMappingResponse = Response<GetDeviceButtonMappingReply>;

  Future<GetDeviceButtonMappingReply> GetDeviceButtonMapping(
      const GetDeviceButtonMappingRequest& request);

  Future<GetDeviceButtonMappingReply> GetDeviceButtonMapping(
      const uint8_t& device_id = {});

  struct SetDeviceButtonMappingRequest {
    uint8_t device_id{};
    std::vector<uint8_t> map{};
  };

  struct SetDeviceButtonMappingReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    MappingStatus status{};
  };

  using SetDeviceButtonMappingResponse = Response<SetDeviceButtonMappingReply>;

  Future<SetDeviceButtonMappingReply> SetDeviceButtonMapping(
      const SetDeviceButtonMappingRequest& request);

  Future<SetDeviceButtonMappingReply> SetDeviceButtonMapping(
      const uint8_t& device_id = {},
      const std::vector<uint8_t>& map = {});

  struct QueryDeviceStateRequest {
    uint8_t device_id{};
  };

  struct QueryDeviceStateReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<InputState> classes{};
  };

  using QueryDeviceStateResponse = Response<QueryDeviceStateReply>;

  Future<QueryDeviceStateReply> QueryDeviceState(
      const QueryDeviceStateRequest& request);

  Future<QueryDeviceStateReply> QueryDeviceState(const uint8_t& device_id = {});

  struct DeviceBellRequest {
    uint8_t device_id{};
    uint8_t feedback_id{};
    uint8_t feedback_class{};
    int8_t percent{};
  };

  using DeviceBellResponse = Response<void>;

  Future<void> DeviceBell(const DeviceBellRequest& request);

  Future<void> DeviceBell(const uint8_t& device_id = {},
                          const uint8_t& feedback_id = {},
                          const uint8_t& feedback_class = {},
                          const int8_t& percent = {});

  struct SetDeviceValuatorsRequest {
    uint8_t device_id{};
    uint8_t first_valuator{};
    std::vector<int32_t> valuators{};
  };

  struct SetDeviceValuatorsReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    GrabStatus status{};
  };

  using SetDeviceValuatorsResponse = Response<SetDeviceValuatorsReply>;

  Future<SetDeviceValuatorsReply> SetDeviceValuators(
      const SetDeviceValuatorsRequest& request);

  Future<SetDeviceValuatorsReply> SetDeviceValuators(
      const uint8_t& device_id = {},
      const uint8_t& first_valuator = {},
      const std::vector<int32_t>& valuators = {});

  struct GetDeviceControlRequest {
    DeviceControl control_id{};
    uint8_t device_id{};
  };

  struct GetDeviceControlReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    uint8_t status{};
    DeviceState control{};
  };

  using GetDeviceControlResponse = Response<GetDeviceControlReply>;

  Future<GetDeviceControlReply> GetDeviceControl(
      const GetDeviceControlRequest& request);

  Future<GetDeviceControlReply> GetDeviceControl(
      const DeviceControl& control_id = {},
      const uint8_t& device_id = {});

  struct ChangeDeviceControlRequest {
    DeviceControl control_id{};
    uint8_t device_id{};
    DeviceCtl control{};
  };

  struct ChangeDeviceControlReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    uint8_t status{};
  };

  using ChangeDeviceControlResponse = Response<ChangeDeviceControlReply>;

  Future<ChangeDeviceControlReply> ChangeDeviceControl(
      const ChangeDeviceControlRequest& request);

  struct Resolution {
    uint8_t first_valuator{};
    std::vector<uint32_t> resolution_values{};
  };
  struct AbsCalib {
    int32_t min_x{};
    int32_t max_x{};
    int32_t min_y{};
    int32_t max_y{};
    uint32_t flip_x{};
    uint32_t flip_y{};
    uint32_t rotation{};
    uint32_t button_threshold{};
  };
  struct Core {
    uint8_t status{};
  };
  struct Enable {
    uint8_t enable{};
  };
  struct AbsArea {
    uint32_t offset_x{};
    uint32_t offset_y{};
    int32_t width{};
    int32_t height{};
    int32_t screen{};
    uint32_t following{};
  };
  Future<ChangeDeviceControlReply> ChangeDeviceControl(
      const DeviceControl& control_id = {},
      const uint8_t& device_id = {},
      const DeviceCtl& control = {{},
                                  std::nullopt,
                                  std::nullopt,
                                  std::nullopt,
                                  std::nullopt,
                                  std::nullopt});

  struct ListDevicePropertiesRequest {
    uint8_t device_id{};
  };

  struct ListDevicePropertiesReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    std::vector<Atom> atoms{};
  };

  using ListDevicePropertiesResponse = Response<ListDevicePropertiesReply>;

  Future<ListDevicePropertiesReply> ListDeviceProperties(
      const ListDevicePropertiesRequest& request);

  Future<ListDevicePropertiesReply> ListDeviceProperties(
      const uint8_t& device_id = {});

  struct ChangeDevicePropertyRequest {
    Atom property{};
    Atom type{};
    uint8_t device_id{};
    PropMode mode{};
    uint32_t num_items{};
    std::optional<std::vector<uint8_t>> data8{};
    std::optional<std::vector<uint16_t>> data16{};
    std::optional<std::vector<uint32_t>> data32{};
  };

  using ChangeDevicePropertyResponse = Response<void>;

  Future<void> ChangeDeviceProperty(const ChangeDevicePropertyRequest& request);

  Future<void> ChangeDeviceProperty(
      const Atom& property = {},
      const Atom& type = {},
      const uint8_t& device_id = {},
      const PropMode& mode = {},
      const uint32_t& num_items = {},
      const std::optional<std::vector<uint8_t>>& data8 = std::nullopt,
      const std::optional<std::vector<uint16_t>>& data16 = std::nullopt,
      const std::optional<std::vector<uint32_t>>& data32 = std::nullopt);

  struct DeleteDevicePropertyRequest {
    Atom property{};
    uint8_t device_id{};
  };

  using DeleteDevicePropertyResponse = Response<void>;

  Future<void> DeleteDeviceProperty(const DeleteDevicePropertyRequest& request);

  Future<void> DeleteDeviceProperty(const Atom& property = {},
                                    const uint8_t& device_id = {});

  struct GetDevicePropertyRequest {
    Atom property{};
    Atom type{};
    uint32_t offset{};
    uint32_t len{};
    uint8_t device_id{};
    uint8_t c_delete{};
  };

  struct GetDevicePropertyReply {
    uint8_t xi_reply_type{};
    uint16_t sequence{};
    Atom type{};
    uint32_t bytes_after{};
    uint32_t num_items{};
    uint8_t device_id{};
    std::optional<std::vector<uint8_t>> data8{};
    std::optional<std::vector<uint16_t>> data16{};
    std::optional<std::vector<uint32_t>> data32{};
  };

  using GetDevicePropertyResponse = Response<GetDevicePropertyReply>;

  Future<GetDevicePropertyReply> GetDeviceProperty(
      const GetDevicePropertyRequest& request);

  Future<GetDevicePropertyReply> GetDeviceProperty(
      const Atom& property = {},
      const Atom& type = {},
      const uint32_t& offset = {},
      const uint32_t& len = {},
      const uint8_t& device_id = {},
      const uint8_t& c_delete = {});

  struct XIQueryPointerRequest {
    Window window{};
    DeviceId deviceid{};
  };

  struct XIQueryPointerReply {
    uint16_t sequence{};
    Window root{};
    Window child{};
    Fp1616 root_x{};
    Fp1616 root_y{};
    Fp1616 win_x{};
    Fp1616 win_y{};
    uint8_t same_screen{};
    ModifierInfo mods{};
    GroupInfo group{};
    std::vector<uint32_t> buttons{};
  };

  using XIQueryPointerResponse = Response<XIQueryPointerReply>;

  Future<XIQueryPointerReply> XIQueryPointer(
      const XIQueryPointerRequest& request);

  Future<XIQueryPointerReply> XIQueryPointer(const Window& window = {},
                                             const DeviceId& deviceid = {});

  struct XIWarpPointerRequest {
    Window src_win{};
    Window dst_win{};
    Fp1616 src_x{};
    Fp1616 src_y{};
    uint16_t src_width{};
    uint16_t src_height{};
    Fp1616 dst_x{};
    Fp1616 dst_y{};
    DeviceId deviceid{};
  };

  using XIWarpPointerResponse = Response<void>;

  Future<void> XIWarpPointer(const XIWarpPointerRequest& request);

  Future<void> XIWarpPointer(const Window& src_win = {},
                             const Window& dst_win = {},
                             const Fp1616& src_x = {},
                             const Fp1616& src_y = {},
                             const uint16_t& src_width = {},
                             const uint16_t& src_height = {},
                             const Fp1616& dst_x = {},
                             const Fp1616& dst_y = {},
                             const DeviceId& deviceid = {});

  struct XIChangeCursorRequest {
    Window window{};
    Cursor cursor{};
    DeviceId deviceid{};
  };

  using XIChangeCursorResponse = Response<void>;

  Future<void> XIChangeCursor(const XIChangeCursorRequest& request);

  Future<void> XIChangeCursor(const Window& window = {},
                              const Cursor& cursor = {},
                              const DeviceId& deviceid = {});

  struct XIChangeHierarchyRequest {
    std::vector<HierarchyChange> changes{};
  };

  using XIChangeHierarchyResponse = Response<void>;

  Future<void> XIChangeHierarchy(const XIChangeHierarchyRequest& request);

  Future<void> XIChangeHierarchy(
      const std::vector<HierarchyChange>& changes = {});

  struct XISetClientPointerRequest {
    Window window{};
    DeviceId deviceid{};
  };

  using XISetClientPointerResponse = Response<void>;

  Future<void> XISetClientPointer(const XISetClientPointerRequest& request);

  Future<void> XISetClientPointer(const Window& window = {},
                                  const DeviceId& deviceid = {});

  struct XIGetClientPointerRequest {
    Window window{};
  };

  struct XIGetClientPointerReply {
    uint16_t sequence{};
    uint8_t set{};
    DeviceId deviceid{};
  };

  using XIGetClientPointerResponse = Response<XIGetClientPointerReply>;

  Future<XIGetClientPointerReply> XIGetClientPointer(
      const XIGetClientPointerRequest& request);

  Future<XIGetClientPointerReply> XIGetClientPointer(const Window& window = {});

  struct XISelectEventsRequest {
    Window window{};
    std::vector<EventMask> masks{};
  };

  using XISelectEventsResponse = Response<void>;

  Future<void> XISelectEvents(const XISelectEventsRequest& request);

  Future<void> XISelectEvents(const Window& window = {},
                              const std::vector<EventMask>& masks = {});

  struct XIQueryVersionRequest {
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  struct XIQueryVersionReply {
    uint16_t sequence{};
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  using XIQueryVersionResponse = Response<XIQueryVersionReply>;

  Future<XIQueryVersionReply> XIQueryVersion(
      const XIQueryVersionRequest& request);

  Future<XIQueryVersionReply> XIQueryVersion(
      const uint16_t& major_version = {},
      const uint16_t& minor_version = {});

  struct XIQueryDeviceRequest {
    DeviceId deviceid{};
  };

  struct XIQueryDeviceReply {
    uint16_t sequence{};
    std::vector<XIDeviceInfo> infos{};
  };

  using XIQueryDeviceResponse = Response<XIQueryDeviceReply>;

  Future<XIQueryDeviceReply> XIQueryDevice(const XIQueryDeviceRequest& request);

  Future<XIQueryDeviceReply> XIQueryDevice(const DeviceId& deviceid = {});

  struct XISetFocusRequest {
    Window window{};
    Time time{};
    DeviceId deviceid{};
  };

  using XISetFocusResponse = Response<void>;

  Future<void> XISetFocus(const XISetFocusRequest& request);

  Future<void> XISetFocus(const Window& window = {},
                          const Time& time = {},
                          const DeviceId& deviceid = {});

  struct XIGetFocusRequest {
    DeviceId deviceid{};
  };

  struct XIGetFocusReply {
    uint16_t sequence{};
    Window focus{};
  };

  using XIGetFocusResponse = Response<XIGetFocusReply>;

  Future<XIGetFocusReply> XIGetFocus(const XIGetFocusRequest& request);

  Future<XIGetFocusReply> XIGetFocus(const DeviceId& deviceid = {});

  struct XIGrabDeviceRequest {
    Window window{};
    Time time{};
    Cursor cursor{};
    DeviceId deviceid{};
    GrabMode mode{};
    GrabMode paired_device_mode{};
    GrabOwner owner_events{};
    std::vector<uint32_t> mask{};
  };

  struct XIGrabDeviceReply {
    uint16_t sequence{};
    GrabStatus status{};
  };

  using XIGrabDeviceResponse = Response<XIGrabDeviceReply>;

  Future<XIGrabDeviceReply> XIGrabDevice(const XIGrabDeviceRequest& request);

  Future<XIGrabDeviceReply> XIGrabDevice(
      const Window& window = {},
      const Time& time = {},
      const Cursor& cursor = {},
      const DeviceId& deviceid = {},
      const GrabMode& mode = {},
      const GrabMode& paired_device_mode = {},
      const GrabOwner& owner_events = {},
      const std::vector<uint32_t>& mask = {});

  struct XIUngrabDeviceRequest {
    Time time{};
    DeviceId deviceid{};
  };

  using XIUngrabDeviceResponse = Response<void>;

  Future<void> XIUngrabDevice(const XIUngrabDeviceRequest& request);

  Future<void> XIUngrabDevice(const Time& time = {},
                              const DeviceId& deviceid = {});

  struct XIAllowEventsRequest {
    Time time{};
    DeviceId deviceid{};
    EventMode event_mode{};
    uint32_t touchid{};
    Window grab_window{};
  };

  using XIAllowEventsResponse = Response<void>;

  Future<void> XIAllowEvents(const XIAllowEventsRequest& request);

  Future<void> XIAllowEvents(const Time& time = {},
                             const DeviceId& deviceid = {},
                             const EventMode& event_mode = {},
                             const uint32_t& touchid = {},
                             const Window& grab_window = {});

  struct XIPassiveGrabDeviceRequest {
    Time time{};
    Window grab_window{};
    Cursor cursor{};
    uint32_t detail{};
    DeviceId deviceid{};
    GrabType grab_type{};
    GrabMode22 grab_mode{};
    GrabMode paired_device_mode{};
    GrabOwner owner_events{};
    std::vector<uint32_t> mask{};
    std::vector<uint32_t> modifiers{};
  };

  struct XIPassiveGrabDeviceReply {
    uint16_t sequence{};
    std::vector<GrabModifierInfo> modifiers{};
  };

  using XIPassiveGrabDeviceResponse = Response<XIPassiveGrabDeviceReply>;

  Future<XIPassiveGrabDeviceReply> XIPassiveGrabDevice(
      const XIPassiveGrabDeviceRequest& request);

  Future<XIPassiveGrabDeviceReply> XIPassiveGrabDevice(
      const Time& time = {},
      const Window& grab_window = {},
      const Cursor& cursor = {},
      const uint32_t& detail = {},
      const DeviceId& deviceid = {},
      const GrabType& grab_type = {},
      const GrabMode22& grab_mode = {},
      const GrabMode& paired_device_mode = {},
      const GrabOwner& owner_events = {},
      const std::vector<uint32_t>& mask = {},
      const std::vector<uint32_t>& modifiers = {});

  struct XIPassiveUngrabDeviceRequest {
    Window grab_window{};
    uint32_t detail{};
    DeviceId deviceid{};
    GrabType grab_type{};
    std::vector<uint32_t> modifiers{};
  };

  using XIPassiveUngrabDeviceResponse = Response<void>;

  Future<void> XIPassiveUngrabDevice(
      const XIPassiveUngrabDeviceRequest& request);

  Future<void> XIPassiveUngrabDevice(
      const Window& grab_window = {},
      const uint32_t& detail = {},
      const DeviceId& deviceid = {},
      const GrabType& grab_type = {},
      const std::vector<uint32_t>& modifiers = {});

  struct XIListPropertiesRequest {
    DeviceId deviceid{};
  };

  struct XIListPropertiesReply {
    uint16_t sequence{};
    std::vector<Atom> properties{};
  };

  using XIListPropertiesResponse = Response<XIListPropertiesReply>;

  Future<XIListPropertiesReply> XIListProperties(
      const XIListPropertiesRequest& request);

  Future<XIListPropertiesReply> XIListProperties(const DeviceId& deviceid = {});

  struct XIChangePropertyRequest {
    DeviceId deviceid{};
    PropMode mode{};
    Atom property{};
    Atom type{};
    uint32_t num_items{};
    std::optional<std::vector<uint8_t>> data8{};
    std::optional<std::vector<uint16_t>> data16{};
    std::optional<std::vector<uint32_t>> data32{};
  };

  using XIChangePropertyResponse = Response<void>;

  Future<void> XIChangeProperty(const XIChangePropertyRequest& request);

  Future<void> XIChangeProperty(
      const DeviceId& deviceid = {},
      const PropMode& mode = {},
      const Atom& property = {},
      const Atom& type = {},
      const uint32_t& num_items = {},
      const std::optional<std::vector<uint8_t>>& data8 = std::nullopt,
      const std::optional<std::vector<uint16_t>>& data16 = std::nullopt,
      const std::optional<std::vector<uint32_t>>& data32 = std::nullopt);

  struct XIDeletePropertyRequest {
    DeviceId deviceid{};
    Atom property{};
  };

  using XIDeletePropertyResponse = Response<void>;

  Future<void> XIDeleteProperty(const XIDeletePropertyRequest& request);

  Future<void> XIDeleteProperty(const DeviceId& deviceid = {},
                                const Atom& property = {});

  struct XIGetPropertyRequest {
    DeviceId deviceid{};
    uint8_t c_delete{};
    Atom property{};
    Atom type{};
    uint32_t offset{};
    uint32_t len{};
  };

  struct XIGetPropertyReply {
    uint16_t sequence{};
    Atom type{};
    uint32_t bytes_after{};
    uint32_t num_items{};
    std::optional<std::vector<uint8_t>> data8{};
    std::optional<std::vector<uint16_t>> data16{};
    std::optional<std::vector<uint32_t>> data32{};
  };

  using XIGetPropertyResponse = Response<XIGetPropertyReply>;

  Future<XIGetPropertyReply> XIGetProperty(const XIGetPropertyRequest& request);

  Future<XIGetPropertyReply> XIGetProperty(const DeviceId& deviceid = {},
                                           const uint8_t& c_delete = {},
                                           const Atom& property = {},
                                           const Atom& type = {},
                                           const uint32_t& offset = {},
                                           const uint32_t& len = {});

  struct XIGetSelectedEventsRequest {
    Window window{};
  };

  struct XIGetSelectedEventsReply {
    uint16_t sequence{};
    std::vector<EventMask> masks{};
  };

  using XIGetSelectedEventsResponse = Response<XIGetSelectedEventsReply>;

  Future<XIGetSelectedEventsReply> XIGetSelectedEvents(
      const XIGetSelectedEventsRequest& request);

  Future<XIGetSelectedEventsReply> XIGetSelectedEvents(
      const Window& window = {});

  struct XIBarrierReleasePointerRequest {
    std::vector<BarrierReleasePointerInfo> barriers{};
  };

  using XIBarrierReleasePointerResponse = Response<void>;

  Future<void> XIBarrierReleasePointer(
      const XIBarrierReleasePointerRequest& request);

  Future<void> XIBarrierReleasePointer(
      const std::vector<BarrierReleasePointerInfo>& barriers = {});

  struct SendExtensionEventRequest {
    Window destination{};
    uint8_t device_id{};
    uint8_t propagate{};
    std::vector<EventForSend> events{};
    std::vector<EventClass> classes{};
  };

  using SendExtensionEventResponse = Response<void>;

  Future<void> SendExtensionEvent(const SendExtensionEventRequest& request);

  Future<void> SendExtensionEvent(const Window& destination = {},
                                  const uint8_t& device_id = {},
                                  const uint8_t& propagate = {},
                                  const std::vector<EventForSend>& events = {},
                                  const std::vector<EventClass>& classes = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Input::DeviceUse operator|(x11::Input::DeviceUse l,
                                                 x11::Input::DeviceUse r) {
  using T = std::underlying_type_t<x11::Input::DeviceUse>;
  return static_cast<x11::Input::DeviceUse>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Input::DeviceUse operator&(x11::Input::DeviceUse l,
                                                 x11::Input::DeviceUse r) {
  using T = std::underlying_type_t<x11::Input::DeviceUse>;
  return static_cast<x11::Input::DeviceUse>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Input::InputClass operator|(x11::Input::InputClass l,
                                                  x11::Input::InputClass r) {
  using T = std::underlying_type_t<x11::Input::InputClass>;
  return static_cast<x11::Input::InputClass>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Input::InputClass operator&(x11::Input::InputClass l,
                                                  x11::Input::InputClass r) {
  using T = std::underlying_type_t<x11::Input::InputClass>;
  return static_cast<x11::Input::InputClass>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Input::ValuatorMode operator|(
    x11::Input::ValuatorMode l,
    x11::Input::ValuatorMode r) {
  using T = std::underlying_type_t<x11::Input::ValuatorMode>;
  return static_cast<x11::Input::ValuatorMode>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::ValuatorMode operator&(
    x11::Input::ValuatorMode l,
    x11::Input::ValuatorMode r) {
  using T = std::underlying_type_t<x11::Input::ValuatorMode>;
  return static_cast<x11::Input::ValuatorMode>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::PropagateMode operator|(
    x11::Input::PropagateMode l,
    x11::Input::PropagateMode r) {
  using T = std::underlying_type_t<x11::Input::PropagateMode>;
  return static_cast<x11::Input::PropagateMode>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Input::PropagateMode operator&(
    x11::Input::PropagateMode l,
    x11::Input::PropagateMode r) {
  using T = std::underlying_type_t<x11::Input::PropagateMode>;
  return static_cast<x11::Input::PropagateMode>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Input::ModifierDevice operator|(
    x11::Input::ModifierDevice l,
    x11::Input::ModifierDevice r) {
  using T = std::underlying_type_t<x11::Input::ModifierDevice>;
  return static_cast<x11::Input::ModifierDevice>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Input::ModifierDevice operator&(
    x11::Input::ModifierDevice l,
    x11::Input::ModifierDevice r) {
  using T = std::underlying_type_t<x11::Input::ModifierDevice>;
  return static_cast<x11::Input::ModifierDevice>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Input::DeviceInputMode operator|(
    x11::Input::DeviceInputMode l,
    x11::Input::DeviceInputMode r) {
  using T = std::underlying_type_t<x11::Input::DeviceInputMode>;
  return static_cast<x11::Input::DeviceInputMode>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::Input::DeviceInputMode operator&(
    x11::Input::DeviceInputMode l,
    x11::Input::DeviceInputMode r) {
  using T = std::underlying_type_t<x11::Input::DeviceInputMode>;
  return static_cast<x11::Input::DeviceInputMode>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::Input::FeedbackClass operator|(
    x11::Input::FeedbackClass l,
    x11::Input::FeedbackClass r) {
  using T = std::underlying_type_t<x11::Input::FeedbackClass>;
  return static_cast<x11::Input::FeedbackClass>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Input::FeedbackClass operator&(
    x11::Input::FeedbackClass l,
    x11::Input::FeedbackClass r) {
  using T = std::underlying_type_t<x11::Input::FeedbackClass>;
  return static_cast<x11::Input::FeedbackClass>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Input::ChangeFeedbackControlMask operator|(
    x11::Input::ChangeFeedbackControlMask l,
    x11::Input::ChangeFeedbackControlMask r) {
  using T = std::underlying_type_t<x11::Input::ChangeFeedbackControlMask>;
  return static_cast<x11::Input::ChangeFeedbackControlMask>(static_cast<T>(l) |
                                                            static_cast<T>(r));
}

inline constexpr x11::Input::ChangeFeedbackControlMask operator&(
    x11::Input::ChangeFeedbackControlMask l,
    x11::Input::ChangeFeedbackControlMask r) {
  using T = std::underlying_type_t<x11::Input::ChangeFeedbackControlMask>;
  return static_cast<x11::Input::ChangeFeedbackControlMask>(static_cast<T>(l) &
                                                            static_cast<T>(r));
}

inline constexpr x11::Input::ValuatorStateModeMask operator|(
    x11::Input::ValuatorStateModeMask l,
    x11::Input::ValuatorStateModeMask r) {
  using T = std::underlying_type_t<x11::Input::ValuatorStateModeMask>;
  return static_cast<x11::Input::ValuatorStateModeMask>(static_cast<T>(l) |
                                                        static_cast<T>(r));
}

inline constexpr x11::Input::ValuatorStateModeMask operator&(
    x11::Input::ValuatorStateModeMask l,
    x11::Input::ValuatorStateModeMask r) {
  using T = std::underlying_type_t<x11::Input::ValuatorStateModeMask>;
  return static_cast<x11::Input::ValuatorStateModeMask>(static_cast<T>(l) &
                                                        static_cast<T>(r));
}

inline constexpr x11::Input::DeviceControl operator|(
    x11::Input::DeviceControl l,
    x11::Input::DeviceControl r) {
  using T = std::underlying_type_t<x11::Input::DeviceControl>;
  return static_cast<x11::Input::DeviceControl>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Input::DeviceControl operator&(
    x11::Input::DeviceControl l,
    x11::Input::DeviceControl r) {
  using T = std::underlying_type_t<x11::Input::DeviceControl>;
  return static_cast<x11::Input::DeviceControl>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Input::PropertyFormat operator|(
    x11::Input::PropertyFormat l,
    x11::Input::PropertyFormat r) {
  using T = std::underlying_type_t<x11::Input::PropertyFormat>;
  return static_cast<x11::Input::PropertyFormat>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Input::PropertyFormat operator&(
    x11::Input::PropertyFormat l,
    x11::Input::PropertyFormat r) {
  using T = std::underlying_type_t<x11::Input::PropertyFormat>;
  return static_cast<x11::Input::PropertyFormat>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Input::DeviceId operator|(x11::Input::DeviceId l,
                                                x11::Input::DeviceId r) {
  using T = std::underlying_type_t<x11::Input::DeviceId>;
  return static_cast<x11::Input::DeviceId>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Input::DeviceId operator&(x11::Input::DeviceId l,
                                                x11::Input::DeviceId r) {
  using T = std::underlying_type_t<x11::Input::DeviceId>;
  return static_cast<x11::Input::DeviceId>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Input::HierarchyChangeType operator|(
    x11::Input::HierarchyChangeType l,
    x11::Input::HierarchyChangeType r) {
  using T = std::underlying_type_t<x11::Input::HierarchyChangeType>;
  return static_cast<x11::Input::HierarchyChangeType>(static_cast<T>(l) |
                                                      static_cast<T>(r));
}

inline constexpr x11::Input::HierarchyChangeType operator&(
    x11::Input::HierarchyChangeType l,
    x11::Input::HierarchyChangeType r) {
  using T = std::underlying_type_t<x11::Input::HierarchyChangeType>;
  return static_cast<x11::Input::HierarchyChangeType>(static_cast<T>(l) &
                                                      static_cast<T>(r));
}

inline constexpr x11::Input::ChangeMode operator|(x11::Input::ChangeMode l,
                                                  x11::Input::ChangeMode r) {
  using T = std::underlying_type_t<x11::Input::ChangeMode>;
  return static_cast<x11::Input::ChangeMode>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Input::ChangeMode operator&(x11::Input::ChangeMode l,
                                                  x11::Input::ChangeMode r) {
  using T = std::underlying_type_t<x11::Input::ChangeMode>;
  return static_cast<x11::Input::ChangeMode>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Input::XIEventMask operator|(x11::Input::XIEventMask l,
                                                   x11::Input::XIEventMask r) {
  using T = std::underlying_type_t<x11::Input::XIEventMask>;
  return static_cast<x11::Input::XIEventMask>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Input::XIEventMask operator&(x11::Input::XIEventMask l,
                                                   x11::Input::XIEventMask r) {
  using T = std::underlying_type_t<x11::Input::XIEventMask>;
  return static_cast<x11::Input::XIEventMask>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Input::DeviceClassType operator|(
    x11::Input::DeviceClassType l,
    x11::Input::DeviceClassType r) {
  using T = std::underlying_type_t<x11::Input::DeviceClassType>;
  return static_cast<x11::Input::DeviceClassType>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::Input::DeviceClassType operator&(
    x11::Input::DeviceClassType l,
    x11::Input::DeviceClassType r) {
  using T = std::underlying_type_t<x11::Input::DeviceClassType>;
  return static_cast<x11::Input::DeviceClassType>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::Input::DeviceType operator|(x11::Input::DeviceType l,
                                                  x11::Input::DeviceType r) {
  using T = std::underlying_type_t<x11::Input::DeviceType>;
  return static_cast<x11::Input::DeviceType>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Input::DeviceType operator&(x11::Input::DeviceType l,
                                                  x11::Input::DeviceType r) {
  using T = std::underlying_type_t<x11::Input::DeviceType>;
  return static_cast<x11::Input::DeviceType>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Input::ScrollFlags operator|(x11::Input::ScrollFlags l,
                                                   x11::Input::ScrollFlags r) {
  using T = std::underlying_type_t<x11::Input::ScrollFlags>;
  return static_cast<x11::Input::ScrollFlags>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Input::ScrollFlags operator&(x11::Input::ScrollFlags l,
                                                   x11::Input::ScrollFlags r) {
  using T = std::underlying_type_t<x11::Input::ScrollFlags>;
  return static_cast<x11::Input::ScrollFlags>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Input::ScrollType operator|(x11::Input::ScrollType l,
                                                  x11::Input::ScrollType r) {
  using T = std::underlying_type_t<x11::Input::ScrollType>;
  return static_cast<x11::Input::ScrollType>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Input::ScrollType operator&(x11::Input::ScrollType l,
                                                  x11::Input::ScrollType r) {
  using T = std::underlying_type_t<x11::Input::ScrollType>;
  return static_cast<x11::Input::ScrollType>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Input::TouchMode operator|(x11::Input::TouchMode l,
                                                 x11::Input::TouchMode r) {
  using T = std::underlying_type_t<x11::Input::TouchMode>;
  return static_cast<x11::Input::TouchMode>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Input::TouchMode operator&(x11::Input::TouchMode l,
                                                 x11::Input::TouchMode r) {
  using T = std::underlying_type_t<x11::Input::TouchMode>;
  return static_cast<x11::Input::TouchMode>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Input::GrabOwner operator|(x11::Input::GrabOwner l,
                                                 x11::Input::GrabOwner r) {
  using T = std::underlying_type_t<x11::Input::GrabOwner>;
  return static_cast<x11::Input::GrabOwner>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Input::GrabOwner operator&(x11::Input::GrabOwner l,
                                                 x11::Input::GrabOwner r) {
  using T = std::underlying_type_t<x11::Input::GrabOwner>;
  return static_cast<x11::Input::GrabOwner>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Input::EventMode operator|(x11::Input::EventMode l,
                                                 x11::Input::EventMode r) {
  using T = std::underlying_type_t<x11::Input::EventMode>;
  return static_cast<x11::Input::EventMode>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Input::EventMode operator&(x11::Input::EventMode l,
                                                 x11::Input::EventMode r) {
  using T = std::underlying_type_t<x11::Input::EventMode>;
  return static_cast<x11::Input::EventMode>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Input::GrabMode22 operator|(x11::Input::GrabMode22 l,
                                                  x11::Input::GrabMode22 r) {
  using T = std::underlying_type_t<x11::Input::GrabMode22>;
  return static_cast<x11::Input::GrabMode22>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Input::GrabMode22 operator&(x11::Input::GrabMode22 l,
                                                  x11::Input::GrabMode22 r) {
  using T = std::underlying_type_t<x11::Input::GrabMode22>;
  return static_cast<x11::Input::GrabMode22>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Input::GrabType operator|(x11::Input::GrabType l,
                                                x11::Input::GrabType r) {
  using T = std::underlying_type_t<x11::Input::GrabType>;
  return static_cast<x11::Input::GrabType>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Input::GrabType operator&(x11::Input::GrabType l,
                                                x11::Input::GrabType r) {
  using T = std::underlying_type_t<x11::Input::GrabType>;
  return static_cast<x11::Input::GrabType>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Input::ModifierMask operator|(
    x11::Input::ModifierMask l,
    x11::Input::ModifierMask r) {
  using T = std::underlying_type_t<x11::Input::ModifierMask>;
  return static_cast<x11::Input::ModifierMask>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::ModifierMask operator&(
    x11::Input::ModifierMask l,
    x11::Input::ModifierMask r) {
  using T = std::underlying_type_t<x11::Input::ModifierMask>;
  return static_cast<x11::Input::ModifierMask>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::MoreEventsMask operator|(
    x11::Input::MoreEventsMask l,
    x11::Input::MoreEventsMask r) {
  using T = std::underlying_type_t<x11::Input::MoreEventsMask>;
  return static_cast<x11::Input::MoreEventsMask>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Input::MoreEventsMask operator&(
    x11::Input::MoreEventsMask l,
    x11::Input::MoreEventsMask r) {
  using T = std::underlying_type_t<x11::Input::MoreEventsMask>;
  return static_cast<x11::Input::MoreEventsMask>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Input::ClassesReportedMask operator|(
    x11::Input::ClassesReportedMask l,
    x11::Input::ClassesReportedMask r) {
  using T = std::underlying_type_t<x11::Input::ClassesReportedMask>;
  return static_cast<x11::Input::ClassesReportedMask>(static_cast<T>(l) |
                                                      static_cast<T>(r));
}

inline constexpr x11::Input::ClassesReportedMask operator&(
    x11::Input::ClassesReportedMask l,
    x11::Input::ClassesReportedMask r) {
  using T = std::underlying_type_t<x11::Input::ClassesReportedMask>;
  return static_cast<x11::Input::ClassesReportedMask>(static_cast<T>(l) &
                                                      static_cast<T>(r));
}

inline constexpr x11::Input::ChangeDevice operator|(
    x11::Input::ChangeDevice l,
    x11::Input::ChangeDevice r) {
  using T = std::underlying_type_t<x11::Input::ChangeDevice>;
  return static_cast<x11::Input::ChangeDevice>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::ChangeDevice operator&(
    x11::Input::ChangeDevice l,
    x11::Input::ChangeDevice r) {
  using T = std::underlying_type_t<x11::Input::ChangeDevice>;
  return static_cast<x11::Input::ChangeDevice>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::DeviceChange operator|(
    x11::Input::DeviceChange l,
    x11::Input::DeviceChange r) {
  using T = std::underlying_type_t<x11::Input::DeviceChange>;
  return static_cast<x11::Input::DeviceChange>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::DeviceChange operator&(
    x11::Input::DeviceChange l,
    x11::Input::DeviceChange r) {
  using T = std::underlying_type_t<x11::Input::DeviceChange>;
  return static_cast<x11::Input::DeviceChange>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::ChangeReason operator|(
    x11::Input::ChangeReason l,
    x11::Input::ChangeReason r) {
  using T = std::underlying_type_t<x11::Input::ChangeReason>;
  return static_cast<x11::Input::ChangeReason>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::ChangeReason operator&(
    x11::Input::ChangeReason l,
    x11::Input::ChangeReason r) {
  using T = std::underlying_type_t<x11::Input::ChangeReason>;
  return static_cast<x11::Input::ChangeReason>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::KeyEventFlags operator|(
    x11::Input::KeyEventFlags l,
    x11::Input::KeyEventFlags r) {
  using T = std::underlying_type_t<x11::Input::KeyEventFlags>;
  return static_cast<x11::Input::KeyEventFlags>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Input::KeyEventFlags operator&(
    x11::Input::KeyEventFlags l,
    x11::Input::KeyEventFlags r) {
  using T = std::underlying_type_t<x11::Input::KeyEventFlags>;
  return static_cast<x11::Input::KeyEventFlags>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Input::PointerEventFlags operator|(
    x11::Input::PointerEventFlags l,
    x11::Input::PointerEventFlags r) {
  using T = std::underlying_type_t<x11::Input::PointerEventFlags>;
  return static_cast<x11::Input::PointerEventFlags>(static_cast<T>(l) |
                                                    static_cast<T>(r));
}

inline constexpr x11::Input::PointerEventFlags operator&(
    x11::Input::PointerEventFlags l,
    x11::Input::PointerEventFlags r) {
  using T = std::underlying_type_t<x11::Input::PointerEventFlags>;
  return static_cast<x11::Input::PointerEventFlags>(static_cast<T>(l) &
                                                    static_cast<T>(r));
}

inline constexpr x11::Input::NotifyMode operator|(x11::Input::NotifyMode l,
                                                  x11::Input::NotifyMode r) {
  using T = std::underlying_type_t<x11::Input::NotifyMode>;
  return static_cast<x11::Input::NotifyMode>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Input::NotifyMode operator&(x11::Input::NotifyMode l,
                                                  x11::Input::NotifyMode r) {
  using T = std::underlying_type_t<x11::Input::NotifyMode>;
  return static_cast<x11::Input::NotifyMode>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Input::NotifyDetail operator|(
    x11::Input::NotifyDetail l,
    x11::Input::NotifyDetail r) {
  using T = std::underlying_type_t<x11::Input::NotifyDetail>;
  return static_cast<x11::Input::NotifyDetail>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::NotifyDetail operator&(
    x11::Input::NotifyDetail l,
    x11::Input::NotifyDetail r) {
  using T = std::underlying_type_t<x11::Input::NotifyDetail>;
  return static_cast<x11::Input::NotifyDetail>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::HierarchyMask operator|(
    x11::Input::HierarchyMask l,
    x11::Input::HierarchyMask r) {
  using T = std::underlying_type_t<x11::Input::HierarchyMask>;
  return static_cast<x11::Input::HierarchyMask>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Input::HierarchyMask operator&(
    x11::Input::HierarchyMask l,
    x11::Input::HierarchyMask r) {
  using T = std::underlying_type_t<x11::Input::HierarchyMask>;
  return static_cast<x11::Input::HierarchyMask>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Input::PropertyFlag operator|(
    x11::Input::PropertyFlag l,
    x11::Input::PropertyFlag r) {
  using T = std::underlying_type_t<x11::Input::PropertyFlag>;
  return static_cast<x11::Input::PropertyFlag>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::PropertyFlag operator&(
    x11::Input::PropertyFlag l,
    x11::Input::PropertyFlag r) {
  using T = std::underlying_type_t<x11::Input::PropertyFlag>;
  return static_cast<x11::Input::PropertyFlag>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::TouchEventFlags operator|(
    x11::Input::TouchEventFlags l,
    x11::Input::TouchEventFlags r) {
  using T = std::underlying_type_t<x11::Input::TouchEventFlags>;
  return static_cast<x11::Input::TouchEventFlags>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::Input::TouchEventFlags operator&(
    x11::Input::TouchEventFlags l,
    x11::Input::TouchEventFlags r) {
  using T = std::underlying_type_t<x11::Input::TouchEventFlags>;
  return static_cast<x11::Input::TouchEventFlags>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::Input::TouchOwnershipFlags operator|(
    x11::Input::TouchOwnershipFlags l,
    x11::Input::TouchOwnershipFlags r) {
  using T = std::underlying_type_t<x11::Input::TouchOwnershipFlags>;
  return static_cast<x11::Input::TouchOwnershipFlags>(static_cast<T>(l) |
                                                      static_cast<T>(r));
}

inline constexpr x11::Input::TouchOwnershipFlags operator&(
    x11::Input::TouchOwnershipFlags l,
    x11::Input::TouchOwnershipFlags r) {
  using T = std::underlying_type_t<x11::Input::TouchOwnershipFlags>;
  return static_cast<x11::Input::TouchOwnershipFlags>(static_cast<T>(l) &
                                                      static_cast<T>(r));
}

inline constexpr x11::Input::BarrierFlags operator|(
    x11::Input::BarrierFlags l,
    x11::Input::BarrierFlags r) {
  using T = std::underlying_type_t<x11::Input::BarrierFlags>;
  return static_cast<x11::Input::BarrierFlags>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Input::BarrierFlags operator&(
    x11::Input::BarrierFlags l,
    x11::Input::BarrierFlags r) {
  using T = std::underlying_type_t<x11::Input::BarrierFlags>;
  return static_cast<x11::Input::BarrierFlags>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Input::GesturePinchEventFlags operator|(
    x11::Input::GesturePinchEventFlags l,
    x11::Input::GesturePinchEventFlags r) {
  using T = std::underlying_type_t<x11::Input::GesturePinchEventFlags>;
  return static_cast<x11::Input::GesturePinchEventFlags>(static_cast<T>(l) |
                                                         static_cast<T>(r));
}

inline constexpr x11::Input::GesturePinchEventFlags operator&(
    x11::Input::GesturePinchEventFlags l,
    x11::Input::GesturePinchEventFlags r) {
  using T = std::underlying_type_t<x11::Input::GesturePinchEventFlags>;
  return static_cast<x11::Input::GesturePinchEventFlags>(static_cast<T>(l) &
                                                         static_cast<T>(r));
}

inline constexpr x11::Input::GestureSwipeEventFlags operator|(
    x11::Input::GestureSwipeEventFlags l,
    x11::Input::GestureSwipeEventFlags r) {
  using T = std::underlying_type_t<x11::Input::GestureSwipeEventFlags>;
  return static_cast<x11::Input::GestureSwipeEventFlags>(static_cast<T>(l) |
                                                         static_cast<T>(r));
}

inline constexpr x11::Input::GestureSwipeEventFlags operator&(
    x11::Input::GestureSwipeEventFlags l,
    x11::Input::GestureSwipeEventFlags r) {
  using T = std::underlying_type_t<x11::Input::GestureSwipeEventFlags>;
  return static_cast<x11::Input::GestureSwipeEventFlags>(static_cast<T>(l) &
                                                         static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XINPUT_H_
