// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    composite \
//    damage \
//    dpms \
//    dri2 \
//    dri3 \
//    ge \
//    glx \
//    present \
//    randr \
//    record \
//    render \
//    res \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xc_misc \
//    xevie \
//    xf86dri \
//    xf86vidmode \
//    xfixes \
//    xinerama \
//    xinput \
//    xkb \
//    xprint \
//    xproto \
//    xselinux \
//    xtest \
//    xv \
//    xvmc

#ifndef UI_GFX_X_GENERATED_PROTOS_XKB_H_
#define UI_GFX_X_GENERATED_PROTOS_XKB_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Xkb {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 0;

  Xkb(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Const : int {
    MaxLegalKeyCode = 255,
    PerKeyBitArraySize = 32,
    KeyNameLength = 4,
  };

  enum class EventType : int {
    NewKeyboardNotify = 1 << 0,
    MapNotify = 1 << 1,
    StateNotify = 1 << 2,
    ControlsNotify = 1 << 3,
    IndicatorStateNotify = 1 << 4,
    IndicatorMapNotify = 1 << 5,
    NamesNotify = 1 << 6,
    CompatMapNotify = 1 << 7,
    BellNotify = 1 << 8,
    ActionMessage = 1 << 9,
    AccessXNotify = 1 << 10,
    ExtensionDeviceNotify = 1 << 11,
  };

  enum class NKNDetail : int {
    Keycodes = 1 << 0,
    Geometry = 1 << 1,
    DeviceID = 1 << 2,
  };

  enum class AXNDetail : int {
    SKPress = 1 << 0,
    SKAccept = 1 << 1,
    SKReject = 1 << 2,
    SKRelease = 1 << 3,
    BKAccept = 1 << 4,
    BKReject = 1 << 5,
    AXKWarning = 1 << 6,
  };

  enum class MapPart : int {
    KeyTypes = 1 << 0,
    KeySyms = 1 << 1,
    ModifierMap = 1 << 2,
    ExplicitComponents = 1 << 3,
    KeyActions = 1 << 4,
    KeyBehaviors = 1 << 5,
    VirtualMods = 1 << 6,
    VirtualModMap = 1 << 7,
  };

  enum class SetMapFlags : int {
    ResizeTypes = 1 << 0,
    RecomputeActions = 1 << 1,
  };

  enum class StatePart : int {
    ModifierState = 1 << 0,
    ModifierBase = 1 << 1,
    ModifierLatch = 1 << 2,
    ModifierLock = 1 << 3,
    GroupState = 1 << 4,
    GroupBase = 1 << 5,
    GroupLatch = 1 << 6,
    GroupLock = 1 << 7,
    CompatState = 1 << 8,
    GrabMods = 1 << 9,
    CompatGrabMods = 1 << 10,
    LookupMods = 1 << 11,
    CompatLookupMods = 1 << 12,
    PointerButtons = 1 << 13,
  };

  enum class BoolCtrl : int {
    RepeatKeys = 1 << 0,
    SlowKeys = 1 << 1,
    BounceKeys = 1 << 2,
    StickyKeys = 1 << 3,
    MouseKeys = 1 << 4,
    MouseKeysAccel = 1 << 5,
    AccessXKeys = 1 << 6,
    AccessXTimeoutMask = 1 << 7,
    AccessXFeedbackMask = 1 << 8,
    AudibleBellMask = 1 << 9,
    Overlay1Mask = 1 << 10,
    Overlay2Mask = 1 << 11,
    IgnoreGroupLockMask = 1 << 12,
  };

  enum class Control : int {
    GroupsWrap = 1 << 27,
    InternalMods = 1 << 28,
    IgnoreLockMods = 1 << 29,
    PerKeyRepeat = 1 << 30,
    ControlsEnabled = 1 << 31,
  };

  enum class AXOption : int {
    SKPressFB = 1 << 0,
    SKAcceptFB = 1 << 1,
    FeatureFB = 1 << 2,
    SlowWarnFB = 1 << 3,
    IndicatorFB = 1 << 4,
    StickyKeysFB = 1 << 5,
    TwoKeys = 1 << 6,
    LatchToLock = 1 << 7,
    SKReleaseFB = 1 << 8,
    SKRejectFB = 1 << 9,
    BKRejectFB = 1 << 10,
    DumbBell = 1 << 11,
  };

  enum class DeviceSpec : uint16_t {};

  enum class LedClassResult : int {
    KbdFeedbackClass = 0,
    LedFeedbackClass = 4,
  };

  enum class LedClass : int {
    KbdFeedbackClass = 0,
    LedFeedbackClass = 4,
    DfltXIClass = 768,
    AllXIClasses = 1280,
  };

  enum class LedClassSpec : uint16_t {};

  enum class BellClassResult : int {
    KbdFeedbackClass = 0,
    BellFeedbackClass = 5,
  };

  enum class BellClass : int {
    KbdFeedbackClass = 0,
    BellFeedbackClass = 5,
    DfltXIClass = 768,
  };

  enum class BellClassSpec : uint16_t {};

  enum class Id : int {
    UseCoreKbd = 256,
    UseCorePtr = 512,
    DfltXIClass = 768,
    DfltXIId = 1024,
    AllXIClass = 1280,
    AllXIId = 1536,
    XINone = 65280,
  };

  enum class IDSpec : uint16_t {};

  enum class Group : int {
    c_1 = 0,
    c_2 = 1,
    c_3 = 2,
    c_4 = 3,
  };

  enum class Groups : int {
    Any = 254,
    All = 255,
  };

  enum class SetOfGroup : int {
    Group1 = 1 << 0,
    Group2 = 1 << 1,
    Group3 = 1 << 2,
    Group4 = 1 << 3,
  };

  enum class SetOfGroups : int {
    Any = 1 << 7,
  };

  enum class GroupsWrap : int {
    WrapIntoRange = 0,
    ClampIntoRange = 1 << 6,
    RedirectIntoRange = 1 << 7,
  };

  enum class VModsHigh : int {
    c_15 = 1 << 7,
    c_14 = 1 << 6,
    c_13 = 1 << 5,
    c_12 = 1 << 4,
    c_11 = 1 << 3,
    c_10 = 1 << 2,
    c_9 = 1 << 1,
    c_8 = 1 << 0,
  };

  enum class VModsLow : int {
    c_7 = 1 << 7,
    c_6 = 1 << 6,
    c_5 = 1 << 5,
    c_4 = 1 << 4,
    c_3 = 1 << 3,
    c_2 = 1 << 2,
    c_1 = 1 << 1,
    c_0 = 1 << 0,
  };

  enum class VMod : int {
    c_15 = 1 << 15,
    c_14 = 1 << 14,
    c_13 = 1 << 13,
    c_12 = 1 << 12,
    c_11 = 1 << 11,
    c_10 = 1 << 10,
    c_9 = 1 << 9,
    c_8 = 1 << 8,
    c_7 = 1 << 7,
    c_6 = 1 << 6,
    c_5 = 1 << 5,
    c_4 = 1 << 4,
    c_3 = 1 << 3,
    c_2 = 1 << 2,
    c_1 = 1 << 1,
    c_0 = 1 << 0,
  };

  enum class Explicit : int {
    VModMap = 1 << 7,
    Behavior = 1 << 6,
    AutoRepeat = 1 << 5,
    Interpret = 1 << 4,
    KeyType4 = 1 << 3,
    KeyType3 = 1 << 2,
    KeyType2 = 1 << 1,
    KeyType1 = 1 << 0,
  };

  enum class SymInterpretMatch : int {
    NoneOf = 0,
    AnyOfOrNone = 1,
    AnyOf = 2,
    AllOf = 3,
    Exactly = 4,
  };

  enum class SymInterpMatch : int {
    OpMask = 127,
    LevelOneOnly = 1 << 7,
  };

  enum class IMFlag : int {
    NoExplicit = 1 << 7,
    NoAutomatic = 1 << 6,
    LEDDrivesKB = 1 << 5,
  };

  enum class IMModsWhich : int {
    UseCompat = 1 << 4,
    UseEffective = 1 << 3,
    UseLocked = 1 << 2,
    UseLatched = 1 << 1,
    UseBase = 1 << 0,
  };

  enum class IMGroupsWhich : int {
    UseCompat = 1 << 4,
    UseEffective = 1 << 3,
    UseLocked = 1 << 2,
    UseLatched = 1 << 1,
    UseBase = 1 << 0,
  };

  enum class CMDetail : int {
    SymInterp = 1 << 0,
    GroupCompat = 1 << 1,
  };

  enum class NameDetail : int {
    Keycodes = 1 << 0,
    Geometry = 1 << 1,
    Symbols = 1 << 2,
    PhysSymbols = 1 << 3,
    Types = 1 << 4,
    Compat = 1 << 5,
    KeyTypeNames = 1 << 6,
    KTLevelNames = 1 << 7,
    IndicatorNames = 1 << 8,
    KeyNames = 1 << 9,
    KeyAliases = 1 << 10,
    VirtualModNames = 1 << 11,
    GroupNames = 1 << 12,
    RGNames = 1 << 13,
  };

  enum class GBNDetail : int {
    Types = 1 << 0,
    CompatMap = 1 << 1,
    ClientSymbols = 1 << 2,
    ServerSymbols = 1 << 3,
    IndicatorMaps = 1 << 4,
    KeyNames = 1 << 5,
    Geometry = 1 << 6,
    OtherNames = 1 << 7,
  };

  enum class XIFeature : int {
    Keyboards = 1 << 0,
    ButtonActions = 1 << 1,
    IndicatorNames = 1 << 2,
    IndicatorMaps = 1 << 3,
    IndicatorState = 1 << 4,
  };

  enum class PerClientFlag : int {
    DetectableAutoRepeat = 1 << 0,
    GrabsUseXKBState = 1 << 1,
    AutoResetControls = 1 << 2,
    LookupStateWhenGrabbed = 1 << 3,
    SendEventUsesXKBState = 1 << 4,
  };

  enum class BehaviorType : int {
    Default = 0,
    Lock = 1,
    RadioGroup = 2,
    Overlay1 = 3,
    Overlay2 = 4,
    PermamentLock = 129,
    PermamentRadioGroup = 130,
    PermamentOverlay1 = 131,
    PermamentOverlay2 = 132,
  };

  enum class String8 : char {};

  enum class DoodadType : int {
    Outline = 1,
    Solid = 2,
    Text = 3,
    Indicator = 4,
    Logo = 5,
  };

  enum class Error : int {
    BadDevice = 255,
    BadClass = 254,
    BadId = 253,
  };

  enum class Sa : int {
    ClearLocks = 1 << 0,
    LatchToLock = 1 << 1,
    UseModMapMods = 1 << 2,
    GroupAbsolute = 1 << 2,
  };

  enum class SAType : int {
    NoAction = 0,
    SetMods = 1,
    LatchMods = 2,
    LockMods = 3,
    SetGroup = 4,
    LatchGroup = 5,
    LockGroup = 6,
    MovePtr = 7,
    PtrBtn = 8,
    LockPtrBtn = 9,
    SetPtrDflt = 10,
    ISOLock = 11,
    Terminate = 12,
    SwitchScreen = 13,
    SetControls = 14,
    LockControls = 15,
    ActionMessage = 16,
    RedirectKey = 17,
    DeviceBtn = 18,
    LockDeviceBtn = 19,
    DeviceValuator = 20,
  };

  enum class SAMovePtrFlag : int {
    NoAcceleration = 1 << 0,
    MoveAbsoluteX = 1 << 1,
    MoveAbsoluteY = 1 << 2,
  };

  enum class SASetPtrDfltFlag : int {
    DfltBtnAbsolute = 1 << 2,
    AffectDfltButton = 1 << 0,
  };

  enum class SAIsoLockFlag : int {
    NoLock = 1 << 0,
    NoUnlock = 1 << 1,
    UseModMapMods = 1 << 2,
    GroupAbsolute = 1 << 2,
    ISODfltIsGroup = 1 << 3,
  };

  enum class SAIsoLockNoAffect : int {
    Ctrls = 1 << 3,
    Ptr = 1 << 4,
    Group = 1 << 5,
    Mods = 1 << 6,
  };

  enum class SwitchScreenFlag : int {
    Application = 1 << 0,
    Absolute = 1 << 2,
  };

  enum class BoolCtrlsHigh : int {
    AccessXFeedback = 1 << 0,
    AudibleBell = 1 << 1,
    Overlay1 = 1 << 2,
    Overlay2 = 1 << 3,
    IgnoreGroupLock = 1 << 4,
  };

  enum class BoolCtrlsLow : int {
    RepeatKeys = 1 << 0,
    SlowKeys = 1 << 1,
    BounceKeys = 1 << 2,
    StickyKeys = 1 << 3,
    MouseKeys = 1 << 4,
    MouseKeysAccel = 1 << 5,
    AccessXKeys = 1 << 6,
    AccessXTimeout = 1 << 7,
  };

  enum class ActionMessageFlag : int {
    OnPress = 1 << 0,
    OnRelease = 1 << 1,
    GenKeyEvent = 1 << 2,
  };

  enum class LockDeviceFlags : int {
    NoLock = 1 << 0,
    NoUnlock = 1 << 1,
  };

  enum class SAValWhat : int {
    IgnoreVal = 0,
    SetValMin = 1,
    SetValCenter = 2,
    SetValMax = 3,
    SetValRelative = 4,
    SetValAbsolute = 5,
  };

  struct IndicatorMap {
    IMFlag flags{};
    IMGroupsWhich whichGroups{};
    SetOfGroup groups{};
    IMModsWhich whichMods{};
    ModMask mods{};
    ModMask realMods{};
    VMod vmods{};
    BoolCtrl ctrls{};
  };

  struct ModDef {
    ModMask mask{};
    ModMask realMods{};
    VMod vmods{};
  };

  struct KeyName {
    std::array<char, 4> name{};
  };

  struct KeyAlias {
    std::array<char, 4> real{};
    std::array<char, 4> alias{};
  };

  struct CountedString16 {
    std::string string{};
    scoped_refptr<base::RefCountedMemory> alignment_pad{};
  };

  struct KTMapEntry {
    uint8_t active{};
    ModMask mods_mask{};
    uint8_t level{};
    ModMask mods_mods{};
    VMod mods_vmods{};
  };

  struct KeyType {
    ModMask mods_mask{};
    ModMask mods_mods{};
    VMod mods_vmods{};
    uint8_t numLevels{};
    uint8_t hasPreserve{};
    std::vector<KTMapEntry> map{};
    std::vector<ModDef> preserve{};
  };

  struct KeySymMap {
    std::array<uint8_t, 4> kt_index{};
    uint8_t groupInfo{};
    uint8_t width{};
    std::vector<KeySym> syms{};
  };

  struct CommonBehavior {
    uint8_t type{};
    uint8_t data{};
  };

  struct DefaultBehavior {
    uint8_t type{};
  };

  struct LockBehavior {
    uint8_t type{};
  };

  struct RadioGroupBehavior {
    uint8_t type{};
    uint8_t group{};
  };

  struct OverlayBehavior {
    uint8_t type{};
    KeyCode key{};
  };

  struct PermamentLockBehavior {
    uint8_t type{};
  };

  struct PermamentRadioGroupBehavior {
    uint8_t type{};
    uint8_t group{};
  };

  struct PermamentOverlayBehavior {
    uint8_t type{};
    KeyCode key{};
  };

  union Behavior {
    Behavior() { memset(this, 0, sizeof(*this)); }

    CommonBehavior common;
    DefaultBehavior c_default;
    LockBehavior lock;
    RadioGroupBehavior radioGroup;
    OverlayBehavior overlay1;
    OverlayBehavior overlay2;
    PermamentLockBehavior permamentLock;
    PermamentRadioGroupBehavior permamentRadioGroup;
    PermamentOverlayBehavior permamentOverlay1;
    PermamentOverlayBehavior permamentOverlay2;
    uint8_t type;
  };
  static_assert(std::is_trivially_copyable<Behavior>::value, "");

  struct SetBehavior {
    KeyCode keycode{};
    Behavior behavior{};
  };

  struct SetExplicit {
    KeyCode keycode{};
    Explicit c_explicit{};
  };

  struct KeyModMap {
    KeyCode keycode{};
    ModMask mods{};
  };

  struct KeyVModMap {
    KeyCode keycode{};
    VMod vmods{};
  };

  struct KTSetMapEntry {
    uint8_t level{};
    ModMask realMods{};
    VMod virtualMods{};
  };

  struct SetKeyType {
    ModMask mask{};
    ModMask realMods{};
    VMod virtualMods{};
    uint8_t numLevels{};
    uint8_t preserve{};
    std::vector<KTSetMapEntry> entries{};
    std::vector<KTSetMapEntry> preserve_entries{};
  };

  struct Outline {
    uint8_t cornerRadius{};
    std::vector<Point> points{};
  };

  struct Shape {
    Atom name{};
    uint8_t primaryNdx{};
    uint8_t approxNdx{};
    std::vector<Outline> outlines{};
  };

  struct Key {
    std::array<String8, 4> name{};
    int16_t gap{};
    uint8_t shapeNdx{};
    uint8_t colorNdx{};
  };

  struct OverlayKey {
    std::array<String8, 4> over{};
    std::array<String8, 4> under{};
  };

  struct OverlayRow {
    uint8_t rowUnder{};
    std::vector<OverlayKey> keys{};
  };

  struct Overlay {
    Atom name{};
    std::vector<OverlayRow> rows{};
  };

  struct Row {
    int16_t top{};
    int16_t left{};
    uint8_t vertical{};
    std::vector<Key> keys{};
  };

  struct Listing {
    uint16_t flags{};
    std::vector<String8> string{};
  };

  struct DeviceLedInfo {
    LedClass ledClass{};
    IDSpec ledID{};
    uint32_t namesPresent{};
    uint32_t mapsPresent{};
    uint32_t physIndicators{};
    uint32_t state{};
    std::vector<Atom> names{};
    std::vector<IndicatorMap> maps{};
  };

  struct KeyboardError : public x11::Error {
    uint16_t sequence{};
    uint32_t value{};
    uint16_t minorOpcode{};
    uint8_t majorOpcode{};

    std::string ToString() const override;
  };

  struct SANoAction {
    SAType type{};
  };

  struct SASetMods {
    SAType type{};
    Sa flags{};
    ModMask mask{};
    ModMask realMods{};
    VModsHigh vmodsHigh{};
    VModsLow vmodsLow{};
  };

  struct SALatchMods {
    SAType type{};
    Sa flags{};
    ModMask mask{};
    ModMask realMods{};
    VModsHigh vmodsHigh{};
    VModsLow vmodsLow{};
  };

  struct SALockMods {
    SAType type{};
    Sa flags{};
    ModMask mask{};
    ModMask realMods{};
    VModsHigh vmodsHigh{};
    VModsLow vmodsLow{};
  };

  struct SASetGroup {
    SAType type{};
    Sa flags{};
    int8_t group{};
  };

  struct SALatchGroup {
    SAType type{};
    Sa flags{};
    int8_t group{};
  };

  struct SALockGroup {
    SAType type{};
    Sa flags{};
    int8_t group{};
  };

  struct SAMovePtr {
    SAType type{};
    SAMovePtrFlag flags{};
    int8_t xHigh{};
    uint8_t xLow{};
    int8_t yHigh{};
    uint8_t yLow{};
  };

  struct SAPtrBtn {
    SAType type{};
    uint8_t flags{};
    uint8_t count{};
    uint8_t button{};
  };

  struct SALockPtrBtn {
    SAType type{};
    uint8_t flags{};
    uint8_t button{};
  };

  struct SASetPtrDflt {
    SAType type{};
    SASetPtrDfltFlag flags{};
    SASetPtrDfltFlag affect{};
    int8_t value{};
  };

  struct SAIsoLock {
    SAType type{};
    SAIsoLockFlag flags{};
    ModMask mask{};
    ModMask realMods{};
    int8_t group{};
    SAIsoLockNoAffect affect{};
    VModsHigh vmodsHigh{};
    VModsLow vmodsLow{};
  };

  struct SATerminate {
    SAType type{};
  };

  struct SASwitchScreen {
    SAType type{};
    uint8_t flags{};
    int8_t newScreen{};
  };

  struct SASetControls {
    SAType type{};
    BoolCtrlsHigh boolCtrlsHigh{};
    BoolCtrlsLow boolCtrlsLow{};
  };

  struct SALockControls {
    SAType type{};
    BoolCtrlsHigh boolCtrlsHigh{};
    BoolCtrlsLow boolCtrlsLow{};
  };

  struct SAActionMessage {
    SAType type{};
    ActionMessageFlag flags{};
    std::array<uint8_t, 6> message{};
  };

  struct SARedirectKey {
    SAType type{};
    KeyCode newkey{};
    ModMask mask{};
    ModMask realModifiers{};
    VModsHigh vmodsMaskHigh{};
    VModsLow vmodsMaskLow{};
    VModsHigh vmodsHigh{};
    VModsLow vmodsLow{};
  };

  struct SADeviceBtn {
    SAType type{};
    uint8_t flags{};
    uint8_t count{};
    uint8_t button{};
    uint8_t device{};
  };

  struct SALockDeviceBtn {
    SAType type{};
    LockDeviceFlags flags{};
    uint8_t button{};
    uint8_t device{};
  };

  struct SADeviceValuator {
    SAType type{};
    uint8_t device{};
    SAValWhat val1what{};
    uint8_t val1index{};
    uint8_t val1value{};
    SAValWhat val2what{};
    uint8_t val2index{};
    uint8_t val2value{};
  };

  struct SIAction {
    SAType type{};
    std::array<uint8_t, 7> data{};
  };

  struct SymInterpret {
    KeySym sym{};
    ModMask mods{};
    uint8_t match{};
    VModsLow virtualMod{};
    uint8_t flags{};
    SIAction action{};
  };

  union Action {
    Action() { memset(this, 0, sizeof(*this)); }

    SANoAction noaction;
    SASetMods setmods;
    SALatchMods latchmods;
    SALockMods lockmods;
    SASetGroup setgroup;
    SALatchGroup latchgroup;
    SALockGroup lockgroup;
    SAMovePtr moveptr;
    SAPtrBtn ptrbtn;
    SALockPtrBtn lockptrbtn;
    SASetPtrDflt setptrdflt;
    SAIsoLock isolock;
    SATerminate terminate;
    SASwitchScreen switchscreen;
    SASetControls setcontrols;
    SALockControls lockcontrols;
    SAActionMessage message;
    SARedirectKey redirect;
    SADeviceBtn devbtn;
    SALockDeviceBtn lockdevbtn;
    SADeviceValuator devval;
    SAType type;
  };
  static_assert(std::is_trivially_copyable<Action>::value, "");

  struct NewKeyboardNotifyEvent {
    static constexpr int type_id = 38;
    static constexpr uint8_t opcode = 0;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    uint8_t oldDeviceID{};
    KeyCode minKeyCode{};
    KeyCode maxKeyCode{};
    KeyCode oldMinKeyCode{};
    KeyCode oldMaxKeyCode{};
    uint8_t requestMajor{};
    uint8_t requestMinor{};
    NKNDetail changed{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct MapNotifyEvent {
    static constexpr int type_id = 39;
    static constexpr uint8_t opcode = 1;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    uint8_t ptrBtnActions{};
    MapPart changed{};
    KeyCode minKeyCode{};
    KeyCode maxKeyCode{};
    uint8_t firstType{};
    uint8_t nTypes{};
    KeyCode firstKeySym{};
    uint8_t nKeySyms{};
    KeyCode firstKeyAct{};
    uint8_t nKeyActs{};
    KeyCode firstKeyBehavior{};
    uint8_t nKeyBehavior{};
    KeyCode firstKeyExplicit{};
    uint8_t nKeyExplicit{};
    KeyCode firstModMapKey{};
    uint8_t nModMapKeys{};
    KeyCode firstVModMapKey{};
    uint8_t nVModMapKeys{};
    VMod virtualMods{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct StateNotifyEvent {
    static constexpr int type_id = 40;
    static constexpr uint8_t opcode = 2;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    ModMask mods{};
    ModMask baseMods{};
    ModMask latchedMods{};
    ModMask lockedMods{};
    Group group{};
    int16_t baseGroup{};
    int16_t latchedGroup{};
    Group lockedGroup{};
    ModMask compatState{};
    ModMask grabMods{};
    ModMask compatGrabMods{};
    ModMask lookupMods{};
    ModMask compatLoockupMods{};
    KeyButMask ptrBtnState{};
    StatePart changed{};
    KeyCode keycode{};
    uint8_t eventType{};
    uint8_t requestMajor{};
    uint8_t requestMinor{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct ControlsNotifyEvent {
    static constexpr int type_id = 41;
    static constexpr uint8_t opcode = 3;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    uint8_t numGroups{};
    Control changedControls{};
    BoolCtrl enabledControls{};
    BoolCtrl enabledControlChanges{};
    KeyCode keycode{};
    uint8_t eventType{};
    uint8_t requestMajor{};
    uint8_t requestMinor{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct IndicatorStateNotifyEvent {
    static constexpr int type_id = 42;
    static constexpr uint8_t opcode = 4;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    uint32_t state{};
    uint32_t stateChanged{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct IndicatorMapNotifyEvent {
    static constexpr int type_id = 43;
    static constexpr uint8_t opcode = 5;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    uint32_t state{};
    uint32_t mapChanged{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct NamesNotifyEvent {
    static constexpr int type_id = 44;
    static constexpr uint8_t opcode = 6;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    NameDetail changed{};
    uint8_t firstType{};
    uint8_t nTypes{};
    uint8_t firstLevelName{};
    uint8_t nLevelNames{};
    uint8_t nRadioGroups{};
    uint8_t nKeyAliases{};
    SetOfGroup changedGroupNames{};
    VMod changedVirtualMods{};
    KeyCode firstKey{};
    uint8_t nKeys{};
    uint32_t changedIndicators{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct CompatMapNotifyEvent {
    static constexpr int type_id = 45;
    static constexpr uint8_t opcode = 7;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    SetOfGroup changedGroups{};
    uint16_t firstSI{};
    uint16_t nSI{};
    uint16_t nTotalSI{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct BellNotifyEvent {
    static constexpr int type_id = 46;
    static constexpr uint8_t opcode = 8;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    BellClassResult bellClass{};
    uint8_t bellID{};
    uint8_t percent{};
    uint16_t pitch{};
    uint16_t duration{};
    Atom name{};
    Window window{};
    uint8_t eventOnly{};

    x11::Window* GetWindow() { return reinterpret_cast<x11::Window*>(&window); }
  };

  struct ActionMessageEvent {
    static constexpr int type_id = 47;
    static constexpr uint8_t opcode = 9;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    KeyCode keycode{};
    uint8_t press{};
    uint8_t keyEventFollows{};
    ModMask mods{};
    Group group{};
    std::array<String8, 8> message{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct AccessXNotifyEvent {
    static constexpr int type_id = 48;
    static constexpr uint8_t opcode = 10;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    KeyCode keycode{};
    AXNDetail detailt{};
    uint16_t slowKeysDelay{};
    uint16_t debounceDelay{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct ExtensionDeviceNotifyEvent {
    static constexpr int type_id = 49;
    static constexpr uint8_t opcode = 11;
    bool send_event{};
    uint8_t xkbType{};
    uint16_t sequence{};
    Time time{};
    uint8_t deviceID{};
    XIFeature reason{};
    LedClassResult ledClass{};
    uint16_t ledID{};
    uint32_t ledsDefined{};
    uint32_t ledState{};
    uint8_t firstButton{};
    uint8_t nButtons{};
    XIFeature supported{};
    XIFeature unsupported{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct UseExtensionRequest {
    uint16_t wantedMajor{};
    uint16_t wantedMinor{};
  };

  struct UseExtensionReply {
    uint8_t supported{};
    uint16_t sequence{};
    uint16_t serverMajor{};
    uint16_t serverMinor{};
  };

  using UseExtensionResponse = Response<UseExtensionReply>;

  Future<UseExtensionReply> UseExtension(const UseExtensionRequest& request);

  Future<UseExtensionReply> UseExtension(const uint16_t& wantedMajor = {},
                                         const uint16_t& wantedMinor = {});

  struct SelectEventsRequest {
    DeviceSpec deviceSpec{};
    EventType affectWhich{};
    EventType clear{};
    EventType selectAll{};
    MapPart affectMap{};
    MapPart map{};
    base::Optional<NKNDetail> affectNewKeyboard{};
    base::Optional<NKNDetail> newKeyboardDetails{};
    base::Optional<StatePart> affectState{};
    base::Optional<StatePart> stateDetails{};
    base::Optional<Control> affectCtrls{};
    base::Optional<Control> ctrlDetails{};
    base::Optional<uint32_t> affectIndicatorState{};
    base::Optional<uint32_t> indicatorStateDetails{};
    base::Optional<uint32_t> affectIndicatorMap{};
    base::Optional<uint32_t> indicatorMapDetails{};
    base::Optional<NameDetail> affectNames{};
    base::Optional<NameDetail> namesDetails{};
    base::Optional<CMDetail> affectCompat{};
    base::Optional<CMDetail> compatDetails{};
    base::Optional<uint8_t> affectBell{};
    base::Optional<uint8_t> bellDetails{};
    base::Optional<uint8_t> affectMsgDetails{};
    base::Optional<uint8_t> msgDetails{};
    base::Optional<AXNDetail> affectAccessX{};
    base::Optional<AXNDetail> accessXDetails{};
    base::Optional<XIFeature> affectExtDev{};
    base::Optional<XIFeature> extdevDetails{};
  };

  using SelectEventsResponse = Response<void>;

  Future<void> SelectEvents(const SelectEventsRequest& request);

  Future<void> SelectEvents(
      const DeviceSpec& deviceSpec = {},
      const EventType& affectWhich = {},
      const EventType& clear = {},
      const EventType& selectAll = {},
      const MapPart& affectMap = {},
      const MapPart& map = {},
      const base::Optional<NKNDetail>& affectNewKeyboard = base::nullopt,
      const base::Optional<NKNDetail>& newKeyboardDetails = base::nullopt,
      const base::Optional<StatePart>& affectState = base::nullopt,
      const base::Optional<StatePart>& stateDetails = base::nullopt,
      const base::Optional<Control>& affectCtrls = base::nullopt,
      const base::Optional<Control>& ctrlDetails = base::nullopt,
      const base::Optional<uint32_t>& affectIndicatorState = base::nullopt,
      const base::Optional<uint32_t>& indicatorStateDetails = base::nullopt,
      const base::Optional<uint32_t>& affectIndicatorMap = base::nullopt,
      const base::Optional<uint32_t>& indicatorMapDetails = base::nullopt,
      const base::Optional<NameDetail>& affectNames = base::nullopt,
      const base::Optional<NameDetail>& namesDetails = base::nullopt,
      const base::Optional<CMDetail>& affectCompat = base::nullopt,
      const base::Optional<CMDetail>& compatDetails = base::nullopt,
      const base::Optional<uint8_t>& affectBell = base::nullopt,
      const base::Optional<uint8_t>& bellDetails = base::nullopt,
      const base::Optional<uint8_t>& affectMsgDetails = base::nullopt,
      const base::Optional<uint8_t>& msgDetails = base::nullopt,
      const base::Optional<AXNDetail>& affectAccessX = base::nullopt,
      const base::Optional<AXNDetail>& accessXDetails = base::nullopt,
      const base::Optional<XIFeature>& affectExtDev = base::nullopt,
      const base::Optional<XIFeature>& extdevDetails = base::nullopt);

  struct BellRequest {
    DeviceSpec deviceSpec{};
    BellClassSpec bellClass{};
    IDSpec bellID{};
    int8_t percent{};
    uint8_t forceSound{};
    uint8_t eventOnly{};
    int16_t pitch{};
    int16_t duration{};
    Atom name{};
    Window window{};
  };

  using BellResponse = Response<void>;

  Future<void> Bell(const BellRequest& request);

  Future<void> Bell(const DeviceSpec& deviceSpec = {},
                    const BellClassSpec& bellClass = {},
                    const IDSpec& bellID = {},
                    const int8_t& percent = {},
                    const uint8_t& forceSound = {},
                    const uint8_t& eventOnly = {},
                    const int16_t& pitch = {},
                    const int16_t& duration = {},
                    const Atom& name = {},
                    const Window& window = {});

  struct GetStateRequest {
    DeviceSpec deviceSpec{};
  };

  struct GetStateReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    ModMask mods{};
    ModMask baseMods{};
    ModMask latchedMods{};
    ModMask lockedMods{};
    Group group{};
    Group lockedGroup{};
    int16_t baseGroup{};
    int16_t latchedGroup{};
    ModMask compatState{};
    ModMask grabMods{};
    ModMask compatGrabMods{};
    ModMask lookupMods{};
    ModMask compatLookupMods{};
    KeyButMask ptrBtnState{};
  };

  using GetStateResponse = Response<GetStateReply>;

  Future<GetStateReply> GetState(const GetStateRequest& request);

  Future<GetStateReply> GetState(const DeviceSpec& deviceSpec = {});

  struct LatchLockStateRequest {
    DeviceSpec deviceSpec{};
    ModMask affectModLocks{};
    ModMask modLocks{};
    uint8_t lockGroup{};
    Group groupLock{};
    ModMask affectModLatches{};
    uint8_t latchGroup{};
    uint16_t groupLatch{};
  };

  using LatchLockStateResponse = Response<void>;

  Future<void> LatchLockState(const LatchLockStateRequest& request);

  Future<void> LatchLockState(const DeviceSpec& deviceSpec = {},
                              const ModMask& affectModLocks = {},
                              const ModMask& modLocks = {},
                              const uint8_t& lockGroup = {},
                              const Group& groupLock = {},
                              const ModMask& affectModLatches = {},
                              const uint8_t& latchGroup = {},
                              const uint16_t& groupLatch = {});

  struct GetControlsRequest {
    DeviceSpec deviceSpec{};
  };

  struct GetControlsReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    uint8_t mouseKeysDfltBtn{};
    uint8_t numGroups{};
    uint8_t groupsWrap{};
    ModMask internalModsMask{};
    ModMask ignoreLockModsMask{};
    ModMask internalModsRealMods{};
    ModMask ignoreLockModsRealMods{};
    VMod internalModsVmods{};
    VMod ignoreLockModsVmods{};
    uint16_t repeatDelay{};
    uint16_t repeatInterval{};
    uint16_t slowKeysDelay{};
    uint16_t debounceDelay{};
    uint16_t mouseKeysDelay{};
    uint16_t mouseKeysInterval{};
    uint16_t mouseKeysTimeToMax{};
    uint16_t mouseKeysMaxSpeed{};
    int16_t mouseKeysCurve{};
    AXOption accessXOption{};
    uint16_t accessXTimeout{};
    AXOption accessXTimeoutOptionsMask{};
    AXOption accessXTimeoutOptionsValues{};
    BoolCtrl accessXTimeoutMask{};
    BoolCtrl accessXTimeoutValues{};
    BoolCtrl enabledControls{};
    std::array<uint8_t, 32> perKeyRepeat{};
  };

  using GetControlsResponse = Response<GetControlsReply>;

  Future<GetControlsReply> GetControls(const GetControlsRequest& request);

  Future<GetControlsReply> GetControls(const DeviceSpec& deviceSpec = {});

  struct SetControlsRequest {
    DeviceSpec deviceSpec{};
    ModMask affectInternalRealMods{};
    ModMask internalRealMods{};
    ModMask affectIgnoreLockRealMods{};
    ModMask ignoreLockRealMods{};
    VMod affectInternalVirtualMods{};
    VMod internalVirtualMods{};
    VMod affectIgnoreLockVirtualMods{};
    VMod ignoreLockVirtualMods{};
    uint8_t mouseKeysDfltBtn{};
    uint8_t groupsWrap{};
    AXOption accessXOptions{};
    BoolCtrl affectEnabledControls{};
    BoolCtrl enabledControls{};
    Control changeControls{};
    uint16_t repeatDelay{};
    uint16_t repeatInterval{};
    uint16_t slowKeysDelay{};
    uint16_t debounceDelay{};
    uint16_t mouseKeysDelay{};
    uint16_t mouseKeysInterval{};
    uint16_t mouseKeysTimeToMax{};
    uint16_t mouseKeysMaxSpeed{};
    int16_t mouseKeysCurve{};
    uint16_t accessXTimeout{};
    BoolCtrl accessXTimeoutMask{};
    BoolCtrl accessXTimeoutValues{};
    AXOption accessXTimeoutOptionsMask{};
    AXOption accessXTimeoutOptionsValues{};
    std::array<uint8_t, 32> perKeyRepeat{};
  };

  using SetControlsResponse = Response<void>;

  Future<void> SetControls(const SetControlsRequest& request);

  Future<void> SetControls(const DeviceSpec& deviceSpec = {},
                           const ModMask& affectInternalRealMods = {},
                           const ModMask& internalRealMods = {},
                           const ModMask& affectIgnoreLockRealMods = {},
                           const ModMask& ignoreLockRealMods = {},
                           const VMod& affectInternalVirtualMods = {},
                           const VMod& internalVirtualMods = {},
                           const VMod& affectIgnoreLockVirtualMods = {},
                           const VMod& ignoreLockVirtualMods = {},
                           const uint8_t& mouseKeysDfltBtn = {},
                           const uint8_t& groupsWrap = {},
                           const AXOption& accessXOptions = {},
                           const BoolCtrl& affectEnabledControls = {},
                           const BoolCtrl& enabledControls = {},
                           const Control& changeControls = {},
                           const uint16_t& repeatDelay = {},
                           const uint16_t& repeatInterval = {},
                           const uint16_t& slowKeysDelay = {},
                           const uint16_t& debounceDelay = {},
                           const uint16_t& mouseKeysDelay = {},
                           const uint16_t& mouseKeysInterval = {},
                           const uint16_t& mouseKeysTimeToMax = {},
                           const uint16_t& mouseKeysMaxSpeed = {},
                           const int16_t& mouseKeysCurve = {},
                           const uint16_t& accessXTimeout = {},
                           const BoolCtrl& accessXTimeoutMask = {},
                           const BoolCtrl& accessXTimeoutValues = {},
                           const AXOption& accessXTimeoutOptionsMask = {},
                           const AXOption& accessXTimeoutOptionsValues = {},
                           const std::array<uint8_t, 32>& perKeyRepeat = {});

  struct GetMapRequest {
    DeviceSpec deviceSpec{};
    MapPart full{};
    MapPart partial{};
    uint8_t firstType{};
    uint8_t nTypes{};
    KeyCode firstKeySym{};
    uint8_t nKeySyms{};
    KeyCode firstKeyAction{};
    uint8_t nKeyActions{};
    KeyCode firstKeyBehavior{};
    uint8_t nKeyBehaviors{};
    VMod virtualMods{};
    KeyCode firstKeyExplicit{};
    uint8_t nKeyExplicit{};
    KeyCode firstModMapKey{};
    uint8_t nModMapKeys{};
    KeyCode firstVModMapKey{};
    uint8_t nVModMapKeys{};
  };

  struct GetMapReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    KeyCode minKeyCode{};
    KeyCode maxKeyCode{};
    uint8_t firstType{};
    uint8_t nTypes{};
    uint8_t totalTypes{};
    KeyCode firstKeySym{};
    uint16_t totalSyms{};
    uint8_t nKeySyms{};
    KeyCode firstKeyAction{};
    uint16_t totalActions{};
    uint8_t nKeyActions{};
    KeyCode firstKeyBehavior{};
    uint8_t nKeyBehaviors{};
    uint8_t totalKeyBehaviors{};
    KeyCode firstKeyExplicit{};
    uint8_t nKeyExplicit{};
    uint8_t totalKeyExplicit{};
    KeyCode firstModMapKey{};
    uint8_t nModMapKeys{};
    uint8_t totalModMapKeys{};
    KeyCode firstVModMapKey{};
    uint8_t nVModMapKeys{};
    uint8_t totalVModMapKeys{};
    VMod virtualMods{};
    base::Optional<std::vector<KeyType>> types_rtrn{};
    base::Optional<std::vector<KeySymMap>> syms_rtrn{};
    base::Optional<std::vector<uint8_t>> acts_rtrn_count{};
    base::Optional<std::vector<Action>> acts_rtrn_acts{};
    base::Optional<std::vector<SetBehavior>> behaviors_rtrn{};
    base::Optional<std::vector<ModMask>> vmods_rtrn{};
    base::Optional<std::vector<SetExplicit>> explicit_rtrn{};
    base::Optional<std::vector<KeyModMap>> modmap_rtrn{};
    base::Optional<std::vector<KeyVModMap>> vmodmap_rtrn{};
  };

  using GetMapResponse = Response<GetMapReply>;

  Future<GetMapReply> GetMap(const GetMapRequest& request);

  Future<GetMapReply> GetMap(const DeviceSpec& deviceSpec = {},
                             const MapPart& full = {},
                             const MapPart& partial = {},
                             const uint8_t& firstType = {},
                             const uint8_t& nTypes = {},
                             const KeyCode& firstKeySym = {},
                             const uint8_t& nKeySyms = {},
                             const KeyCode& firstKeyAction = {},
                             const uint8_t& nKeyActions = {},
                             const KeyCode& firstKeyBehavior = {},
                             const uint8_t& nKeyBehaviors = {},
                             const VMod& virtualMods = {},
                             const KeyCode& firstKeyExplicit = {},
                             const uint8_t& nKeyExplicit = {},
                             const KeyCode& firstModMapKey = {},
                             const uint8_t& nModMapKeys = {},
                             const KeyCode& firstVModMapKey = {},
                             const uint8_t& nVModMapKeys = {});

  struct SetMapRequest {
    DeviceSpec deviceSpec{};
    SetMapFlags flags{};
    KeyCode minKeyCode{};
    KeyCode maxKeyCode{};
    uint8_t firstType{};
    uint8_t nTypes{};
    KeyCode firstKeySym{};
    uint8_t nKeySyms{};
    uint16_t totalSyms{};
    KeyCode firstKeyAction{};
    uint8_t nKeyActions{};
    uint16_t totalActions{};
    KeyCode firstKeyBehavior{};
    uint8_t nKeyBehaviors{};
    uint8_t totalKeyBehaviors{};
    KeyCode firstKeyExplicit{};
    uint8_t nKeyExplicit{};
    uint8_t totalKeyExplicit{};
    KeyCode firstModMapKey{};
    uint8_t nModMapKeys{};
    uint8_t totalModMapKeys{};
    KeyCode firstVModMapKey{};
    uint8_t nVModMapKeys{};
    uint8_t totalVModMapKeys{};
    VMod virtualMods{};
    base::Optional<std::vector<SetKeyType>> types{};
    base::Optional<std::vector<KeySymMap>> syms{};
    base::Optional<std::vector<uint8_t>> actionsCount{};
    base::Optional<std::vector<Action>> actions{};
    base::Optional<std::vector<SetBehavior>> behaviors{};
    base::Optional<std::vector<uint8_t>> vmods{};
    base::Optional<std::vector<SetExplicit>> c_explicit{};
    base::Optional<std::vector<KeyModMap>> modmap{};
    base::Optional<std::vector<KeyVModMap>> vmodmap{};
  };

  using SetMapResponse = Response<void>;

  Future<void> SetMap(const SetMapRequest& request);

  Future<void> SetMap(
      const DeviceSpec& deviceSpec = {},
      const SetMapFlags& flags = {},
      const KeyCode& minKeyCode = {},
      const KeyCode& maxKeyCode = {},
      const uint8_t& firstType = {},
      const uint8_t& nTypes = {},
      const KeyCode& firstKeySym = {},
      const uint8_t& nKeySyms = {},
      const uint16_t& totalSyms = {},
      const KeyCode& firstKeyAction = {},
      const uint8_t& nKeyActions = {},
      const uint16_t& totalActions = {},
      const KeyCode& firstKeyBehavior = {},
      const uint8_t& nKeyBehaviors = {},
      const uint8_t& totalKeyBehaviors = {},
      const KeyCode& firstKeyExplicit = {},
      const uint8_t& nKeyExplicit = {},
      const uint8_t& totalKeyExplicit = {},
      const KeyCode& firstModMapKey = {},
      const uint8_t& nModMapKeys = {},
      const uint8_t& totalModMapKeys = {},
      const KeyCode& firstVModMapKey = {},
      const uint8_t& nVModMapKeys = {},
      const uint8_t& totalVModMapKeys = {},
      const VMod& virtualMods = {},
      const base::Optional<std::vector<SetKeyType>>& types = base::nullopt,
      const base::Optional<std::vector<KeySymMap>>& syms = base::nullopt,
      const base::Optional<std::vector<uint8_t>>& actionsCount = base::nullopt,
      const base::Optional<std::vector<Action>>& actions = base::nullopt,
      const base::Optional<std::vector<SetBehavior>>& behaviors = base::nullopt,
      const base::Optional<std::vector<uint8_t>>& vmods = base::nullopt,
      const base::Optional<std::vector<SetExplicit>>& c_explicit =
          base::nullopt,
      const base::Optional<std::vector<KeyModMap>>& modmap = base::nullopt,
      const base::Optional<std::vector<KeyVModMap>>& vmodmap = base::nullopt);

  struct GetCompatMapRequest {
    DeviceSpec deviceSpec{};
    SetOfGroup groups{};
    uint8_t getAllSI{};
    uint16_t firstSI{};
    uint16_t nSI{};
  };

  struct GetCompatMapReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    SetOfGroup groupsRtrn{};
    uint16_t firstSIRtrn{};
    uint16_t nTotalSI{};
    std::vector<SymInterpret> si_rtrn{};
    std::vector<ModDef> group_rtrn{};
  };

  using GetCompatMapResponse = Response<GetCompatMapReply>;

  Future<GetCompatMapReply> GetCompatMap(const GetCompatMapRequest& request);

  Future<GetCompatMapReply> GetCompatMap(const DeviceSpec& deviceSpec = {},
                                         const SetOfGroup& groups = {},
                                         const uint8_t& getAllSI = {},
                                         const uint16_t& firstSI = {},
                                         const uint16_t& nSI = {});

  struct SetCompatMapRequest {
    DeviceSpec deviceSpec{};
    uint8_t recomputeActions{};
    uint8_t truncateSI{};
    SetOfGroup groups{};
    uint16_t firstSI{};
    std::vector<SymInterpret> si{};
    std::vector<ModDef> groupMaps{};
  };

  using SetCompatMapResponse = Response<void>;

  Future<void> SetCompatMap(const SetCompatMapRequest& request);

  Future<void> SetCompatMap(const DeviceSpec& deviceSpec = {},
                            const uint8_t& recomputeActions = {},
                            const uint8_t& truncateSI = {},
                            const SetOfGroup& groups = {},
                            const uint16_t& firstSI = {},
                            const std::vector<SymInterpret>& si = {},
                            const std::vector<ModDef>& groupMaps = {});

  struct GetIndicatorStateRequest {
    DeviceSpec deviceSpec{};
  };

  struct GetIndicatorStateReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    uint32_t state{};
  };

  using GetIndicatorStateResponse = Response<GetIndicatorStateReply>;

  Future<GetIndicatorStateReply> GetIndicatorState(
      const GetIndicatorStateRequest& request);

  Future<GetIndicatorStateReply> GetIndicatorState(
      const DeviceSpec& deviceSpec = {});

  struct GetIndicatorMapRequest {
    DeviceSpec deviceSpec{};
    uint32_t which{};
  };

  struct GetIndicatorMapReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    uint32_t which{};
    uint32_t realIndicators{};
    uint8_t nIndicators{};
    std::vector<IndicatorMap> maps{};
  };

  using GetIndicatorMapResponse = Response<GetIndicatorMapReply>;

  Future<GetIndicatorMapReply> GetIndicatorMap(
      const GetIndicatorMapRequest& request);

  Future<GetIndicatorMapReply> GetIndicatorMap(
      const DeviceSpec& deviceSpec = {},
      const uint32_t& which = {});

  struct SetIndicatorMapRequest {
    DeviceSpec deviceSpec{};
    uint32_t which{};
    std::vector<IndicatorMap> maps{};
  };

  using SetIndicatorMapResponse = Response<void>;

  Future<void> SetIndicatorMap(const SetIndicatorMapRequest& request);

  Future<void> SetIndicatorMap(const DeviceSpec& deviceSpec = {},
                               const uint32_t& which = {},
                               const std::vector<IndicatorMap>& maps = {});

  struct GetNamedIndicatorRequest {
    DeviceSpec deviceSpec{};
    LedClass ledClass{};
    IDSpec ledID{};
    Atom indicator{};
  };

  struct GetNamedIndicatorReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    Atom indicator{};
    uint8_t found{};
    uint8_t on{};
    uint8_t realIndicator{};
    uint8_t ndx{};
    IMFlag map_flags{};
    IMGroupsWhich map_whichGroups{};
    SetOfGroups map_groups{};
    IMModsWhich map_whichMods{};
    ModMask map_mods{};
    ModMask map_realMods{};
    VMod map_vmod{};
    BoolCtrl map_ctrls{};
    uint8_t supported{};
  };

  using GetNamedIndicatorResponse = Response<GetNamedIndicatorReply>;

  Future<GetNamedIndicatorReply> GetNamedIndicator(
      const GetNamedIndicatorRequest& request);

  Future<GetNamedIndicatorReply> GetNamedIndicator(
      const DeviceSpec& deviceSpec = {},
      const LedClass& ledClass = {},
      const IDSpec& ledID = {},
      const Atom& indicator = {});

  struct SetNamedIndicatorRequest {
    DeviceSpec deviceSpec{};
    LedClass ledClass{};
    IDSpec ledID{};
    Atom indicator{};
    uint8_t setState{};
    uint8_t on{};
    uint8_t setMap{};
    uint8_t createMap{};
    IMFlag map_flags{};
    IMGroupsWhich map_whichGroups{};
    SetOfGroups map_groups{};
    IMModsWhich map_whichMods{};
    ModMask map_realMods{};
    VMod map_vmods{};
    BoolCtrl map_ctrls{};
  };

  using SetNamedIndicatorResponse = Response<void>;

  Future<void> SetNamedIndicator(const SetNamedIndicatorRequest& request);

  Future<void> SetNamedIndicator(const DeviceSpec& deviceSpec = {},
                                 const LedClass& ledClass = {},
                                 const IDSpec& ledID = {},
                                 const Atom& indicator = {},
                                 const uint8_t& setState = {},
                                 const uint8_t& on = {},
                                 const uint8_t& setMap = {},
                                 const uint8_t& createMap = {},
                                 const IMFlag& map_flags = {},
                                 const IMGroupsWhich& map_whichGroups = {},
                                 const SetOfGroups& map_groups = {},
                                 const IMModsWhich& map_whichMods = {},
                                 const ModMask& map_realMods = {},
                                 const VMod& map_vmods = {},
                                 const BoolCtrl& map_ctrls = {});

  struct GetNamesRequest {
    DeviceSpec deviceSpec{};
    NameDetail which{};
  };

  struct GetNamesReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    KeyCode minKeyCode{};
    KeyCode maxKeyCode{};
    uint8_t nTypes{};
    SetOfGroup groupNames{};
    VMod virtualMods{};
    KeyCode firstKey{};
    uint8_t nKeys{};
    uint32_t indicators{};
    uint8_t nRadioGroups{};
    uint8_t nKeyAliases{};
    uint16_t nKTLevels{};
    base::Optional<Atom> keycodesName{};
    base::Optional<Atom> geometryName{};
    base::Optional<Atom> symbolsName{};
    base::Optional<Atom> physSymbolsName{};
    base::Optional<Atom> typesName{};
    base::Optional<Atom> compatName{};
    base::Optional<std::vector<Atom>> typeNames{};
    base::Optional<std::vector<uint8_t>> nLevelsPerType{};
    base::Optional<std::vector<Atom>> ktLevelNames{};
    base::Optional<std::vector<Atom>> indicatorNames{};
    base::Optional<std::vector<Atom>> virtualModNames{};
    base::Optional<std::vector<Atom>> groups{};
    base::Optional<std::vector<KeyName>> keyNames{};
    base::Optional<std::vector<KeyAlias>> keyAliases{};
    base::Optional<std::vector<Atom>> radioGroupNames{};
  };

  using GetNamesResponse = Response<GetNamesReply>;

  Future<GetNamesReply> GetNames(const GetNamesRequest& request);

  Future<GetNamesReply> GetNames(const DeviceSpec& deviceSpec = {},
                                 const NameDetail& which = {});

  struct SetNamesRequest {
    DeviceSpec deviceSpec{};
    VMod virtualMods{};
    uint8_t firstType{};
    uint8_t nTypes{};
    uint8_t firstKTLevelt{};
    uint8_t nKTLevels{};
    uint32_t indicators{};
    SetOfGroup groupNames{};
    uint8_t nRadioGroups{};
    KeyCode firstKey{};
    uint8_t nKeys{};
    uint8_t nKeyAliases{};
    uint16_t totalKTLevelNames{};
    base::Optional<Atom> keycodesName{};
    base::Optional<Atom> geometryName{};
    base::Optional<Atom> symbolsName{};
    base::Optional<Atom> physSymbolsName{};
    base::Optional<Atom> typesName{};
    base::Optional<Atom> compatName{};
    base::Optional<std::vector<Atom>> typeNames{};
    base::Optional<std::vector<uint8_t>> nLevelsPerType{};
    base::Optional<std::vector<Atom>> ktLevelNames{};
    base::Optional<std::vector<Atom>> indicatorNames{};
    base::Optional<std::vector<Atom>> virtualModNames{};
    base::Optional<std::vector<Atom>> groups{};
    base::Optional<std::vector<KeyName>> keyNames{};
    base::Optional<std::vector<KeyAlias>> keyAliases{};
    base::Optional<std::vector<Atom>> radioGroupNames{};
  };

  using SetNamesResponse = Response<void>;

  Future<void> SetNames(const SetNamesRequest& request);

  Future<void> SetNames(
      const DeviceSpec& deviceSpec = {},
      const VMod& virtualMods = {},
      const uint8_t& firstType = {},
      const uint8_t& nTypes = {},
      const uint8_t& firstKTLevelt = {},
      const uint8_t& nKTLevels = {},
      const uint32_t& indicators = {},
      const SetOfGroup& groupNames = {},
      const uint8_t& nRadioGroups = {},
      const KeyCode& firstKey = {},
      const uint8_t& nKeys = {},
      const uint8_t& nKeyAliases = {},
      const uint16_t& totalKTLevelNames = {},
      const base::Optional<Atom>& keycodesName = base::nullopt,
      const base::Optional<Atom>& geometryName = base::nullopt,
      const base::Optional<Atom>& symbolsName = base::nullopt,
      const base::Optional<Atom>& physSymbolsName = base::nullopt,
      const base::Optional<Atom>& typesName = base::nullopt,
      const base::Optional<Atom>& compatName = base::nullopt,
      const base::Optional<std::vector<Atom>>& typeNames = base::nullopt,
      const base::Optional<std::vector<uint8_t>>& nLevelsPerType =
          base::nullopt,
      const base::Optional<std::vector<Atom>>& ktLevelNames = base::nullopt,
      const base::Optional<std::vector<Atom>>& indicatorNames = base::nullopt,
      const base::Optional<std::vector<Atom>>& virtualModNames = base::nullopt,
      const base::Optional<std::vector<Atom>>& groups = base::nullopt,
      const base::Optional<std::vector<KeyName>>& keyNames = base::nullopt,
      const base::Optional<std::vector<KeyAlias>>& keyAliases = base::nullopt,
      const base::Optional<std::vector<Atom>>& radioGroupNames = base::nullopt);

  struct PerClientFlagsRequest {
    DeviceSpec deviceSpec{};
    PerClientFlag change{};
    PerClientFlag value{};
    BoolCtrl ctrlsToChange{};
    BoolCtrl autoCtrls{};
    BoolCtrl autoCtrlsValues{};
  };

  struct PerClientFlagsReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    PerClientFlag supported{};
    PerClientFlag value{};
    BoolCtrl autoCtrls{};
    BoolCtrl autoCtrlsValues{};
  };

  using PerClientFlagsResponse = Response<PerClientFlagsReply>;

  Future<PerClientFlagsReply> PerClientFlags(
      const PerClientFlagsRequest& request);

  Future<PerClientFlagsReply> PerClientFlags(
      const DeviceSpec& deviceSpec = {},
      const PerClientFlag& change = {},
      const PerClientFlag& value = {},
      const BoolCtrl& ctrlsToChange = {},
      const BoolCtrl& autoCtrls = {},
      const BoolCtrl& autoCtrlsValues = {});

  struct ListComponentsRequest {
    DeviceSpec deviceSpec{};
    uint16_t maxNames{};
  };

  struct ListComponentsReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    uint16_t extra{};
    std::vector<Listing> keymaps{};
    std::vector<Listing> keycodes{};
    std::vector<Listing> types{};
    std::vector<Listing> compatMaps{};
    std::vector<Listing> symbols{};
    std::vector<Listing> geometries{};
  };

  using ListComponentsResponse = Response<ListComponentsReply>;

  Future<ListComponentsReply> ListComponents(
      const ListComponentsRequest& request);

  Future<ListComponentsReply> ListComponents(const DeviceSpec& deviceSpec = {},
                                             const uint16_t& maxNames = {});

  struct GetKbdByNameRequest {
    DeviceSpec deviceSpec{};
    GBNDetail need{};
    GBNDetail want{};
    uint8_t load{};
  };

  struct GetKbdByNameReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    KeyCode minKeyCode{};
    KeyCode maxKeyCode{};
    uint8_t loaded{};
    uint8_t newKeyboard{};
    GBNDetail found{};
    struct Types {
      uint8_t getmap_type{};
      uint8_t typeDeviceID{};
      uint16_t getmap_sequence{};
      uint32_t getmap_length{};
      KeyCode typeMinKeyCode{};
      KeyCode typeMaxKeyCode{};
      uint8_t firstType{};
      uint8_t nTypes{};
      uint8_t totalTypes{};
      KeyCode firstKeySym{};
      uint16_t totalSyms{};
      uint8_t nKeySyms{};
      KeyCode firstKeyAction{};
      uint16_t totalActions{};
      uint8_t nKeyActions{};
      KeyCode firstKeyBehavior{};
      uint8_t nKeyBehaviors{};
      uint8_t totalKeyBehaviors{};
      KeyCode firstKeyExplicit{};
      uint8_t nKeyExplicit{};
      uint8_t totalKeyExplicit{};
      KeyCode firstModMapKey{};
      uint8_t nModMapKeys{};
      uint8_t totalModMapKeys{};
      KeyCode firstVModMapKey{};
      uint8_t nVModMapKeys{};
      uint8_t totalVModMapKeys{};
      VMod virtualMods{};
      base::Optional<std::vector<KeyType>> types_rtrn{};
      base::Optional<std::vector<KeySymMap>> syms_rtrn{};
      base::Optional<std::vector<uint8_t>> acts_rtrn_count{};
      base::Optional<std::vector<Action>> acts_rtrn_acts{};
      base::Optional<std::vector<SetBehavior>> behaviors_rtrn{};
      base::Optional<std::vector<ModMask>> vmods_rtrn{};
      base::Optional<std::vector<SetExplicit>> explicit_rtrn{};
      base::Optional<std::vector<KeyModMap>> modmap_rtrn{};
      base::Optional<std::vector<KeyVModMap>> vmodmap_rtrn{};
    };
    struct CompatMap {
      uint8_t compatmap_type{};
      uint8_t compatDeviceID{};
      uint16_t compatmap_sequence{};
      uint32_t compatmap_length{};
      SetOfGroup groupsRtrn{};
      uint16_t firstSIRtrn{};
      uint16_t nTotalSI{};
      std::vector<SymInterpret> si_rtrn{};
      std::vector<ModDef> group_rtrn{};
    };
    struct IndicatorMaps {
      uint8_t indicatormap_type{};
      uint8_t indicatorDeviceID{};
      uint16_t indicatormap_sequence{};
      uint32_t indicatormap_length{};
      uint32_t which{};
      uint32_t realIndicators{};
      std::vector<IndicatorMap> maps{};
    };
    struct KeyNames {
      uint8_t keyname_type{};
      uint8_t keyDeviceID{};
      uint16_t keyname_sequence{};
      uint32_t keyname_length{};
      KeyCode keyMinKeyCode{};
      KeyCode keyMaxKeyCode{};
      uint8_t nTypes{};
      SetOfGroup groupNames{};
      VMod virtualMods{};
      KeyCode firstKey{};
      uint8_t nKeys{};
      uint32_t indicators{};
      uint8_t nRadioGroups{};
      uint8_t nKeyAliases{};
      uint16_t nKTLevels{};
      base::Optional<Atom> keycodesName{};
      base::Optional<Atom> geometryName{};
      base::Optional<Atom> symbolsName{};
      base::Optional<Atom> physSymbolsName{};
      base::Optional<Atom> typesName{};
      base::Optional<Atom> compatName{};
      base::Optional<std::vector<Atom>> typeNames{};
      base::Optional<std::vector<uint8_t>> nLevelsPerType{};
      base::Optional<std::vector<Atom>> ktLevelNames{};
      base::Optional<std::vector<Atom>> indicatorNames{};
      base::Optional<std::vector<Atom>> virtualModNames{};
      base::Optional<std::vector<Atom>> groups{};
      base::Optional<std::vector<KeyName>> keyNames{};
      base::Optional<std::vector<KeyAlias>> keyAliases{};
      base::Optional<std::vector<Atom>> radioGroupNames{};
    };
    struct Geometry {
      uint8_t geometry_type{};
      uint8_t geometryDeviceID{};
      uint16_t geometry_sequence{};
      uint32_t geometry_length{};
      Atom name{};
      uint8_t geometryFound{};
      uint16_t widthMM{};
      uint16_t heightMM{};
      uint16_t nProperties{};
      uint16_t nColors{};
      uint16_t nShapes{};
      uint16_t nSections{};
      uint16_t nDoodads{};
      uint16_t nKeyAliases{};
      uint8_t baseColorNdx{};
      uint8_t labelColorNdx{};
      CountedString16 labelFont{};
    };
    base::Optional<Types> types{};
    base::Optional<CompatMap> compat_map{};
    base::Optional<IndicatorMaps> indicator_maps{};
    base::Optional<KeyNames> key_names{};
    base::Optional<Geometry> geometry{};
  };

  using GetKbdByNameResponse = Response<GetKbdByNameReply>;

  Future<GetKbdByNameReply> GetKbdByName(const GetKbdByNameRequest& request);

  Future<GetKbdByNameReply> GetKbdByName(const DeviceSpec& deviceSpec = {},
                                         const GBNDetail& need = {},
                                         const GBNDetail& want = {},
                                         const uint8_t& load = {});

  struct GetDeviceInfoRequest {
    DeviceSpec deviceSpec{};
    XIFeature wanted{};
    uint8_t allButtons{};
    uint8_t firstButton{};
    uint8_t nButtons{};
    LedClass ledClass{};
    IDSpec ledID{};
  };

  struct GetDeviceInfoReply {
    uint8_t deviceID{};
    uint16_t sequence{};
    XIFeature present{};
    XIFeature supported{};
    XIFeature unsupported{};
    uint8_t firstBtnWanted{};
    uint8_t nBtnsWanted{};
    uint8_t firstBtnRtrn{};
    uint8_t totalBtns{};
    uint8_t hasOwnState{};
    uint16_t dfltKbdFB{};
    uint16_t dfltLedFB{};
    Atom devType{};
    std::vector<String8> name{};
    std::vector<Action> btnActions{};
    std::vector<DeviceLedInfo> leds{};
  };

  using GetDeviceInfoResponse = Response<GetDeviceInfoReply>;

  Future<GetDeviceInfoReply> GetDeviceInfo(const GetDeviceInfoRequest& request);

  Future<GetDeviceInfoReply> GetDeviceInfo(const DeviceSpec& deviceSpec = {},
                                           const XIFeature& wanted = {},
                                           const uint8_t& allButtons = {},
                                           const uint8_t& firstButton = {},
                                           const uint8_t& nButtons = {},
                                           const LedClass& ledClass = {},
                                           const IDSpec& ledID = {});

  struct SetDeviceInfoRequest {
    DeviceSpec deviceSpec{};
    uint8_t firstBtn{};
    XIFeature change{};
    std::vector<Action> btnActions{};
    std::vector<DeviceLedInfo> leds{};
  };

  using SetDeviceInfoResponse = Response<void>;

  Future<void> SetDeviceInfo(const SetDeviceInfoRequest& request);

  Future<void> SetDeviceInfo(const DeviceSpec& deviceSpec = {},
                             const uint8_t& firstBtn = {},
                             const XIFeature& change = {},
                             const std::vector<Action>& btnActions = {},
                             const std::vector<DeviceLedInfo>& leds = {});

  struct SetDebuggingFlagsRequest {
    uint32_t affectFlags{};
    uint32_t flags{};
    uint32_t affectCtrls{};
    uint32_t ctrls{};
    std::vector<String8> message{};
  };

  struct SetDebuggingFlagsReply {
    uint16_t sequence{};
    uint32_t currentFlags{};
    uint32_t currentCtrls{};
    uint32_t supportedFlags{};
    uint32_t supportedCtrls{};
  };

  using SetDebuggingFlagsResponse = Response<SetDebuggingFlagsReply>;

  Future<SetDebuggingFlagsReply> SetDebuggingFlags(
      const SetDebuggingFlagsRequest& request);

  Future<SetDebuggingFlagsReply> SetDebuggingFlags(
      const uint32_t& affectFlags = {},
      const uint32_t& flags = {},
      const uint32_t& affectCtrls = {},
      const uint32_t& ctrls = {},
      const std::vector<String8>& message = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Xkb::Const operator|(x11::Xkb::Const l,
                                           x11::Xkb::Const r) {
  using T = std::underlying_type_t<x11::Xkb::Const>;
  return static_cast<x11::Xkb::Const>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Const operator&(x11::Xkb::Const l,
                                           x11::Xkb::Const r) {
  using T = std::underlying_type_t<x11::Xkb::Const>;
  return static_cast<x11::Xkb::Const>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::EventType operator|(x11::Xkb::EventType l,
                                               x11::Xkb::EventType r) {
  using T = std::underlying_type_t<x11::Xkb::EventType>;
  return static_cast<x11::Xkb::EventType>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::EventType operator&(x11::Xkb::EventType l,
                                               x11::Xkb::EventType r) {
  using T = std::underlying_type_t<x11::Xkb::EventType>;
  return static_cast<x11::Xkb::EventType>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::NKNDetail operator|(x11::Xkb::NKNDetail l,
                                               x11::Xkb::NKNDetail r) {
  using T = std::underlying_type_t<x11::Xkb::NKNDetail>;
  return static_cast<x11::Xkb::NKNDetail>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::NKNDetail operator&(x11::Xkb::NKNDetail l,
                                               x11::Xkb::NKNDetail r) {
  using T = std::underlying_type_t<x11::Xkb::NKNDetail>;
  return static_cast<x11::Xkb::NKNDetail>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::AXNDetail operator|(x11::Xkb::AXNDetail l,
                                               x11::Xkb::AXNDetail r) {
  using T = std::underlying_type_t<x11::Xkb::AXNDetail>;
  return static_cast<x11::Xkb::AXNDetail>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::AXNDetail operator&(x11::Xkb::AXNDetail l,
                                               x11::Xkb::AXNDetail r) {
  using T = std::underlying_type_t<x11::Xkb::AXNDetail>;
  return static_cast<x11::Xkb::AXNDetail>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::MapPart operator|(x11::Xkb::MapPart l,
                                             x11::Xkb::MapPart r) {
  using T = std::underlying_type_t<x11::Xkb::MapPart>;
  return static_cast<x11::Xkb::MapPart>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::MapPart operator&(x11::Xkb::MapPart l,
                                             x11::Xkb::MapPart r) {
  using T = std::underlying_type_t<x11::Xkb::MapPart>;
  return static_cast<x11::Xkb::MapPart>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::SetMapFlags operator|(x11::Xkb::SetMapFlags l,
                                                 x11::Xkb::SetMapFlags r) {
  using T = std::underlying_type_t<x11::Xkb::SetMapFlags>;
  return static_cast<x11::Xkb::SetMapFlags>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Xkb::SetMapFlags operator&(x11::Xkb::SetMapFlags l,
                                                 x11::Xkb::SetMapFlags r) {
  using T = std::underlying_type_t<x11::Xkb::SetMapFlags>;
  return static_cast<x11::Xkb::SetMapFlags>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Xkb::StatePart operator|(x11::Xkb::StatePart l,
                                               x11::Xkb::StatePart r) {
  using T = std::underlying_type_t<x11::Xkb::StatePart>;
  return static_cast<x11::Xkb::StatePart>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::StatePart operator&(x11::Xkb::StatePart l,
                                               x11::Xkb::StatePart r) {
  using T = std::underlying_type_t<x11::Xkb::StatePart>;
  return static_cast<x11::Xkb::StatePart>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::BoolCtrl operator|(x11::Xkb::BoolCtrl l,
                                              x11::Xkb::BoolCtrl r) {
  using T = std::underlying_type_t<x11::Xkb::BoolCtrl>;
  return static_cast<x11::Xkb::BoolCtrl>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::BoolCtrl operator&(x11::Xkb::BoolCtrl l,
                                              x11::Xkb::BoolCtrl r) {
  using T = std::underlying_type_t<x11::Xkb::BoolCtrl>;
  return static_cast<x11::Xkb::BoolCtrl>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::Control operator|(x11::Xkb::Control l,
                                             x11::Xkb::Control r) {
  using T = std::underlying_type_t<x11::Xkb::Control>;
  return static_cast<x11::Xkb::Control>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Control operator&(x11::Xkb::Control l,
                                             x11::Xkb::Control r) {
  using T = std::underlying_type_t<x11::Xkb::Control>;
  return static_cast<x11::Xkb::Control>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::AXOption operator|(x11::Xkb::AXOption l,
                                              x11::Xkb::AXOption r) {
  using T = std::underlying_type_t<x11::Xkb::AXOption>;
  return static_cast<x11::Xkb::AXOption>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::AXOption operator&(x11::Xkb::AXOption l,
                                              x11::Xkb::AXOption r) {
  using T = std::underlying_type_t<x11::Xkb::AXOption>;
  return static_cast<x11::Xkb::AXOption>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::LedClassResult operator|(
    x11::Xkb::LedClassResult l,
    x11::Xkb::LedClassResult r) {
  using T = std::underlying_type_t<x11::Xkb::LedClassResult>;
  return static_cast<x11::Xkb::LedClassResult>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Xkb::LedClassResult operator&(
    x11::Xkb::LedClassResult l,
    x11::Xkb::LedClassResult r) {
  using T = std::underlying_type_t<x11::Xkb::LedClassResult>;
  return static_cast<x11::Xkb::LedClassResult>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Xkb::LedClass operator|(x11::Xkb::LedClass l,
                                              x11::Xkb::LedClass r) {
  using T = std::underlying_type_t<x11::Xkb::LedClass>;
  return static_cast<x11::Xkb::LedClass>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::LedClass operator&(x11::Xkb::LedClass l,
                                              x11::Xkb::LedClass r) {
  using T = std::underlying_type_t<x11::Xkb::LedClass>;
  return static_cast<x11::Xkb::LedClass>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::BellClassResult operator|(
    x11::Xkb::BellClassResult l,
    x11::Xkb::BellClassResult r) {
  using T = std::underlying_type_t<x11::Xkb::BellClassResult>;
  return static_cast<x11::Xkb::BellClassResult>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Xkb::BellClassResult operator&(
    x11::Xkb::BellClassResult l,
    x11::Xkb::BellClassResult r) {
  using T = std::underlying_type_t<x11::Xkb::BellClassResult>;
  return static_cast<x11::Xkb::BellClassResult>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Xkb::BellClass operator|(x11::Xkb::BellClass l,
                                               x11::Xkb::BellClass r) {
  using T = std::underlying_type_t<x11::Xkb::BellClass>;
  return static_cast<x11::Xkb::BellClass>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::BellClass operator&(x11::Xkb::BellClass l,
                                               x11::Xkb::BellClass r) {
  using T = std::underlying_type_t<x11::Xkb::BellClass>;
  return static_cast<x11::Xkb::BellClass>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::Id operator|(x11::Xkb::Id l, x11::Xkb::Id r) {
  using T = std::underlying_type_t<x11::Xkb::Id>;
  return static_cast<x11::Xkb::Id>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Id operator&(x11::Xkb::Id l, x11::Xkb::Id r) {
  using T = std::underlying_type_t<x11::Xkb::Id>;
  return static_cast<x11::Xkb::Id>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::Group operator|(x11::Xkb::Group l,
                                           x11::Xkb::Group r) {
  using T = std::underlying_type_t<x11::Xkb::Group>;
  return static_cast<x11::Xkb::Group>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Group operator&(x11::Xkb::Group l,
                                           x11::Xkb::Group r) {
  using T = std::underlying_type_t<x11::Xkb::Group>;
  return static_cast<x11::Xkb::Group>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::Groups operator|(x11::Xkb::Groups l,
                                            x11::Xkb::Groups r) {
  using T = std::underlying_type_t<x11::Xkb::Groups>;
  return static_cast<x11::Xkb::Groups>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Groups operator&(x11::Xkb::Groups l,
                                            x11::Xkb::Groups r) {
  using T = std::underlying_type_t<x11::Xkb::Groups>;
  return static_cast<x11::Xkb::Groups>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::SetOfGroup operator|(x11::Xkb::SetOfGroup l,
                                                x11::Xkb::SetOfGroup r) {
  using T = std::underlying_type_t<x11::Xkb::SetOfGroup>;
  return static_cast<x11::Xkb::SetOfGroup>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::SetOfGroup operator&(x11::Xkb::SetOfGroup l,
                                                x11::Xkb::SetOfGroup r) {
  using T = std::underlying_type_t<x11::Xkb::SetOfGroup>;
  return static_cast<x11::Xkb::SetOfGroup>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::SetOfGroups operator|(x11::Xkb::SetOfGroups l,
                                                 x11::Xkb::SetOfGroups r) {
  using T = std::underlying_type_t<x11::Xkb::SetOfGroups>;
  return static_cast<x11::Xkb::SetOfGroups>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Xkb::SetOfGroups operator&(x11::Xkb::SetOfGroups l,
                                                 x11::Xkb::SetOfGroups r) {
  using T = std::underlying_type_t<x11::Xkb::SetOfGroups>;
  return static_cast<x11::Xkb::SetOfGroups>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Xkb::GroupsWrap operator|(x11::Xkb::GroupsWrap l,
                                                x11::Xkb::GroupsWrap r) {
  using T = std::underlying_type_t<x11::Xkb::GroupsWrap>;
  return static_cast<x11::Xkb::GroupsWrap>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::GroupsWrap operator&(x11::Xkb::GroupsWrap l,
                                                x11::Xkb::GroupsWrap r) {
  using T = std::underlying_type_t<x11::Xkb::GroupsWrap>;
  return static_cast<x11::Xkb::GroupsWrap>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::VModsHigh operator|(x11::Xkb::VModsHigh l,
                                               x11::Xkb::VModsHigh r) {
  using T = std::underlying_type_t<x11::Xkb::VModsHigh>;
  return static_cast<x11::Xkb::VModsHigh>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::VModsHigh operator&(x11::Xkb::VModsHigh l,
                                               x11::Xkb::VModsHigh r) {
  using T = std::underlying_type_t<x11::Xkb::VModsHigh>;
  return static_cast<x11::Xkb::VModsHigh>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::VModsLow operator|(x11::Xkb::VModsLow l,
                                              x11::Xkb::VModsLow r) {
  using T = std::underlying_type_t<x11::Xkb::VModsLow>;
  return static_cast<x11::Xkb::VModsLow>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::VModsLow operator&(x11::Xkb::VModsLow l,
                                              x11::Xkb::VModsLow r) {
  using T = std::underlying_type_t<x11::Xkb::VModsLow>;
  return static_cast<x11::Xkb::VModsLow>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::VMod operator|(x11::Xkb::VMod l, x11::Xkb::VMod r) {
  using T = std::underlying_type_t<x11::Xkb::VMod>;
  return static_cast<x11::Xkb::VMod>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::VMod operator&(x11::Xkb::VMod l, x11::Xkb::VMod r) {
  using T = std::underlying_type_t<x11::Xkb::VMod>;
  return static_cast<x11::Xkb::VMod>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::Explicit operator|(x11::Xkb::Explicit l,
                                              x11::Xkb::Explicit r) {
  using T = std::underlying_type_t<x11::Xkb::Explicit>;
  return static_cast<x11::Xkb::Explicit>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Explicit operator&(x11::Xkb::Explicit l,
                                              x11::Xkb::Explicit r) {
  using T = std::underlying_type_t<x11::Xkb::Explicit>;
  return static_cast<x11::Xkb::Explicit>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::SymInterpretMatch operator|(
    x11::Xkb::SymInterpretMatch l,
    x11::Xkb::SymInterpretMatch r) {
  using T = std::underlying_type_t<x11::Xkb::SymInterpretMatch>;
  return static_cast<x11::Xkb::SymInterpretMatch>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::Xkb::SymInterpretMatch operator&(
    x11::Xkb::SymInterpretMatch l,
    x11::Xkb::SymInterpretMatch r) {
  using T = std::underlying_type_t<x11::Xkb::SymInterpretMatch>;
  return static_cast<x11::Xkb::SymInterpretMatch>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::Xkb::SymInterpMatch operator|(
    x11::Xkb::SymInterpMatch l,
    x11::Xkb::SymInterpMatch r) {
  using T = std::underlying_type_t<x11::Xkb::SymInterpMatch>;
  return static_cast<x11::Xkb::SymInterpMatch>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Xkb::SymInterpMatch operator&(
    x11::Xkb::SymInterpMatch l,
    x11::Xkb::SymInterpMatch r) {
  using T = std::underlying_type_t<x11::Xkb::SymInterpMatch>;
  return static_cast<x11::Xkb::SymInterpMatch>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

inline constexpr x11::Xkb::IMFlag operator|(x11::Xkb::IMFlag l,
                                            x11::Xkb::IMFlag r) {
  using T = std::underlying_type_t<x11::Xkb::IMFlag>;
  return static_cast<x11::Xkb::IMFlag>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::IMFlag operator&(x11::Xkb::IMFlag l,
                                            x11::Xkb::IMFlag r) {
  using T = std::underlying_type_t<x11::Xkb::IMFlag>;
  return static_cast<x11::Xkb::IMFlag>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::IMModsWhich operator|(x11::Xkb::IMModsWhich l,
                                                 x11::Xkb::IMModsWhich r) {
  using T = std::underlying_type_t<x11::Xkb::IMModsWhich>;
  return static_cast<x11::Xkb::IMModsWhich>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Xkb::IMModsWhich operator&(x11::Xkb::IMModsWhich l,
                                                 x11::Xkb::IMModsWhich r) {
  using T = std::underlying_type_t<x11::Xkb::IMModsWhich>;
  return static_cast<x11::Xkb::IMModsWhich>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Xkb::IMGroupsWhich operator|(x11::Xkb::IMGroupsWhich l,
                                                   x11::Xkb::IMGroupsWhich r) {
  using T = std::underlying_type_t<x11::Xkb::IMGroupsWhich>;
  return static_cast<x11::Xkb::IMGroupsWhich>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::IMGroupsWhich operator&(x11::Xkb::IMGroupsWhich l,
                                                   x11::Xkb::IMGroupsWhich r) {
  using T = std::underlying_type_t<x11::Xkb::IMGroupsWhich>;
  return static_cast<x11::Xkb::IMGroupsWhich>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::CMDetail operator|(x11::Xkb::CMDetail l,
                                              x11::Xkb::CMDetail r) {
  using T = std::underlying_type_t<x11::Xkb::CMDetail>;
  return static_cast<x11::Xkb::CMDetail>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::CMDetail operator&(x11::Xkb::CMDetail l,
                                              x11::Xkb::CMDetail r) {
  using T = std::underlying_type_t<x11::Xkb::CMDetail>;
  return static_cast<x11::Xkb::CMDetail>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::NameDetail operator|(x11::Xkb::NameDetail l,
                                                x11::Xkb::NameDetail r) {
  using T = std::underlying_type_t<x11::Xkb::NameDetail>;
  return static_cast<x11::Xkb::NameDetail>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::NameDetail operator&(x11::Xkb::NameDetail l,
                                                x11::Xkb::NameDetail r) {
  using T = std::underlying_type_t<x11::Xkb::NameDetail>;
  return static_cast<x11::Xkb::NameDetail>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::GBNDetail operator|(x11::Xkb::GBNDetail l,
                                               x11::Xkb::GBNDetail r) {
  using T = std::underlying_type_t<x11::Xkb::GBNDetail>;
  return static_cast<x11::Xkb::GBNDetail>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::GBNDetail operator&(x11::Xkb::GBNDetail l,
                                               x11::Xkb::GBNDetail r) {
  using T = std::underlying_type_t<x11::Xkb::GBNDetail>;
  return static_cast<x11::Xkb::GBNDetail>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::XIFeature operator|(x11::Xkb::XIFeature l,
                                               x11::Xkb::XIFeature r) {
  using T = std::underlying_type_t<x11::Xkb::XIFeature>;
  return static_cast<x11::Xkb::XIFeature>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::XIFeature operator&(x11::Xkb::XIFeature l,
                                               x11::Xkb::XIFeature r) {
  using T = std::underlying_type_t<x11::Xkb::XIFeature>;
  return static_cast<x11::Xkb::XIFeature>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::PerClientFlag operator|(x11::Xkb::PerClientFlag l,
                                                   x11::Xkb::PerClientFlag r) {
  using T = std::underlying_type_t<x11::Xkb::PerClientFlag>;
  return static_cast<x11::Xkb::PerClientFlag>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::PerClientFlag operator&(x11::Xkb::PerClientFlag l,
                                                   x11::Xkb::PerClientFlag r) {
  using T = std::underlying_type_t<x11::Xkb::PerClientFlag>;
  return static_cast<x11::Xkb::PerClientFlag>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::BehaviorType operator|(x11::Xkb::BehaviorType l,
                                                  x11::Xkb::BehaviorType r) {
  using T = std::underlying_type_t<x11::Xkb::BehaviorType>;
  return static_cast<x11::Xkb::BehaviorType>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Xkb::BehaviorType operator&(x11::Xkb::BehaviorType l,
                                                  x11::Xkb::BehaviorType r) {
  using T = std::underlying_type_t<x11::Xkb::BehaviorType>;
  return static_cast<x11::Xkb::BehaviorType>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Xkb::DoodadType operator|(x11::Xkb::DoodadType l,
                                                x11::Xkb::DoodadType r) {
  using T = std::underlying_type_t<x11::Xkb::DoodadType>;
  return static_cast<x11::Xkb::DoodadType>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::DoodadType operator&(x11::Xkb::DoodadType l,
                                                x11::Xkb::DoodadType r) {
  using T = std::underlying_type_t<x11::Xkb::DoodadType>;
  return static_cast<x11::Xkb::DoodadType>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Xkb::Error operator|(x11::Xkb::Error l,
                                           x11::Xkb::Error r) {
  using T = std::underlying_type_t<x11::Xkb::Error>;
  return static_cast<x11::Xkb::Error>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Error operator&(x11::Xkb::Error l,
                                           x11::Xkb::Error r) {
  using T = std::underlying_type_t<x11::Xkb::Error>;
  return static_cast<x11::Xkb::Error>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::Sa operator|(x11::Xkb::Sa l, x11::Xkb::Sa r) {
  using T = std::underlying_type_t<x11::Xkb::Sa>;
  return static_cast<x11::Xkb::Sa>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::Sa operator&(x11::Xkb::Sa l, x11::Xkb::Sa r) {
  using T = std::underlying_type_t<x11::Xkb::Sa>;
  return static_cast<x11::Xkb::Sa>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::SAType operator|(x11::Xkb::SAType l,
                                            x11::Xkb::SAType r) {
  using T = std::underlying_type_t<x11::Xkb::SAType>;
  return static_cast<x11::Xkb::SAType>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Xkb::SAType operator&(x11::Xkb::SAType l,
                                            x11::Xkb::SAType r) {
  using T = std::underlying_type_t<x11::Xkb::SAType>;
  return static_cast<x11::Xkb::SAType>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Xkb::SAMovePtrFlag operator|(x11::Xkb::SAMovePtrFlag l,
                                                   x11::Xkb::SAMovePtrFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SAMovePtrFlag>;
  return static_cast<x11::Xkb::SAMovePtrFlag>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::SAMovePtrFlag operator&(x11::Xkb::SAMovePtrFlag l,
                                                   x11::Xkb::SAMovePtrFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SAMovePtrFlag>;
  return static_cast<x11::Xkb::SAMovePtrFlag>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::SASetPtrDfltFlag operator|(
    x11::Xkb::SASetPtrDfltFlag l,
    x11::Xkb::SASetPtrDfltFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SASetPtrDfltFlag>;
  return static_cast<x11::Xkb::SASetPtrDfltFlag>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Xkb::SASetPtrDfltFlag operator&(
    x11::Xkb::SASetPtrDfltFlag l,
    x11::Xkb::SASetPtrDfltFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SASetPtrDfltFlag>;
  return static_cast<x11::Xkb::SASetPtrDfltFlag>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Xkb::SAIsoLockFlag operator|(x11::Xkb::SAIsoLockFlag l,
                                                   x11::Xkb::SAIsoLockFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SAIsoLockFlag>;
  return static_cast<x11::Xkb::SAIsoLockFlag>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::SAIsoLockFlag operator&(x11::Xkb::SAIsoLockFlag l,
                                                   x11::Xkb::SAIsoLockFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SAIsoLockFlag>;
  return static_cast<x11::Xkb::SAIsoLockFlag>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::SAIsoLockNoAffect operator|(
    x11::Xkb::SAIsoLockNoAffect l,
    x11::Xkb::SAIsoLockNoAffect r) {
  using T = std::underlying_type_t<x11::Xkb::SAIsoLockNoAffect>;
  return static_cast<x11::Xkb::SAIsoLockNoAffect>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::Xkb::SAIsoLockNoAffect operator&(
    x11::Xkb::SAIsoLockNoAffect l,
    x11::Xkb::SAIsoLockNoAffect r) {
  using T = std::underlying_type_t<x11::Xkb::SAIsoLockNoAffect>;
  return static_cast<x11::Xkb::SAIsoLockNoAffect>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::Xkb::SwitchScreenFlag operator|(
    x11::Xkb::SwitchScreenFlag l,
    x11::Xkb::SwitchScreenFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SwitchScreenFlag>;
  return static_cast<x11::Xkb::SwitchScreenFlag>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::Xkb::SwitchScreenFlag operator&(
    x11::Xkb::SwitchScreenFlag l,
    x11::Xkb::SwitchScreenFlag r) {
  using T = std::underlying_type_t<x11::Xkb::SwitchScreenFlag>;
  return static_cast<x11::Xkb::SwitchScreenFlag>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::Xkb::BoolCtrlsHigh operator|(x11::Xkb::BoolCtrlsHigh l,
                                                   x11::Xkb::BoolCtrlsHigh r) {
  using T = std::underlying_type_t<x11::Xkb::BoolCtrlsHigh>;
  return static_cast<x11::Xkb::BoolCtrlsHigh>(static_cast<T>(l) |
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::BoolCtrlsHigh operator&(x11::Xkb::BoolCtrlsHigh l,
                                                   x11::Xkb::BoolCtrlsHigh r) {
  using T = std::underlying_type_t<x11::Xkb::BoolCtrlsHigh>;
  return static_cast<x11::Xkb::BoolCtrlsHigh>(static_cast<T>(l) &
                                              static_cast<T>(r));
}

inline constexpr x11::Xkb::BoolCtrlsLow operator|(x11::Xkb::BoolCtrlsLow l,
                                                  x11::Xkb::BoolCtrlsLow r) {
  using T = std::underlying_type_t<x11::Xkb::BoolCtrlsLow>;
  return static_cast<x11::Xkb::BoolCtrlsLow>(static_cast<T>(l) |
                                             static_cast<T>(r));
}

inline constexpr x11::Xkb::BoolCtrlsLow operator&(x11::Xkb::BoolCtrlsLow l,
                                                  x11::Xkb::BoolCtrlsLow r) {
  using T = std::underlying_type_t<x11::Xkb::BoolCtrlsLow>;
  return static_cast<x11::Xkb::BoolCtrlsLow>(static_cast<T>(l) &
                                             static_cast<T>(r));
}

inline constexpr x11::Xkb::ActionMessageFlag operator|(
    x11::Xkb::ActionMessageFlag l,
    x11::Xkb::ActionMessageFlag r) {
  using T = std::underlying_type_t<x11::Xkb::ActionMessageFlag>;
  return static_cast<x11::Xkb::ActionMessageFlag>(static_cast<T>(l) |
                                                  static_cast<T>(r));
}

inline constexpr x11::Xkb::ActionMessageFlag operator&(
    x11::Xkb::ActionMessageFlag l,
    x11::Xkb::ActionMessageFlag r) {
  using T = std::underlying_type_t<x11::Xkb::ActionMessageFlag>;
  return static_cast<x11::Xkb::ActionMessageFlag>(static_cast<T>(l) &
                                                  static_cast<T>(r));
}

inline constexpr x11::Xkb::LockDeviceFlags operator|(
    x11::Xkb::LockDeviceFlags l,
    x11::Xkb::LockDeviceFlags r) {
  using T = std::underlying_type_t<x11::Xkb::LockDeviceFlags>;
  return static_cast<x11::Xkb::LockDeviceFlags>(static_cast<T>(l) |
                                                static_cast<T>(r));
}

inline constexpr x11::Xkb::LockDeviceFlags operator&(
    x11::Xkb::LockDeviceFlags l,
    x11::Xkb::LockDeviceFlags r) {
  using T = std::underlying_type_t<x11::Xkb::LockDeviceFlags>;
  return static_cast<x11::Xkb::LockDeviceFlags>(static_cast<T>(l) &
                                                static_cast<T>(r));
}

inline constexpr x11::Xkb::SAValWhat operator|(x11::Xkb::SAValWhat l,
                                               x11::Xkb::SAValWhat r) {
  using T = std::underlying_type_t<x11::Xkb::SAValWhat>;
  return static_cast<x11::Xkb::SAValWhat>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Xkb::SAValWhat operator&(x11::Xkb::SAValWhat l,
                                               x11::Xkb::SAValWhat r) {
  using T = std::underlying_type_t<x11::Xkb::SAValWhat>;
  return static_cast<x11::Xkb::SAValWhat>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XKB_H_
