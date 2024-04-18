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

#ifndef UI_GFX_X_GENERATED_PROTOS_XPROTO_H_
#define UI_GFX_X_GENERATED_PROTOS_XPROTO_H_

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

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

enum class GraphicsContext : uint32_t {};

enum class ColorMap : uint32_t {};

enum class Bool32 : uint32_t {};

enum class VisualId : uint32_t {};

enum class KeySym : uint32_t {};

enum class KeyCode : uint8_t {};

enum class KeyCode32 : uint32_t {};

enum class Button : uint8_t {};

enum class VisualClass : int {
  StaticGray = 0,
  GrayScale = 1,
  StaticColor = 2,
  PseudoColor = 3,
  TrueColor = 4,
  DirectColor = 5,
};

enum class EventMask : int {
  NoEvent = 0,
  KeyPress = 1 << 0,
  KeyRelease = 1 << 1,
  ButtonPress = 1 << 2,
  ButtonRelease = 1 << 3,
  EnterWindow = 1 << 4,
  LeaveWindow = 1 << 5,
  PointerMotion = 1 << 6,
  PointerMotionHint = 1 << 7,
  Button1Motion = 1 << 8,
  Button2Motion = 1 << 9,
  Button3Motion = 1 << 10,
  Button4Motion = 1 << 11,
  Button5Motion = 1 << 12,
  ButtonMotion = 1 << 13,
  KeymapState = 1 << 14,
  Exposure = 1 << 15,
  VisibilityChange = 1 << 16,
  StructureNotify = 1 << 17,
  ResizeRedirect = 1 << 18,
  SubstructureNotify = 1 << 19,
  SubstructureRedirect = 1 << 20,
  FocusChange = 1 << 21,
  PropertyChange = 1 << 22,
  ColorMapChange = 1 << 23,
  OwnerGrabButton = 1 << 24,
};

enum class BackingStore : int {
  NotUseful = 0,
  WhenMapped = 1,
  Always = 2,
};

enum class ImageOrder : int {
  LSBFirst = 0,
  MSBFirst = 1,
};

enum class ModMask : int {
  Shift = 1 << 0,
  Lock = 1 << 1,
  Control = 1 << 2,
  c_1 = 1 << 3,
  c_2 = 1 << 4,
  c_3 = 1 << 5,
  c_4 = 1 << 6,
  c_5 = 1 << 7,
  Any = 1 << 15,
};

enum class KeyButMask : int {
  Shift = 1 << 0,
  Lock = 1 << 1,
  Control = 1 << 2,
  Mod1 = 1 << 3,
  Mod2 = 1 << 4,
  Mod3 = 1 << 5,
  Mod4 = 1 << 6,
  Mod5 = 1 << 7,
  Button1 = 1 << 8,
  Button2 = 1 << 9,
  Button3 = 1 << 10,
  Button4 = 1 << 11,
  Button5 = 1 << 12,
};

enum class Window : uint32_t {
  None = 0,
};

enum class ButtonMask : int {
  c_1 = 1 << 8,
  c_2 = 1 << 9,
  c_3 = 1 << 10,
  c_4 = 1 << 11,
  c_5 = 1 << 12,
  Any = 1 << 15,
};

enum class Motion : int {
  Normal = 0,
  Hint = 1,
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

enum class NotifyMode : int {
  Normal = 0,
  Grab = 1,
  Ungrab = 2,
  WhileGrabbed = 3,
};

enum class Visibility : int {
  Unobscured = 0,
  PartiallyObscured = 1,
  FullyObscured = 2,
};

enum class Place : int {
  OnTop = 0,
  OnBottom = 1,
};

enum class Property : int {
  NewValue = 0,
  Delete = 1,
};

enum class Time : uint32_t {
  CurrentTime = 0,
};

enum class Atom : uint32_t {
  None = 0,
  Any = 0,
  PRIMARY = 1,
  SECONDARY = 2,
  ARC = 3,
  ATOM = 4,
  BITMAP = 5,
  CARDINAL = 6,
  COLORMAP = 7,
  CURSOR = 8,
  CUT_BUFFER0 = 9,
  CUT_BUFFER1 = 10,
  CUT_BUFFER2 = 11,
  CUT_BUFFER3 = 12,
  CUT_BUFFER4 = 13,
  CUT_BUFFER5 = 14,
  CUT_BUFFER6 = 15,
  CUT_BUFFER7 = 16,
  DRAWABLE = 17,
  FONT = 18,
  INTEGER = 19,
  PIXMAP = 20,
  POINT = 21,
  RECTANGLE = 22,
  RESOURCE_MANAGER = 23,
  RGB_COLOR_MAP = 24,
  RGB_BEST_MAP = 25,
  RGB_BLUE_MAP = 26,
  RGB_DEFAULT_MAP = 27,
  RGB_GRAY_MAP = 28,
  RGB_GREEN_MAP = 29,
  RGB_RED_MAP = 30,
  STRING = 31,
  VISUALID = 32,
  WINDOW = 33,
  WM_COMMAND = 34,
  WM_HINTS = 35,
  WM_CLIENT_MACHINE = 36,
  WM_ICON_NAME = 37,
  WM_ICON_SIZE = 38,
  WM_NAME = 39,
  WM_NORMAL_HINTS = 40,
  WM_SIZE_HINTS = 41,
  WM_ZOOM_HINTS = 42,
  MIN_SPACE = 43,
  NORM_SPACE = 44,
  MAX_SPACE = 45,
  END_SPACE = 46,
  SUPERSCRIPT_X = 47,
  SUPERSCRIPT_Y = 48,
  SUBSCRIPT_X = 49,
  SUBSCRIPT_Y = 50,
  UNDERLINE_POSITION = 51,
  UNDERLINE_THICKNESS = 52,
  STRIKEOUT_ASCENT = 53,
  STRIKEOUT_DESCENT = 54,
  ITALIC_ANGLE = 55,
  X_HEIGHT = 56,
  QUAD_WIDTH = 57,
  WEIGHT = 58,
  POINT_SIZE = 59,
  RESOLUTION = 60,
  COPYRIGHT = 61,
  NOTICE = 62,
  FONT_NAME = 63,
  FAMILY_NAME = 64,
  FULL_NAME = 65,
  CAP_HEIGHT = 66,
  WM_CLASS = 67,
  WM_TRANSIENT_FOR = 68,
  kLastPredefinedAtom = 68,
};

enum class ColormapState : int {
  Uninstalled = 0,
  Installed = 1,
};

enum class Colormap : int {
  None = 0,
};

enum class Mapping : int {
  Modifier = 0,
  Keyboard = 1,
  Pointer = 2,
};

enum class WindowClass : int {
  CopyFromParent = 0,
  InputOutput = 1,
  InputOnly = 2,
};

enum class CreateWindowAttribute : int {
  BackPixmap = 1 << 0,
  BackPixel = 1 << 1,
  BorderPixmap = 1 << 2,
  BorderPixel = 1 << 3,
  BitGravity = 1 << 4,
  WinGravity = 1 << 5,
  BackingStore = 1 << 6,
  BackingPlanes = 1 << 7,
  BackingPixel = 1 << 8,
  OverrideRedirect = 1 << 9,
  SaveUnder = 1 << 10,
  EventMask = 1 << 11,
  DontPropagate = 1 << 12,
  Colormap = 1 << 13,
  Cursor = 1 << 14,
};

enum class BackPixmap : int {
  None = 0,
  ParentRelative = 1,
};

enum class Gravity : int {
  BitForget = 0,
  WinUnmap = 0,
  NorthWest = 1,
  North = 2,
  NorthEast = 3,
  West = 4,
  Center = 5,
  East = 6,
  SouthWest = 7,
  South = 8,
  SouthEast = 9,
  Static = 10,
};

enum class MapState : int {
  Unmapped = 0,
  Unviewable = 1,
  Viewable = 2,
};

enum class SetMode : int {
  Insert = 0,
  Delete = 1,
};

enum class ConfigWindow : int {
  X = 1 << 0,
  Y = 1 << 1,
  Width = 1 << 2,
  Height = 1 << 3,
  BorderWidth = 1 << 4,
  Sibling = 1 << 5,
  StackMode = 1 << 6,
};

enum class StackMode : int {
  Above = 0,
  Below = 1,
  TopIf = 2,
  BottomIf = 3,
  Opposite = 4,
};

enum class Circulate : int {
  RaiseLowest = 0,
  LowerHighest = 1,
};

enum class PropMode : int {
  Replace = 0,
  Prepend = 1,
  Append = 2,
};

enum class GetPropertyType : int {
  Any = 0,
};

enum class SendEventDest : int {
  PointerWindow = 0,
  ItemFocus = 1,
};

enum class GrabMode : int {
  Sync = 0,
  Async = 1,
};

enum class GrabStatus : int {
  Success = 0,
  AlreadyGrabbed = 1,
  InvalidTime = 2,
  NotViewable = 3,
  Frozen = 4,
};

enum class Cursor : uint32_t {
  None = 0,
};

enum class ButtonIndex : int {
  Any = 0,
  c_1 = 1,
  c_2 = 2,
  c_3 = 3,
  c_4 = 4,
  c_5 = 5,
};

enum class Grab : int {
  Any = 0,
};

enum class Allow : int {
  AsyncPointer = 0,
  SyncPointer = 1,
  ReplayPointer = 2,
  AsyncKeyboard = 3,
  SyncKeyboard = 4,
  ReplayKeyboard = 5,
  AsyncBoth = 6,
  SyncBoth = 7,
};

enum class InputFocus : int {
  None = 0,
  PointerRoot = 1,
  Parent = 2,
  FollowKeyboard = 3,
};

enum class FontDraw : int {
  LeftToRight = 0,
  RightToLeft = 1,
};

enum class GraphicsContextAttribute : int {
  Function = 1 << 0,
  PlaneMask = 1 << 1,
  Foreground = 1 << 2,
  Background = 1 << 3,
  LineWidth = 1 << 4,
  LineStyle = 1 << 5,
  CapStyle = 1 << 6,
  JoinStyle = 1 << 7,
  FillStyle = 1 << 8,
  FillRule = 1 << 9,
  Tile = 1 << 10,
  Stipple = 1 << 11,
  TileStippleOriginX = 1 << 12,
  TileStippleOriginY = 1 << 13,
  Font = 1 << 14,
  SubwindowMode = 1 << 15,
  GraphicsExposures = 1 << 16,
  ClipOriginX = 1 << 17,
  ClipOriginY = 1 << 18,
  ClipMask = 1 << 19,
  DashOffset = 1 << 20,
  DashList = 1 << 21,
  ArcMode = 1 << 22,
};

enum class Gx : int {
  clear = 0,
  c_and = 1,
  andReverse = 2,
  copy = 3,
  andInverted = 4,
  noop = 5,
  c_xor = 6,
  c_or = 7,
  nor = 8,
  equiv = 9,
  invert = 10,
  orReverse = 11,
  copyInverted = 12,
  orInverted = 13,
  nand = 14,
  set = 15,
};

enum class LineStyle : int {
  Solid = 0,
  OnOffDash = 1,
  DoubleDash = 2,
};

enum class CapStyle : int {
  NotLast = 0,
  Butt = 1,
  Round = 2,
  Projecting = 3,
};

enum class JoinStyle : int {
  Miter = 0,
  Round = 1,
  Bevel = 2,
};

enum class FillStyle : int {
  Solid = 0,
  Tiled = 1,
  Stippled = 2,
  OpaqueStippled = 3,
};

enum class FillRule : int {
  EvenOdd = 0,
  Winding = 1,
};

enum class SubwindowMode : int {
  ClipByChildren = 0,
  IncludeInferiors = 1,
};

enum class ArcMode : int {
  Chord = 0,
  PieSlice = 1,
};

enum class ClipOrdering : int {
  Unsorted = 0,
  YSorted = 1,
  YXSorted = 2,
  YXBanded = 3,
};

enum class CoordMode : int {
  Origin = 0,
  Previous = 1,
};

enum class PolyShape : int {
  Complex = 0,
  Nonconvex = 1,
  Convex = 2,
};

enum class ImageFormat : int {
  XYBitmap = 0,
  XYPixmap = 1,
  ZPixmap = 2,
};

enum class ColormapAlloc : int {
  None = 0,
  All = 1,
};

enum class ColorFlag : int {
  Red = 1 << 0,
  Green = 1 << 1,
  Blue = 1 << 2,
};

enum class Pixmap : uint32_t {
  None = 0,
};

enum class Font : uint32_t {
  None = 0,
};

enum class QueryShapeOf : int {
  LargestCursor = 0,
  FastestTile = 1,
  FastestStipple = 2,
};

enum class Keyboard : int {
  KeyClickPercent = 1 << 0,
  BellPercent = 1 << 1,
  BellPitch = 1 << 2,
  BellDuration = 1 << 3,
  Led = 1 << 4,
  LedMode = 1 << 5,
  Key = 1 << 6,
  AutoRepeatMode = 1 << 7,
};

enum class LedMode : int {
  Off = 0,
  On = 1,
};

enum class AutoRepeatMode : int {
  Off = 0,
  On = 1,
  Default = 2,
};

enum class Blanking : int {
  NotPreferred = 0,
  Preferred = 1,
  Default = 2,
};

enum class Exposures : int {
  NotAllowed = 0,
  Allowed = 1,
  Default = 2,
};

enum class HostMode : int {
  Insert = 0,
  Delete = 1,
};

enum class Family : int {
  Internet = 0,
  DECnet = 1,
  Chaos = 2,
  ServerInterpreted = 5,
  Internet6 = 6,
};

enum class AccessControl : int {
  Disable = 0,
  Enable = 1,
};

enum class CloseDown : int {
  DestroyAll = 0,
  RetainPermanent = 1,
  RetainTemporary = 2,
};

enum class Kill : int {
  AllTemporary = 0,
};

enum class ScreenSaverMode : int {
  Reset = 0,
  Active = 1,
};

enum class MappingStatus : int {
  Success = 0,
  Busy = 1,
  Failure = 2,
};

enum class MapIndex : int {
  Shift = 0,
  Lock = 1,
  Control = 2,
  c_1 = 3,
  c_2 = 4,
  c_3 = 5,
  c_4 = 6,
  c_5 = 7,
};

struct Drawable {
  Drawable() : value{} {}

  Drawable(Window value) : value{static_cast<uint32_t>(value)} {}
  operator Window() const { return static_cast<Window>(value); }

  Drawable(Pixmap value) : value{static_cast<uint32_t>(value)} {}
  operator Pixmap() const { return static_cast<Pixmap>(value); }

  uint32_t value{};
};

struct Fontable {
  Fontable() : value{} {}

  Fontable(Font value) : value{static_cast<uint32_t>(value)} {}
  operator Font() const { return static_cast<Font>(value); }

  Fontable(GraphicsContext value) : value{static_cast<uint32_t>(value)} {}
  operator GraphicsContext() const {
    return static_cast<GraphicsContext>(value);
  }

  uint32_t value{};
};

struct Char16 {
  bool operator==(const Char16& other) const {
    return byte1 == other.byte1 && byte2 == other.byte2;
  }

  uint8_t byte1{};
  uint8_t byte2{};
};

struct Point {
  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }

  int16_t x{};
  int16_t y{};
};

struct Rectangle {
  bool operator==(const Rectangle& other) const {
    return x == other.x && y == other.y && width == other.width &&
           height == other.height;
  }

  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
};

struct Arc {
  bool operator==(const Arc& other) const {
    return x == other.x && y == other.y && width == other.width &&
           height == other.height && angle1 == other.angle1 &&
           angle2 == other.angle2;
  }

  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  int16_t angle1{};
  int16_t angle2{};
};

struct Format {
  bool operator==(const Format& other) const {
    return depth == other.depth && bits_per_pixel == other.bits_per_pixel &&
           scanline_pad == other.scanline_pad;
  }

  uint8_t depth{};
  uint8_t bits_per_pixel{};
  uint8_t scanline_pad{};
};

struct VisualType {
  bool operator==(const VisualType& other) const {
    return visual_id == other.visual_id && c_class == other.c_class &&
           bits_per_rgb_value == other.bits_per_rgb_value &&
           colormap_entries == other.colormap_entries &&
           red_mask == other.red_mask && green_mask == other.green_mask &&
           blue_mask == other.blue_mask;
  }

  VisualId visual_id{};
  VisualClass c_class{};
  uint8_t bits_per_rgb_value{};
  uint16_t colormap_entries{};
  uint32_t red_mask{};
  uint32_t green_mask{};
  uint32_t blue_mask{};
};

struct Depth {
  bool operator==(const Depth& other) const {
    return depth == other.depth && visuals == other.visuals;
  }

  uint8_t depth{};
  std::vector<VisualType> visuals{};
};

struct Screen {
  bool operator==(const Screen& other) const {
    return root == other.root && default_colormap == other.default_colormap &&
           white_pixel == other.white_pixel &&
           black_pixel == other.black_pixel &&
           current_input_masks == other.current_input_masks &&
           width_in_pixels == other.width_in_pixels &&
           height_in_pixels == other.height_in_pixels &&
           width_in_millimeters == other.width_in_millimeters &&
           height_in_millimeters == other.height_in_millimeters &&
           min_installed_maps == other.min_installed_maps &&
           max_installed_maps == other.max_installed_maps &&
           root_visual == other.root_visual &&
           backing_stores == other.backing_stores &&
           save_unders == other.save_unders && root_depth == other.root_depth &&
           allowed_depths == other.allowed_depths;
  }

  Window root{};
  ColorMap default_colormap{};
  uint32_t white_pixel{};
  uint32_t black_pixel{};
  EventMask current_input_masks{};
  uint16_t width_in_pixels{};
  uint16_t height_in_pixels{};
  uint16_t width_in_millimeters{};
  uint16_t height_in_millimeters{};
  uint16_t min_installed_maps{};
  uint16_t max_installed_maps{};
  VisualId root_visual{};
  BackingStore backing_stores{};
  uint8_t save_unders{};
  uint8_t root_depth{};
  std::vector<Depth> allowed_depths{};
};

struct SetupRequest {
  bool operator==(const SetupRequest& other) const {
    return byte_order == other.byte_order &&
           protocol_major_version == other.protocol_major_version &&
           protocol_minor_version == other.protocol_minor_version &&
           authorization_protocol_name == other.authorization_protocol_name &&
           authorization_protocol_data == other.authorization_protocol_data;
  }

  uint8_t byte_order{};
  uint16_t protocol_major_version{};
  uint16_t protocol_minor_version{};
  std::string authorization_protocol_name{};
  std::string authorization_protocol_data{};
};

struct SetupFailed {
  bool operator==(const SetupFailed& other) const {
    return status == other.status &&
           protocol_major_version == other.protocol_major_version &&
           protocol_minor_version == other.protocol_minor_version &&
           length == other.length && reason == other.reason;
  }

  uint8_t status{};
  uint16_t protocol_major_version{};
  uint16_t protocol_minor_version{};
  uint16_t length{};
  std::string reason{};
};

struct SetupAuthenticate {
  bool operator==(const SetupAuthenticate& other) const {
    return status == other.status && length == other.length &&
           reason == other.reason;
  }

  uint8_t status{};
  uint16_t length{};
  std::string reason{};
};

struct Setup {
  bool operator==(const Setup& other) const {
    return status == other.status &&
           protocol_major_version == other.protocol_major_version &&
           protocol_minor_version == other.protocol_minor_version &&
           length == other.length && release_number == other.release_number &&
           resource_id_base == other.resource_id_base &&
           resource_id_mask == other.resource_id_mask &&
           motion_buffer_size == other.motion_buffer_size &&
           maximum_request_length == other.maximum_request_length &&
           image_byte_order == other.image_byte_order &&
           bitmap_format_bit_order == other.bitmap_format_bit_order &&
           bitmap_format_scanline_unit == other.bitmap_format_scanline_unit &&
           bitmap_format_scanline_pad == other.bitmap_format_scanline_pad &&
           min_keycode == other.min_keycode &&
           max_keycode == other.max_keycode && vendor == other.vendor &&
           pixmap_formats == other.pixmap_formats && roots == other.roots;
  }

  uint8_t status{};
  uint16_t protocol_major_version{};
  uint16_t protocol_minor_version{};
  uint16_t length{};
  uint32_t release_number{};
  uint32_t resource_id_base{};
  uint32_t resource_id_mask{};
  uint32_t motion_buffer_size{};
  uint16_t maximum_request_length{};
  ImageOrder image_byte_order{};
  ImageOrder bitmap_format_bit_order{};
  uint8_t bitmap_format_scanline_unit{};
  uint8_t bitmap_format_scanline_pad{};
  KeyCode min_keycode{};
  KeyCode max_keycode{};
  std::string vendor{};
  std::vector<Format> pixmap_formats{};
  std::vector<Screen> roots{};
};

struct KeyEvent {
  static constexpr uint8_t type_id = 44;
  enum Opcode {
    Press = 2,
    Release = 3,
  } opcode{};
  KeyCode detail{};
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
};

struct ButtonEvent {
  static constexpr uint8_t type_id = 45;
  enum Opcode {
    Press = 4,
    Release = 5,
  } opcode{};
  Button detail{};
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
};

struct MotionNotifyEvent {
  static constexpr uint8_t type_id = 46;
  static constexpr uint8_t opcode = 6;
  Motion detail{};
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
};

struct CrossingEvent {
  static constexpr uint8_t type_id = 47;
  enum Opcode {
    EnterNotify = 7,
    LeaveNotify = 8,
  } opcode{};
  NotifyDetail detail{};
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
  NotifyMode mode{};
  uint8_t same_screen_focus{};
};

struct FocusEvent {
  static constexpr uint8_t type_id = 48;
  enum Opcode {
    In = 9,
    Out = 10,
  } opcode{};
  NotifyDetail detail{};
  uint16_t sequence{};
  Window event{};
  NotifyMode mode{};
};

struct KeymapNotifyEvent {
  static constexpr uint8_t type_id = 49;
  static constexpr uint8_t opcode = 11;
  std::array<uint8_t, 31> keys{};
};

struct ExposeEvent {
  static constexpr uint8_t type_id = 50;
  static constexpr uint8_t opcode = 12;
  uint16_t sequence{};
  Window window{};
  uint16_t x{};
  uint16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t count{};
};

struct GraphicsExposureEvent {
  static constexpr uint8_t type_id = 51;
  static constexpr uint8_t opcode = 13;
  uint16_t sequence{};
  Drawable drawable{};
  uint16_t x{};
  uint16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t minor_opcode{};
  uint16_t count{};
  uint8_t major_opcode{};
};

struct NoExposureEvent {
  static constexpr uint8_t type_id = 52;
  static constexpr uint8_t opcode = 14;
  uint16_t sequence{};
  Drawable drawable{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};
};

struct VisibilityNotifyEvent {
  static constexpr uint8_t type_id = 53;
  static constexpr uint8_t opcode = 15;
  uint16_t sequence{};
  Window window{};
  Visibility state{};
};

struct CreateNotifyEvent {
  static constexpr uint8_t type_id = 54;
  static constexpr uint8_t opcode = 16;
  uint16_t sequence{};
  Window parent{};
  Window window{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t border_width{};
  uint8_t override_redirect{};
};

struct DestroyNotifyEvent {
  static constexpr uint8_t type_id = 55;
  static constexpr uint8_t opcode = 17;
  uint16_t sequence{};
  Window event{};
  Window window{};
};

struct UnmapNotifyEvent {
  static constexpr uint8_t type_id = 56;
  static constexpr uint8_t opcode = 18;
  uint16_t sequence{};
  Window event{};
  Window window{};
  uint8_t from_configure{};
};

struct MapNotifyEvent {
  static constexpr uint8_t type_id = 57;
  static constexpr uint8_t opcode = 19;
  uint16_t sequence{};
  Window event{};
  Window window{};
  uint8_t override_redirect{};
};

struct MapRequestEvent {
  static constexpr uint8_t type_id = 58;
  static constexpr uint8_t opcode = 20;
  uint16_t sequence{};
  Window parent{};
  Window window{};
};

struct ReparentNotifyEvent {
  static constexpr uint8_t type_id = 59;
  static constexpr uint8_t opcode = 21;
  uint16_t sequence{};
  Window event{};
  Window window{};
  Window parent{};
  int16_t x{};
  int16_t y{};
  uint8_t override_redirect{};
};

struct ConfigureNotifyEvent {
  static constexpr uint8_t type_id = 60;
  static constexpr uint8_t opcode = 22;
  uint16_t sequence{};
  Window event{};
  Window window{};
  Window above_sibling{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t border_width{};
  uint8_t override_redirect{};
};

struct ConfigureRequestEvent {
  static constexpr uint8_t type_id = 61;
  static constexpr uint8_t opcode = 23;
  StackMode stack_mode{};
  uint16_t sequence{};
  Window parent{};
  Window window{};
  Window sibling{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t border_width{};
  ConfigWindow value_mask{};
};

struct GravityNotifyEvent {
  static constexpr uint8_t type_id = 62;
  static constexpr uint8_t opcode = 24;
  uint16_t sequence{};
  Window event{};
  Window window{};
  int16_t x{};
  int16_t y{};
};

struct ResizeRequestEvent {
  static constexpr uint8_t type_id = 63;
  static constexpr uint8_t opcode = 25;
  uint16_t sequence{};
  Window window{};
  uint16_t width{};
  uint16_t height{};
};

struct CirculateEvent {
  static constexpr uint8_t type_id = 64;
  enum Opcode {
    Notify = 26,
    Request = 27,
  } opcode{};
  uint16_t sequence{};
  Window event{};
  Window window{};
  Place place{};
};

struct PropertyNotifyEvent {
  static constexpr uint8_t type_id = 65;
  static constexpr uint8_t opcode = 28;
  uint16_t sequence{};
  Window window{};
  Atom atom{};
  Time time{};
  Property state{};
};

struct SelectionClearEvent {
  static constexpr uint8_t type_id = 66;
  static constexpr uint8_t opcode = 29;
  uint16_t sequence{};
  Time time{};
  Window owner{};
  Atom selection{};
};

struct SelectionRequestEvent {
  static constexpr uint8_t type_id = 67;
  static constexpr uint8_t opcode = 30;
  uint16_t sequence{};
  Time time{};
  Window owner{};
  Window requestor{};
  Atom selection{};
  Atom target{};
  Atom property{};
};

struct SelectionNotifyEvent {
  static constexpr uint8_t type_id = 68;
  static constexpr uint8_t opcode = 31;
  uint16_t sequence{};
  Time time{};
  Window requestor{};
  Atom selection{};
  Atom target{};
  Atom property{};
};

struct ColormapNotifyEvent {
  static constexpr uint8_t type_id = 69;
  static constexpr uint8_t opcode = 32;
  uint16_t sequence{};
  Window window{};
  ColorMap colormap{};
  uint8_t c_new{};
  ColormapState state{};
};

union ClientMessageData {
  ClientMessageData() { memset(this, 0, sizeof(*this)); }

  std::array<uint8_t, 20> data8;
  std::array<uint16_t, 10> data16;
  std::array<uint32_t, 5> data32;
};
static_assert(std::is_trivially_copyable<ClientMessageData>::value, "");

struct ClientMessageEvent {
  static constexpr uint8_t type_id = 70;
  static constexpr uint8_t opcode = 33;
  uint8_t format{};
  uint16_t sequence{};
  Window window{};
  Atom type{};
  ClientMessageData data{};
};

struct MappingNotifyEvent {
  static constexpr uint8_t type_id = 71;
  static constexpr uint8_t opcode = 34;
  uint16_t sequence{};
  Mapping request{};
  KeyCode first_keycode{};
  uint8_t count{};
};

struct GeGenericEvent {
  static constexpr uint8_t type_id = 72;
  static constexpr uint8_t opcode = 35;
  uint16_t sequence{};
};

struct RequestError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct ValueError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct WindowError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct PixmapError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct AtomError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct CursorError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct FontError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct MatchError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct DrawableError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct AccessError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct AllocError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct ColormapError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct GContextError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct IDChoiceError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct NameError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct LengthError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct ImplementationError : public x11::Error {
  uint16_t sequence{};
  uint32_t bad_value{};
  uint16_t minor_opcode{};
  uint8_t major_opcode{};

  std::string ToString() const override;
};

struct TimeCoord {
  bool operator==(const TimeCoord& other) const {
    return time == other.time && x == other.x && y == other.y;
  }

  Time time{};
  int16_t x{};
  int16_t y{};
};

struct FontProperty {
  bool operator==(const FontProperty& other) const {
    return name == other.name && value == other.value;
  }

  Atom name{};
  uint32_t value{};
};

struct CharInfo {
  bool operator==(const CharInfo& other) const {
    return left_side_bearing == other.left_side_bearing &&
           right_side_bearing == other.right_side_bearing &&
           character_width == other.character_width && ascent == other.ascent &&
           descent == other.descent && attributes == other.attributes;
  }

  int16_t left_side_bearing{};
  int16_t right_side_bearing{};
  int16_t character_width{};
  int16_t ascent{};
  int16_t descent{};
  uint16_t attributes{};
};

struct Str {
  bool operator==(const Str& other) const { return name == other.name; }

  std::string name{};
};

struct Segment {
  bool operator==(const Segment& other) const {
    return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
  }

  int16_t x1{};
  int16_t y1{};
  int16_t x2{};
  int16_t y2{};
};

struct ColorItem {
  bool operator==(const ColorItem& other) const {
    return pixel == other.pixel && red == other.red && green == other.green &&
           blue == other.blue && flags == other.flags;
  }

  uint32_t pixel{};
  uint16_t red{};
  uint16_t green{};
  uint16_t blue{};
  ColorFlag flags{};
};

struct Rgb {
  bool operator==(const Rgb& other) const {
    return red == other.red && green == other.green && blue == other.blue;
  }

  uint16_t red{};
  uint16_t green{};
  uint16_t blue{};
};

struct Host {
  bool operator==(const Host& other) const {
    return family == other.family && address == other.address;
  }

  Family family{};
  std::vector<uint8_t> address{};
};

struct CreateWindowRequest {
  uint8_t depth{};
  Window wid{};
  Window parent{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t border_width{};
  WindowClass c_class{};
  VisualId visual{};
  std::optional<Pixmap> background_pixmap{};
  std::optional<uint32_t> background_pixel{};
  std::optional<Pixmap> border_pixmap{};
  std::optional<uint32_t> border_pixel{};
  std::optional<Gravity> bit_gravity{};
  std::optional<Gravity> win_gravity{};
  std::optional<BackingStore> backing_store{};
  std::optional<uint32_t> backing_planes{};
  std::optional<uint32_t> backing_pixel{};
  std::optional<Bool32> override_redirect{};
  std::optional<Bool32> save_under{};
  std::optional<EventMask> event_mask{};
  std::optional<EventMask> do_not_propogate_mask{};
  std::optional<ColorMap> colormap{};
  std::optional<Cursor> cursor{};
};

using CreateWindowResponse = Response<void>;

struct ChangeWindowAttributesRequest {
  Window window{};
  std::optional<Pixmap> background_pixmap{};
  std::optional<uint32_t> background_pixel{};
  std::optional<Pixmap> border_pixmap{};
  std::optional<uint32_t> border_pixel{};
  std::optional<Gravity> bit_gravity{};
  std::optional<Gravity> win_gravity{};
  std::optional<BackingStore> backing_store{};
  std::optional<uint32_t> backing_planes{};
  std::optional<uint32_t> backing_pixel{};
  std::optional<Bool32> override_redirect{};
  std::optional<Bool32> save_under{};
  std::optional<EventMask> event_mask{};
  std::optional<EventMask> do_not_propogate_mask{};
  std::optional<ColorMap> colormap{};
  std::optional<Cursor> cursor{};
};

using ChangeWindowAttributesResponse = Response<void>;

struct GetWindowAttributesRequest {
  Window window{};
};

struct GetWindowAttributesReply {
  BackingStore backing_store{};
  uint16_t sequence{};
  VisualId visual{};
  WindowClass c_class{};
  Gravity bit_gravity{};
  Gravity win_gravity{};
  uint32_t backing_planes{};
  uint32_t backing_pixel{};
  uint8_t save_under{};
  uint8_t map_is_installed{};
  MapState map_state{};
  uint8_t override_redirect{};
  ColorMap colormap{};
  EventMask all_event_masks{};
  EventMask your_event_mask{};
  EventMask do_not_propagate_mask{};
};

using GetWindowAttributesResponse = Response<GetWindowAttributesReply>;

struct DestroyWindowRequest {
  Window window{};
};

using DestroyWindowResponse = Response<void>;

struct DestroySubwindowsRequest {
  Window window{};
};

using DestroySubwindowsResponse = Response<void>;

struct ChangeSaveSetRequest {
  SetMode mode{};
  Window window{};
};

using ChangeSaveSetResponse = Response<void>;

struct ReparentWindowRequest {
  Window window{};
  Window parent{};
  int16_t x{};
  int16_t y{};
};

using ReparentWindowResponse = Response<void>;

struct MapWindowRequest {
  Window window{};
};

using MapWindowResponse = Response<void>;

struct MapSubwindowsRequest {
  Window window{};
};

using MapSubwindowsResponse = Response<void>;

struct UnmapWindowRequest {
  Window window{};
};

using UnmapWindowResponse = Response<void>;

struct UnmapSubwindowsRequest {
  Window window{};
};

using UnmapSubwindowsResponse = Response<void>;

struct ConfigureWindowRequest {
  Window window{};
  std::optional<int32_t> x{};
  std::optional<int32_t> y{};
  std::optional<uint32_t> width{};
  std::optional<uint32_t> height{};
  std::optional<uint32_t> border_width{};
  std::optional<Window> sibling{};
  std::optional<StackMode> stack_mode{};
};

using ConfigureWindowResponse = Response<void>;

struct CirculateWindowRequest {
  Circulate direction{};
  Window window{};
};

using CirculateWindowResponse = Response<void>;

struct GetGeometryRequest {
  Drawable drawable{};
};

struct GetGeometryReply {
  uint8_t depth{};
  uint16_t sequence{};
  Window root{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint16_t border_width{};
};

using GetGeometryResponse = Response<GetGeometryReply>;

struct QueryTreeRequest {
  Window window{};
};

struct QueryTreeReply {
  uint16_t sequence{};
  Window root{};
  Window parent{};
  std::vector<Window> children{};
};

using QueryTreeResponse = Response<QueryTreeReply>;

struct InternAtomRequest {
  uint8_t only_if_exists{};
  std::string name{};
};

struct InternAtomReply {
  uint16_t sequence{};
  Atom atom{};
};

using InternAtomResponse = Response<InternAtomReply>;

struct GetAtomNameRequest {
  Atom atom{};
};

struct GetAtomNameReply {
  uint16_t sequence{};
  std::string name{};
};

using GetAtomNameResponse = Response<GetAtomNameReply>;

struct ChangePropertyRequest {
  PropMode mode{};
  Window window{};
  Atom property{};
  Atom type{};
  uint8_t format{};
  uint32_t data_len{};
  scoped_refptr<base::RefCountedMemory> data{};
};

using ChangePropertyResponse = Response<void>;

struct DeletePropertyRequest {
  Window window{};
  Atom property{};
};

using DeletePropertyResponse = Response<void>;

struct GetPropertyRequest {
  uint8_t c_delete{};
  Window window{};
  Atom property{};
  Atom type{};
  uint32_t long_offset{};
  uint32_t long_length{};
};

struct GetPropertyReply {
  uint8_t format{};
  uint16_t sequence{};
  Atom type{};
  uint32_t bytes_after{};
  uint32_t value_len{};
  scoped_refptr<UnsizedRefCountedMemory> value{};
};

using GetPropertyResponse = Response<GetPropertyReply>;

struct ListPropertiesRequest {
  Window window{};
};

struct ListPropertiesReply {
  uint16_t sequence{};
  std::vector<Atom> atoms{};
};

using ListPropertiesResponse = Response<ListPropertiesReply>;

struct SetSelectionOwnerRequest {
  Window owner{};
  Atom selection{};
  Time time{};
};

using SetSelectionOwnerResponse = Response<void>;

struct GetSelectionOwnerRequest {
  Atom selection{};
};

struct GetSelectionOwnerReply {
  uint16_t sequence{};
  Window owner{};
};

using GetSelectionOwnerResponse = Response<GetSelectionOwnerReply>;

struct ConvertSelectionRequest {
  Window requestor{};
  Atom selection{};
  Atom target{};
  Atom property{};
  Time time{};
};

using ConvertSelectionResponse = Response<void>;

struct SendEventRequest {
  uint8_t propagate{};
  Window destination{};
  EventMask event_mask{};
  std::array<char, 32> event{};
};

using SendEventResponse = Response<void>;

struct GrabPointerRequest {
  uint8_t owner_events{};
  Window grab_window{};
  EventMask event_mask{};
  GrabMode pointer_mode{};
  GrabMode keyboard_mode{};
  Window confine_to{};
  Cursor cursor{};
  Time time{};
};

struct GrabPointerReply {
  GrabStatus status{};
  uint16_t sequence{};
};

using GrabPointerResponse = Response<GrabPointerReply>;

struct UngrabPointerRequest {
  Time time{};
};

using UngrabPointerResponse = Response<void>;

struct GrabButtonRequest {
  uint8_t owner_events{};
  Window grab_window{};
  EventMask event_mask{};
  GrabMode pointer_mode{};
  GrabMode keyboard_mode{};
  Window confine_to{};
  Cursor cursor{};
  ButtonIndex button{};
  ModMask modifiers{};
};

using GrabButtonResponse = Response<void>;

struct UngrabButtonRequest {
  ButtonIndex button{};
  Window grab_window{};
  ModMask modifiers{};
};

using UngrabButtonResponse = Response<void>;

struct ChangeActivePointerGrabRequest {
  Cursor cursor{};
  Time time{};
  EventMask event_mask{};
};

using ChangeActivePointerGrabResponse = Response<void>;

struct GrabKeyboardRequest {
  uint8_t owner_events{};
  Window grab_window{};
  Time time{};
  GrabMode pointer_mode{};
  GrabMode keyboard_mode{};
};

struct GrabKeyboardReply {
  GrabStatus status{};
  uint16_t sequence{};
};

using GrabKeyboardResponse = Response<GrabKeyboardReply>;

struct UngrabKeyboardRequest {
  Time time{};
};

using UngrabKeyboardResponse = Response<void>;

struct GrabKeyRequest {
  uint8_t owner_events{};
  Window grab_window{};
  ModMask modifiers{};
  KeyCode key{};
  GrabMode pointer_mode{};
  GrabMode keyboard_mode{};
};

using GrabKeyResponse = Response<void>;

struct UngrabKeyRequest {
  KeyCode key{};
  Window grab_window{};
  ModMask modifiers{};
};

using UngrabKeyResponse = Response<void>;

struct AllowEventsRequest {
  Allow mode{};
  Time time{};
};

using AllowEventsResponse = Response<void>;

struct GrabServerRequest {};

using GrabServerResponse = Response<void>;

struct UngrabServerRequest {};

using UngrabServerResponse = Response<void>;

struct QueryPointerRequest {
  Window window{};
};

struct QueryPointerReply {
  uint8_t same_screen{};
  uint16_t sequence{};
  Window root{};
  Window child{};
  int16_t root_x{};
  int16_t root_y{};
  int16_t win_x{};
  int16_t win_y{};
  KeyButMask mask{};
};

using QueryPointerResponse = Response<QueryPointerReply>;

struct GetMotionEventsRequest {
  Window window{};
  Time start{};
  Time stop{};
};

struct GetMotionEventsReply {
  uint16_t sequence{};
  std::vector<TimeCoord> events{};
};

using GetMotionEventsResponse = Response<GetMotionEventsReply>;

struct TranslateCoordinatesRequest {
  Window src_window{};
  Window dst_window{};
  int16_t src_x{};
  int16_t src_y{};
};

struct TranslateCoordinatesReply {
  uint8_t same_screen{};
  uint16_t sequence{};
  Window child{};
  int16_t dst_x{};
  int16_t dst_y{};
};

using TranslateCoordinatesResponse = Response<TranslateCoordinatesReply>;

struct WarpPointerRequest {
  Window src_window{};
  Window dst_window{};
  int16_t src_x{};
  int16_t src_y{};
  uint16_t src_width{};
  uint16_t src_height{};
  int16_t dst_x{};
  int16_t dst_y{};
};

using WarpPointerResponse = Response<void>;

struct SetInputFocusRequest {
  InputFocus revert_to{};
  Window focus{};
  Time time{};
};

using SetInputFocusResponse = Response<void>;

struct GetInputFocusRequest {};

struct GetInputFocusReply {
  InputFocus revert_to{};
  uint16_t sequence{};
  Window focus{};
};

using GetInputFocusResponse = Response<GetInputFocusReply>;

struct QueryKeymapRequest {};

struct QueryKeymapReply {
  uint16_t sequence{};
  std::array<uint8_t, 32> keys{};
};

using QueryKeymapResponse = Response<QueryKeymapReply>;

struct OpenFontRequest {
  Font fid{};
  std::string name{};
};

using OpenFontResponse = Response<void>;

struct CloseFontRequest {
  Font font{};
};

using CloseFontResponse = Response<void>;

struct QueryFontRequest {
  Fontable font{};
};

struct QueryFontReply {
  uint16_t sequence{};
  CharInfo min_bounds{};
  CharInfo max_bounds{};
  uint16_t min_char_or_byte2{};
  uint16_t max_char_or_byte2{};
  uint16_t default_char{};
  FontDraw draw_direction{};
  uint8_t min_byte1{};
  uint8_t max_byte1{};
  uint8_t all_chars_exist{};
  int16_t font_ascent{};
  int16_t font_descent{};
  std::vector<FontProperty> properties{};
  std::vector<CharInfo> char_infos{};
};

using QueryFontResponse = Response<QueryFontReply>;

struct QueryTextExtentsRequest {
  Fontable font{};
  std::vector<Char16> string{};
};

struct QueryTextExtentsReply {
  FontDraw draw_direction{};
  uint16_t sequence{};
  int16_t font_ascent{};
  int16_t font_descent{};
  int16_t overall_ascent{};
  int16_t overall_descent{};
  int32_t overall_width{};
  int32_t overall_left{};
  int32_t overall_right{};
};

using QueryTextExtentsResponse = Response<QueryTextExtentsReply>;

struct ListFontsRequest {
  uint16_t max_names{};
  std::string pattern{};
};

struct ListFontsReply {
  uint16_t sequence{};
  std::vector<Str> names{};
};

using ListFontsResponse = Response<ListFontsReply>;

struct ListFontsWithInfoRequest {
  uint16_t max_names{};
  std::string pattern{};
};

struct ListFontsWithInfoReply {
  uint16_t sequence{};
  CharInfo min_bounds{};
  CharInfo max_bounds{};
  uint16_t min_char_or_byte2{};
  uint16_t max_char_or_byte2{};
  uint16_t default_char{};
  FontDraw draw_direction{};
  uint8_t min_byte1{};
  uint8_t max_byte1{};
  uint8_t all_chars_exist{};
  int16_t font_ascent{};
  int16_t font_descent{};
  uint32_t replies_hint{};
  std::vector<FontProperty> properties{};
  std::string name{};
};

using ListFontsWithInfoResponse = Response<ListFontsWithInfoReply>;

struct SetFontPathRequest {
  std::vector<Str> font{};
};

using SetFontPathResponse = Response<void>;

struct GetFontPathRequest {};

struct GetFontPathReply {
  uint16_t sequence{};
  std::vector<Str> path{};
};

using GetFontPathResponse = Response<GetFontPathReply>;

struct CreatePixmapRequest {
  uint8_t depth{};
  Pixmap pid{};
  Drawable drawable{};
  uint16_t width{};
  uint16_t height{};
};

using CreatePixmapResponse = Response<void>;

struct FreePixmapRequest {
  Pixmap pixmap{};
};

using FreePixmapResponse = Response<void>;

struct CreateGCRequest {
  GraphicsContext cid{};
  Drawable drawable{};
  std::optional<Gx> function{};
  std::optional<uint32_t> plane_mask{};
  std::optional<uint32_t> foreground{};
  std::optional<uint32_t> background{};
  std::optional<uint32_t> line_width{};
  std::optional<LineStyle> line_style{};
  std::optional<CapStyle> cap_style{};
  std::optional<JoinStyle> join_style{};
  std::optional<FillStyle> fill_style{};
  std::optional<FillRule> fill_rule{};
  std::optional<Pixmap> tile{};
  std::optional<Pixmap> stipple{};
  std::optional<int32_t> tile_stipple_x_origin{};
  std::optional<int32_t> tile_stipple_y_origin{};
  std::optional<Font> font{};
  std::optional<SubwindowMode> subwindow_mode{};
  std::optional<Bool32> graphics_exposures{};
  std::optional<int32_t> clip_x_origin{};
  std::optional<int32_t> clip_y_origin{};
  std::optional<Pixmap> clip_mask{};
  std::optional<uint32_t> dash_offset{};
  std::optional<uint32_t> dashes{};
  std::optional<ArcMode> arc_mode{};
};

using CreateGCResponse = Response<void>;

struct ChangeGCRequest {
  GraphicsContext gc{};
  std::optional<Gx> function{};
  std::optional<uint32_t> plane_mask{};
  std::optional<uint32_t> foreground{};
  std::optional<uint32_t> background{};
  std::optional<uint32_t> line_width{};
  std::optional<LineStyle> line_style{};
  std::optional<CapStyle> cap_style{};
  std::optional<JoinStyle> join_style{};
  std::optional<FillStyle> fill_style{};
  std::optional<FillRule> fill_rule{};
  std::optional<Pixmap> tile{};
  std::optional<Pixmap> stipple{};
  std::optional<int32_t> tile_stipple_x_origin{};
  std::optional<int32_t> tile_stipple_y_origin{};
  std::optional<Font> font{};
  std::optional<SubwindowMode> subwindow_mode{};
  std::optional<Bool32> graphics_exposures{};
  std::optional<int32_t> clip_x_origin{};
  std::optional<int32_t> clip_y_origin{};
  std::optional<Pixmap> clip_mask{};
  std::optional<uint32_t> dash_offset{};
  std::optional<uint32_t> dashes{};
  std::optional<ArcMode> arc_mode{};
};

using ChangeGCResponse = Response<void>;

struct CopyGCRequest {
  GraphicsContext src_gc{};
  GraphicsContext dst_gc{};
  GraphicsContextAttribute value_mask{};
};

using CopyGCResponse = Response<void>;

struct SetDashesRequest {
  GraphicsContext gc{};
  uint16_t dash_offset{};
  std::vector<uint8_t> dashes{};
};

using SetDashesResponse = Response<void>;

struct SetClipRectanglesRequest {
  ClipOrdering ordering{};
  GraphicsContext gc{};
  int16_t clip_x_origin{};
  int16_t clip_y_origin{};
  std::vector<Rectangle> rectangles{};
};

using SetClipRectanglesResponse = Response<void>;

struct FreeGCRequest {
  GraphicsContext gc{};
};

using FreeGCResponse = Response<void>;

struct ClearAreaRequest {
  uint8_t exposures{};
  Window window{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
};

using ClearAreaResponse = Response<void>;

struct CopyAreaRequest {
  Drawable src_drawable{};
  Drawable dst_drawable{};
  GraphicsContext gc{};
  int16_t src_x{};
  int16_t src_y{};
  int16_t dst_x{};
  int16_t dst_y{};
  uint16_t width{};
  uint16_t height{};
};

using CopyAreaResponse = Response<void>;

struct CopyPlaneRequest {
  Drawable src_drawable{};
  Drawable dst_drawable{};
  GraphicsContext gc{};
  int16_t src_x{};
  int16_t src_y{};
  int16_t dst_x{};
  int16_t dst_y{};
  uint16_t width{};
  uint16_t height{};
  uint32_t bit_plane{};
};

using CopyPlaneResponse = Response<void>;

struct PolyPointRequest {
  CoordMode coordinate_mode{};
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Point> points{};
};

using PolyPointResponse = Response<void>;

struct PolyLineRequest {
  CoordMode coordinate_mode{};
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Point> points{};
};

using PolyLineResponse = Response<void>;

struct PolySegmentRequest {
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Segment> segments{};
};

using PolySegmentResponse = Response<void>;

struct PolyRectangleRequest {
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Rectangle> rectangles{};
};

using PolyRectangleResponse = Response<void>;

struct PolyArcRequest {
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Arc> arcs{};
};

using PolyArcResponse = Response<void>;

struct FillPolyRequest {
  Drawable drawable{};
  GraphicsContext gc{};
  PolyShape shape{};
  CoordMode coordinate_mode{};
  std::vector<Point> points{};
};

using FillPolyResponse = Response<void>;

struct PolyFillRectangleRequest {
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Rectangle> rectangles{};
};

using PolyFillRectangleResponse = Response<void>;

struct PolyFillArcRequest {
  Drawable drawable{};
  GraphicsContext gc{};
  std::vector<Arc> arcs{};
};

using PolyFillArcResponse = Response<void>;

struct PutImageRequest {
  ImageFormat format{};
  Drawable drawable{};
  GraphicsContext gc{};
  uint16_t width{};
  uint16_t height{};
  int16_t dst_x{};
  int16_t dst_y{};
  uint8_t left_pad{};
  uint8_t depth{};
  scoped_refptr<base::RefCountedMemory> data{};
};

using PutImageResponse = Response<void>;

struct GetImageRequest {
  ImageFormat format{};
  Drawable drawable{};
  int16_t x{};
  int16_t y{};
  uint16_t width{};
  uint16_t height{};
  uint32_t plane_mask{};
};

struct GetImageReply {
  uint8_t depth{};
  uint16_t sequence{};
  VisualId visual{};
  scoped_refptr<UnsizedRefCountedMemory> data{};
};

using GetImageResponse = Response<GetImageReply>;

struct PolyText8Request {
  Drawable drawable{};
  GraphicsContext gc{};
  int16_t x{};
  int16_t y{};
  std::vector<uint8_t> items{};
};

using PolyText8Response = Response<void>;

struct PolyText16Request {
  Drawable drawable{};
  GraphicsContext gc{};
  int16_t x{};
  int16_t y{};
  std::vector<uint8_t> items{};
};

using PolyText16Response = Response<void>;

struct ImageText8Request {
  Drawable drawable{};
  GraphicsContext gc{};
  int16_t x{};
  int16_t y{};
  std::string string{};
};

using ImageText8Response = Response<void>;

struct ImageText16Request {
  Drawable drawable{};
  GraphicsContext gc{};
  int16_t x{};
  int16_t y{};
  std::vector<Char16> string{};
};

using ImageText16Response = Response<void>;

struct CreateColormapRequest {
  ColormapAlloc alloc{};
  ColorMap mid{};
  Window window{};
  VisualId visual{};
};

using CreateColormapResponse = Response<void>;

struct FreeColormapRequest {
  ColorMap cmap{};
};

using FreeColormapResponse = Response<void>;

struct CopyColormapAndFreeRequest {
  ColorMap mid{};
  ColorMap src_cmap{};
};

using CopyColormapAndFreeResponse = Response<void>;

struct InstallColormapRequest {
  ColorMap cmap{};
};

using InstallColormapResponse = Response<void>;

struct UninstallColormapRequest {
  ColorMap cmap{};
};

using UninstallColormapResponse = Response<void>;

struct ListInstalledColormapsRequest {
  Window window{};
};

struct ListInstalledColormapsReply {
  uint16_t sequence{};
  std::vector<ColorMap> cmaps{};
};

using ListInstalledColormapsResponse = Response<ListInstalledColormapsReply>;

struct AllocColorRequest {
  ColorMap cmap{};
  uint16_t red{};
  uint16_t green{};
  uint16_t blue{};
};

struct AllocColorReply {
  uint16_t sequence{};
  uint16_t red{};
  uint16_t green{};
  uint16_t blue{};
  uint32_t pixel{};
};

using AllocColorResponse = Response<AllocColorReply>;

struct AllocNamedColorRequest {
  ColorMap cmap{};
  std::string name{};
};

struct AllocNamedColorReply {
  uint16_t sequence{};
  uint32_t pixel{};
  uint16_t exact_red{};
  uint16_t exact_green{};
  uint16_t exact_blue{};
  uint16_t visual_red{};
  uint16_t visual_green{};
  uint16_t visual_blue{};
};

using AllocNamedColorResponse = Response<AllocNamedColorReply>;

struct AllocColorCellsRequest {
  uint8_t contiguous{};
  ColorMap cmap{};
  uint16_t colors{};
  uint16_t planes{};
};

struct AllocColorCellsReply {
  uint16_t sequence{};
  std::vector<uint32_t> pixels{};
  std::vector<uint32_t> masks{};
};

using AllocColorCellsResponse = Response<AllocColorCellsReply>;

struct AllocColorPlanesRequest {
  uint8_t contiguous{};
  ColorMap cmap{};
  uint16_t colors{};
  uint16_t reds{};
  uint16_t greens{};
  uint16_t blues{};
};

struct AllocColorPlanesReply {
  uint16_t sequence{};
  uint32_t red_mask{};
  uint32_t green_mask{};
  uint32_t blue_mask{};
  std::vector<uint32_t> pixels{};
};

using AllocColorPlanesResponse = Response<AllocColorPlanesReply>;

struct FreeColorsRequest {
  ColorMap cmap{};
  uint32_t plane_mask{};
  std::vector<uint32_t> pixels{};
};

using FreeColorsResponse = Response<void>;

struct StoreColorsRequest {
  ColorMap cmap{};
  std::vector<ColorItem> items{};
};

using StoreColorsResponse = Response<void>;

struct StoreNamedColorRequest {
  ColorFlag flags{};
  ColorMap cmap{};
  uint32_t pixel{};
  std::string name{};
};

using StoreNamedColorResponse = Response<void>;

struct QueryColorsRequest {
  ColorMap cmap{};
  std::vector<uint32_t> pixels{};
};

struct QueryColorsReply {
  uint16_t sequence{};
  std::vector<Rgb> colors{};
};

using QueryColorsResponse = Response<QueryColorsReply>;

struct LookupColorRequest {
  ColorMap cmap{};
  std::string name{};
};

struct LookupColorReply {
  uint16_t sequence{};
  uint16_t exact_red{};
  uint16_t exact_green{};
  uint16_t exact_blue{};
  uint16_t visual_red{};
  uint16_t visual_green{};
  uint16_t visual_blue{};
};

using LookupColorResponse = Response<LookupColorReply>;

struct CreateCursorRequest {
  Cursor cid{};
  Pixmap source{};
  Pixmap mask{};
  uint16_t fore_red{};
  uint16_t fore_green{};
  uint16_t fore_blue{};
  uint16_t back_red{};
  uint16_t back_green{};
  uint16_t back_blue{};
  uint16_t x{};
  uint16_t y{};
};

using CreateCursorResponse = Response<void>;

struct CreateGlyphCursorRequest {
  Cursor cid{};
  Font source_font{};
  Font mask_font{};
  uint16_t source_char{};
  uint16_t mask_char{};
  uint16_t fore_red{};
  uint16_t fore_green{};
  uint16_t fore_blue{};
  uint16_t back_red{};
  uint16_t back_green{};
  uint16_t back_blue{};
};

using CreateGlyphCursorResponse = Response<void>;

struct FreeCursorRequest {
  Cursor cursor{};
};

using FreeCursorResponse = Response<void>;

struct RecolorCursorRequest {
  Cursor cursor{};
  uint16_t fore_red{};
  uint16_t fore_green{};
  uint16_t fore_blue{};
  uint16_t back_red{};
  uint16_t back_green{};
  uint16_t back_blue{};
};

using RecolorCursorResponse = Response<void>;

struct QueryBestSizeRequest {
  QueryShapeOf c_class{};
  Drawable drawable{};
  uint16_t width{};
  uint16_t height{};
};

struct QueryBestSizeReply {
  uint16_t sequence{};
  uint16_t width{};
  uint16_t height{};
};

using QueryBestSizeResponse = Response<QueryBestSizeReply>;

struct QueryExtensionRequest {
  std::string name{};
};

struct QueryExtensionReply {
  uint16_t sequence{};
  uint8_t present{};
  uint8_t major_opcode{};
  uint8_t first_event{};
  uint8_t first_error{};
};

using QueryExtensionResponse = Response<QueryExtensionReply>;

struct ListExtensionsRequest {};

struct ListExtensionsReply {
  uint16_t sequence{};
  std::vector<Str> names{};
};

using ListExtensionsResponse = Response<ListExtensionsReply>;

struct ChangeKeyboardMappingRequest {
  uint8_t keycode_count{};
  KeyCode first_keycode{};
  uint8_t keysyms_per_keycode{};
  std::vector<KeySym> keysyms{};
};

using ChangeKeyboardMappingResponse = Response<void>;

struct GetKeyboardMappingRequest {
  KeyCode first_keycode{};
  uint8_t count{};
};

struct GetKeyboardMappingReply {
  uint8_t keysyms_per_keycode{};
  uint16_t sequence{};
  std::vector<KeySym> keysyms{};
};

using GetKeyboardMappingResponse = Response<GetKeyboardMappingReply>;

struct ChangeKeyboardControlRequest {
  std::optional<int32_t> key_click_percent{};
  std::optional<int32_t> bell_percent{};
  std::optional<int32_t> bell_pitch{};
  std::optional<int32_t> bell_duration{};
  std::optional<uint32_t> led{};
  std::optional<LedMode> led_mode{};
  std::optional<KeyCode32> key{};
  std::optional<AutoRepeatMode> auto_repeat_mode{};
};

using ChangeKeyboardControlResponse = Response<void>;

struct GetKeyboardControlRequest {};

struct GetKeyboardControlReply {
  AutoRepeatMode global_auto_repeat{};
  uint16_t sequence{};
  uint32_t led_mask{};
  uint8_t key_click_percent{};
  uint8_t bell_percent{};
  uint16_t bell_pitch{};
  uint16_t bell_duration{};
  std::array<uint8_t, 32> auto_repeats{};
};

using GetKeyboardControlResponse = Response<GetKeyboardControlReply>;

struct BellRequest {
  int8_t percent{};
};

using BellResponse = Response<void>;

struct ChangePointerControlRequest {
  int16_t acceleration_numerator{};
  int16_t acceleration_denominator{};
  int16_t threshold{};
  uint8_t do_acceleration{};
  uint8_t do_threshold{};
};

using ChangePointerControlResponse = Response<void>;

struct GetPointerControlRequest {};

struct GetPointerControlReply {
  uint16_t sequence{};
  uint16_t acceleration_numerator{};
  uint16_t acceleration_denominator{};
  uint16_t threshold{};
};

using GetPointerControlResponse = Response<GetPointerControlReply>;

struct SetScreenSaverRequest {
  int16_t timeout{};
  int16_t interval{};
  Blanking prefer_blanking{};
  Exposures allow_exposures{};
};

using SetScreenSaverResponse = Response<void>;

struct GetScreenSaverRequest {};

struct GetScreenSaverReply {
  uint16_t sequence{};
  uint16_t timeout{};
  uint16_t interval{};
  Blanking prefer_blanking{};
  Exposures allow_exposures{};
};

using GetScreenSaverResponse = Response<GetScreenSaverReply>;

struct ChangeHostsRequest {
  HostMode mode{};
  Family family{};
  std::vector<uint8_t> address{};
};

using ChangeHostsResponse = Response<void>;

struct ListHostsRequest {};

struct ListHostsReply {
  AccessControl mode{};
  uint16_t sequence{};
  std::vector<Host> hosts{};
};

using ListHostsResponse = Response<ListHostsReply>;

struct SetAccessControlRequest {
  AccessControl mode{};
};

using SetAccessControlResponse = Response<void>;

struct SetCloseDownModeRequest {
  CloseDown mode{};
};

using SetCloseDownModeResponse = Response<void>;

struct KillClientRequest {
  uint32_t resource{};
};

using KillClientResponse = Response<void>;

struct RotatePropertiesRequest {
  Window window{};
  int16_t delta{};
  std::vector<Atom> atoms{};
};

using RotatePropertiesResponse = Response<void>;

struct ForceScreenSaverRequest {
  ScreenSaverMode mode{};
};

using ForceScreenSaverResponse = Response<void>;

struct SetPointerMappingRequest {
  std::vector<uint8_t> map{};
};

struct SetPointerMappingReply {
  MappingStatus status{};
  uint16_t sequence{};
};

using SetPointerMappingResponse = Response<SetPointerMappingReply>;

struct GetPointerMappingRequest {};

struct GetPointerMappingReply {
  uint16_t sequence{};
  std::vector<uint8_t> map{};
};

using GetPointerMappingResponse = Response<GetPointerMappingReply>;

struct SetModifierMappingRequest {
  uint8_t keycodes_per_modifier{};
  std::vector<KeyCode> keycodes{};
};

struct SetModifierMappingReply {
  MappingStatus status{};
  uint16_t sequence{};
};

using SetModifierMappingResponse = Response<SetModifierMappingReply>;

struct GetModifierMappingRequest {};

struct GetModifierMappingReply {
  uint8_t keycodes_per_modifier{};
  uint16_t sequence{};
  std::vector<KeyCode> keycodes{};
};

using GetModifierMappingResponse = Response<GetModifierMappingReply>;

struct NoOperationRequest {};

using NoOperationResponse = Response<void>;

class COMPONENT_EXPORT(X11) XProto {
 public:
  explicit XProto(Connection* connection);

  Connection* connection() const { return connection_; }

  Future<void> CreateWindow(const CreateWindowRequest& request);

  Future<void> CreateWindow(
      const uint8_t& depth = {},
      const Window& wid = {},
      const Window& parent = {},
      const int16_t& x = {},
      const int16_t& y = {},
      const uint16_t& width = {},
      const uint16_t& height = {},
      const uint16_t& border_width = {},
      const WindowClass& c_class = {},
      const VisualId& visual = {},
      const std::optional<Pixmap>& background_pixmap = std::nullopt,
      const std::optional<uint32_t>& background_pixel = std::nullopt,
      const std::optional<Pixmap>& border_pixmap = std::nullopt,
      const std::optional<uint32_t>& border_pixel = std::nullopt,
      const std::optional<Gravity>& bit_gravity = std::nullopt,
      const std::optional<Gravity>& win_gravity = std::nullopt,
      const std::optional<BackingStore>& backing_store = std::nullopt,
      const std::optional<uint32_t>& backing_planes = std::nullopt,
      const std::optional<uint32_t>& backing_pixel = std::nullopt,
      const std::optional<Bool32>& override_redirect = std::nullopt,
      const std::optional<Bool32>& save_under = std::nullopt,
      const std::optional<EventMask>& event_mask = std::nullopt,
      const std::optional<EventMask>& do_not_propogate_mask = std::nullopt,
      const std::optional<ColorMap>& colormap = std::nullopt,
      const std::optional<Cursor>& cursor = std::nullopt);

  Future<void> ChangeWindowAttributes(
      const ChangeWindowAttributesRequest& request);

  Future<void> ChangeWindowAttributes(
      const Window& window = {},
      const std::optional<Pixmap>& background_pixmap = std::nullopt,
      const std::optional<uint32_t>& background_pixel = std::nullopt,
      const std::optional<Pixmap>& border_pixmap = std::nullopt,
      const std::optional<uint32_t>& border_pixel = std::nullopt,
      const std::optional<Gravity>& bit_gravity = std::nullopt,
      const std::optional<Gravity>& win_gravity = std::nullopt,
      const std::optional<BackingStore>& backing_store = std::nullopt,
      const std::optional<uint32_t>& backing_planes = std::nullopt,
      const std::optional<uint32_t>& backing_pixel = std::nullopt,
      const std::optional<Bool32>& override_redirect = std::nullopt,
      const std::optional<Bool32>& save_under = std::nullopt,
      const std::optional<EventMask>& event_mask = std::nullopt,
      const std::optional<EventMask>& do_not_propogate_mask = std::nullopt,
      const std::optional<ColorMap>& colormap = std::nullopt,
      const std::optional<Cursor>& cursor = std::nullopt);

  Future<GetWindowAttributesReply> GetWindowAttributes(
      const GetWindowAttributesRequest& request);

  Future<GetWindowAttributesReply> GetWindowAttributes(
      const Window& window = {});

  Future<void> DestroyWindow(const DestroyWindowRequest& request);

  Future<void> DestroyWindow(const Window& window = {});

  Future<void> DestroySubwindows(const DestroySubwindowsRequest& request);

  Future<void> DestroySubwindows(const Window& window = {});

  Future<void> ChangeSaveSet(const ChangeSaveSetRequest& request);

  Future<void> ChangeSaveSet(const SetMode& mode = {},
                             const Window& window = {});

  Future<void> ReparentWindow(const ReparentWindowRequest& request);

  Future<void> ReparentWindow(const Window& window = {},
                              const Window& parent = {},
                              const int16_t& x = {},
                              const int16_t& y = {});

  Future<void> MapWindow(const MapWindowRequest& request);

  Future<void> MapWindow(const Window& window = {});

  Future<void> MapSubwindows(const MapSubwindowsRequest& request);

  Future<void> MapSubwindows(const Window& window = {});

  Future<void> UnmapWindow(const UnmapWindowRequest& request);

  Future<void> UnmapWindow(const Window& window = {});

  Future<void> UnmapSubwindows(const UnmapSubwindowsRequest& request);

  Future<void> UnmapSubwindows(const Window& window = {});

  Future<void> ConfigureWindow(const ConfigureWindowRequest& request);

  Future<void> ConfigureWindow(
      const Window& window = {},
      const std::optional<int32_t>& x = std::nullopt,
      const std::optional<int32_t>& y = std::nullopt,
      const std::optional<uint32_t>& width = std::nullopt,
      const std::optional<uint32_t>& height = std::nullopt,
      const std::optional<uint32_t>& border_width = std::nullopt,
      const std::optional<Window>& sibling = std::nullopt,
      const std::optional<StackMode>& stack_mode = std::nullopt);

  Future<void> CirculateWindow(const CirculateWindowRequest& request);

  Future<void> CirculateWindow(const Circulate& direction = {},
                               const Window& window = {});

  Future<GetGeometryReply> GetGeometry(const GetGeometryRequest& request);

  Future<GetGeometryReply> GetGeometry(const Drawable& drawable = {});

  Future<QueryTreeReply> QueryTree(const QueryTreeRequest& request);

  Future<QueryTreeReply> QueryTree(const Window& window = {});

  Future<InternAtomReply> InternAtom(const InternAtomRequest& request);

  Future<InternAtomReply> InternAtom(const uint8_t& only_if_exists = {},
                                     const std::string& name = {});

  Future<GetAtomNameReply> GetAtomName(const GetAtomNameRequest& request);

  Future<GetAtomNameReply> GetAtomName(const Atom& atom = {});

  Future<void> ChangeProperty(const ChangePropertyRequest& request);

  Future<void> ChangeProperty(
      const PropMode& mode = {},
      const Window& window = {},
      const Atom& property = {},
      const Atom& type = {},
      const uint8_t& format = {},
      const uint32_t& data_len = {},
      const scoped_refptr<base::RefCountedMemory>& data = {});

  Future<void> DeleteProperty(const DeletePropertyRequest& request);

  Future<void> DeleteProperty(const Window& window = {},
                              const Atom& property = {});

  Future<GetPropertyReply> GetProperty(const GetPropertyRequest& request);

  Future<GetPropertyReply> GetProperty(const uint8_t& c_delete = {},
                                       const Window& window = {},
                                       const Atom& property = {},
                                       const Atom& type = {},
                                       const uint32_t& long_offset = {},
                                       const uint32_t& long_length = {});

  Future<ListPropertiesReply> ListProperties(
      const ListPropertiesRequest& request);

  Future<ListPropertiesReply> ListProperties(const Window& window = {});

  Future<void> SetSelectionOwner(const SetSelectionOwnerRequest& request);

  Future<void> SetSelectionOwner(const Window& owner = {},
                                 const Atom& selection = {},
                                 const Time& time = {});

  Future<GetSelectionOwnerReply> GetSelectionOwner(
      const GetSelectionOwnerRequest& request);

  Future<GetSelectionOwnerReply> GetSelectionOwner(const Atom& selection = {});

  Future<void> ConvertSelection(const ConvertSelectionRequest& request);

  Future<void> ConvertSelection(const Window& requestor = {},
                                const Atom& selection = {},
                                const Atom& target = {},
                                const Atom& property = {},
                                const Time& time = {});

  Future<void> SendEvent(const SendEventRequest& request);

  Future<void> SendEvent(const uint8_t& propagate = {},
                         const Window& destination = {},
                         const EventMask& event_mask = {},
                         const std::array<char, 32>& event = {});

  Future<GrabPointerReply> GrabPointer(const GrabPointerRequest& request);

  Future<GrabPointerReply> GrabPointer(const uint8_t& owner_events = {},
                                       const Window& grab_window = {},
                                       const EventMask& event_mask = {},
                                       const GrabMode& pointer_mode = {},
                                       const GrabMode& keyboard_mode = {},
                                       const Window& confine_to = {},
                                       const Cursor& cursor = {},
                                       const Time& time = {});

  Future<void> UngrabPointer(const UngrabPointerRequest& request);

  Future<void> UngrabPointer(const Time& time = {});

  Future<void> GrabButton(const GrabButtonRequest& request);

  Future<void> GrabButton(const uint8_t& owner_events = {},
                          const Window& grab_window = {},
                          const EventMask& event_mask = {},
                          const GrabMode& pointer_mode = {},
                          const GrabMode& keyboard_mode = {},
                          const Window& confine_to = {},
                          const Cursor& cursor = {},
                          const ButtonIndex& button = {},
                          const ModMask& modifiers = {});

  Future<void> UngrabButton(const UngrabButtonRequest& request);

  Future<void> UngrabButton(const ButtonIndex& button = {},
                            const Window& grab_window = {},
                            const ModMask& modifiers = {});

  Future<void> ChangeActivePointerGrab(
      const ChangeActivePointerGrabRequest& request);

  Future<void> ChangeActivePointerGrab(const Cursor& cursor = {},
                                       const Time& time = {},
                                       const EventMask& event_mask = {});

  Future<GrabKeyboardReply> GrabKeyboard(const GrabKeyboardRequest& request);

  Future<GrabKeyboardReply> GrabKeyboard(const uint8_t& owner_events = {},
                                         const Window& grab_window = {},
                                         const Time& time = {},
                                         const GrabMode& pointer_mode = {},
                                         const GrabMode& keyboard_mode = {});

  Future<void> UngrabKeyboard(const UngrabKeyboardRequest& request);

  Future<void> UngrabKeyboard(const Time& time = {});

  Future<void> GrabKey(const GrabKeyRequest& request);

  Future<void> GrabKey(const uint8_t& owner_events = {},
                       const Window& grab_window = {},
                       const ModMask& modifiers = {},
                       const KeyCode& key = {},
                       const GrabMode& pointer_mode = {},
                       const GrabMode& keyboard_mode = {});

  Future<void> UngrabKey(const UngrabKeyRequest& request);

  Future<void> UngrabKey(const KeyCode& key = {},
                         const Window& grab_window = {},
                         const ModMask& modifiers = {});

  Future<void> AllowEvents(const AllowEventsRequest& request);

  Future<void> AllowEvents(const Allow& mode = {}, const Time& time = {});

  Future<void> GrabServer(const GrabServerRequest& request);

  Future<void> GrabServer();

  Future<void> UngrabServer(const UngrabServerRequest& request);

  Future<void> UngrabServer();

  Future<QueryPointerReply> QueryPointer(const QueryPointerRequest& request);

  Future<QueryPointerReply> QueryPointer(const Window& window = {});

  Future<GetMotionEventsReply> GetMotionEvents(
      const GetMotionEventsRequest& request);

  Future<GetMotionEventsReply> GetMotionEvents(const Window& window = {},
                                               const Time& start = {},
                                               const Time& stop = {});

  Future<TranslateCoordinatesReply> TranslateCoordinates(
      const TranslateCoordinatesRequest& request);

  Future<TranslateCoordinatesReply> TranslateCoordinates(
      const Window& src_window = {},
      const Window& dst_window = {},
      const int16_t& src_x = {},
      const int16_t& src_y = {});

  Future<void> WarpPointer(const WarpPointerRequest& request);

  Future<void> WarpPointer(const Window& src_window = {},
                           const Window& dst_window = {},
                           const int16_t& src_x = {},
                           const int16_t& src_y = {},
                           const uint16_t& src_width = {},
                           const uint16_t& src_height = {},
                           const int16_t& dst_x = {},
                           const int16_t& dst_y = {});

  Future<void> SetInputFocus(const SetInputFocusRequest& request);

  Future<void> SetInputFocus(const InputFocus& revert_to = {},
                             const Window& focus = {},
                             const Time& time = {});

  Future<GetInputFocusReply> GetInputFocus(const GetInputFocusRequest& request);

  Future<GetInputFocusReply> GetInputFocus();

  Future<QueryKeymapReply> QueryKeymap(const QueryKeymapRequest& request);

  Future<QueryKeymapReply> QueryKeymap();

  Future<void> OpenFont(const OpenFontRequest& request);

  Future<void> OpenFont(const Font& fid = {}, const std::string& name = {});

  Future<void> CloseFont(const CloseFontRequest& request);

  Future<void> CloseFont(const Font& font = {});

  Future<QueryFontReply> QueryFont(const QueryFontRequest& request);

  Future<QueryFontReply> QueryFont(const Fontable& font = {});

  Future<QueryTextExtentsReply> QueryTextExtents(
      const QueryTextExtentsRequest& request);

  Future<QueryTextExtentsReply> QueryTextExtents(
      const Fontable& font = {},
      const std::vector<Char16>& string = {});

  Future<ListFontsReply> ListFonts(const ListFontsRequest& request);

  Future<ListFontsReply> ListFonts(const uint16_t& max_names = {},
                                   const std::string& pattern = {});

  Future<ListFontsWithInfoReply> ListFontsWithInfo(
      const ListFontsWithInfoRequest& request);

  Future<ListFontsWithInfoReply> ListFontsWithInfo(
      const uint16_t& max_names = {},
      const std::string& pattern = {});

  Future<void> SetFontPath(const SetFontPathRequest& request);

  Future<void> SetFontPath(const std::vector<Str>& font = {});

  Future<GetFontPathReply> GetFontPath(const GetFontPathRequest& request);

  Future<GetFontPathReply> GetFontPath();

  Future<void> CreatePixmap(const CreatePixmapRequest& request);

  Future<void> CreatePixmap(const uint8_t& depth = {},
                            const Pixmap& pid = {},
                            const Drawable& drawable = {},
                            const uint16_t& width = {},
                            const uint16_t& height = {});

  Future<void> FreePixmap(const FreePixmapRequest& request);

  Future<void> FreePixmap(const Pixmap& pixmap = {});

  Future<void> CreateGC(const CreateGCRequest& request);

  Future<void> CreateGC(
      const GraphicsContext& cid = {},
      const Drawable& drawable = {},
      const std::optional<Gx>& function = std::nullopt,
      const std::optional<uint32_t>& plane_mask = std::nullopt,
      const std::optional<uint32_t>& foreground = std::nullopt,
      const std::optional<uint32_t>& background = std::nullopt,
      const std::optional<uint32_t>& line_width = std::nullopt,
      const std::optional<LineStyle>& line_style = std::nullopt,
      const std::optional<CapStyle>& cap_style = std::nullopt,
      const std::optional<JoinStyle>& join_style = std::nullopt,
      const std::optional<FillStyle>& fill_style = std::nullopt,
      const std::optional<FillRule>& fill_rule = std::nullopt,
      const std::optional<Pixmap>& tile = std::nullopt,
      const std::optional<Pixmap>& stipple = std::nullopt,
      const std::optional<int32_t>& tile_stipple_x_origin = std::nullopt,
      const std::optional<int32_t>& tile_stipple_y_origin = std::nullopt,
      const std::optional<Font>& font = std::nullopt,
      const std::optional<SubwindowMode>& subwindow_mode = std::nullopt,
      const std::optional<Bool32>& graphics_exposures = std::nullopt,
      const std::optional<int32_t>& clip_x_origin = std::nullopt,
      const std::optional<int32_t>& clip_y_origin = std::nullopt,
      const std::optional<Pixmap>& clip_mask = std::nullopt,
      const std::optional<uint32_t>& dash_offset = std::nullopt,
      const std::optional<uint32_t>& dashes = std::nullopt,
      const std::optional<ArcMode>& arc_mode = std::nullopt);

  Future<void> ChangeGC(const ChangeGCRequest& request);

  Future<void> ChangeGC(
      const GraphicsContext& gc = {},
      const std::optional<Gx>& function = std::nullopt,
      const std::optional<uint32_t>& plane_mask = std::nullopt,
      const std::optional<uint32_t>& foreground = std::nullopt,
      const std::optional<uint32_t>& background = std::nullopt,
      const std::optional<uint32_t>& line_width = std::nullopt,
      const std::optional<LineStyle>& line_style = std::nullopt,
      const std::optional<CapStyle>& cap_style = std::nullopt,
      const std::optional<JoinStyle>& join_style = std::nullopt,
      const std::optional<FillStyle>& fill_style = std::nullopt,
      const std::optional<FillRule>& fill_rule = std::nullopt,
      const std::optional<Pixmap>& tile = std::nullopt,
      const std::optional<Pixmap>& stipple = std::nullopt,
      const std::optional<int32_t>& tile_stipple_x_origin = std::nullopt,
      const std::optional<int32_t>& tile_stipple_y_origin = std::nullopt,
      const std::optional<Font>& font = std::nullopt,
      const std::optional<SubwindowMode>& subwindow_mode = std::nullopt,
      const std::optional<Bool32>& graphics_exposures = std::nullopt,
      const std::optional<int32_t>& clip_x_origin = std::nullopt,
      const std::optional<int32_t>& clip_y_origin = std::nullopt,
      const std::optional<Pixmap>& clip_mask = std::nullopt,
      const std::optional<uint32_t>& dash_offset = std::nullopt,
      const std::optional<uint32_t>& dashes = std::nullopt,
      const std::optional<ArcMode>& arc_mode = std::nullopt);

  Future<void> CopyGC(const CopyGCRequest& request);

  Future<void> CopyGC(const GraphicsContext& src_gc = {},
                      const GraphicsContext& dst_gc = {},
                      const GraphicsContextAttribute& value_mask = {});

  Future<void> SetDashes(const SetDashesRequest& request);

  Future<void> SetDashes(const GraphicsContext& gc = {},
                         const uint16_t& dash_offset = {},
                         const std::vector<uint8_t>& dashes = {});

  Future<void> SetClipRectangles(const SetClipRectanglesRequest& request);

  Future<void> SetClipRectangles(const ClipOrdering& ordering = {},
                                 const GraphicsContext& gc = {},
                                 const int16_t& clip_x_origin = {},
                                 const int16_t& clip_y_origin = {},
                                 const std::vector<Rectangle>& rectangles = {});

  Future<void> FreeGC(const FreeGCRequest& request);

  Future<void> FreeGC(const GraphicsContext& gc = {});

  Future<void> ClearArea(const ClearAreaRequest& request);

  Future<void> ClearArea(const uint8_t& exposures = {},
                         const Window& window = {},
                         const int16_t& x = {},
                         const int16_t& y = {},
                         const uint16_t& width = {},
                         const uint16_t& height = {});

  Future<void> CopyArea(const CopyAreaRequest& request);

  Future<void> CopyArea(const Drawable& src_drawable = {},
                        const Drawable& dst_drawable = {},
                        const GraphicsContext& gc = {},
                        const int16_t& src_x = {},
                        const int16_t& src_y = {},
                        const int16_t& dst_x = {},
                        const int16_t& dst_y = {},
                        const uint16_t& width = {},
                        const uint16_t& height = {});

  Future<void> CopyPlane(const CopyPlaneRequest& request);

  Future<void> CopyPlane(const Drawable& src_drawable = {},
                         const Drawable& dst_drawable = {},
                         const GraphicsContext& gc = {},
                         const int16_t& src_x = {},
                         const int16_t& src_y = {},
                         const int16_t& dst_x = {},
                         const int16_t& dst_y = {},
                         const uint16_t& width = {},
                         const uint16_t& height = {},
                         const uint32_t& bit_plane = {});

  Future<void> PolyPoint(const PolyPointRequest& request);

  Future<void> PolyPoint(const CoordMode& coordinate_mode = {},
                         const Drawable& drawable = {},
                         const GraphicsContext& gc = {},
                         const std::vector<Point>& points = {});

  Future<void> PolyLine(const PolyLineRequest& request);

  Future<void> PolyLine(const CoordMode& coordinate_mode = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const std::vector<Point>& points = {});

  Future<void> PolySegment(const PolySegmentRequest& request);

  Future<void> PolySegment(const Drawable& drawable = {},
                           const GraphicsContext& gc = {},
                           const std::vector<Segment>& segments = {});

  Future<void> PolyRectangle(const PolyRectangleRequest& request);

  Future<void> PolyRectangle(const Drawable& drawable = {},
                             const GraphicsContext& gc = {},
                             const std::vector<Rectangle>& rectangles = {});

  Future<void> PolyArc(const PolyArcRequest& request);

  Future<void> PolyArc(const Drawable& drawable = {},
                       const GraphicsContext& gc = {},
                       const std::vector<Arc>& arcs = {});

  Future<void> FillPoly(const FillPolyRequest& request);

  Future<void> FillPoly(const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const PolyShape& shape = {},
                        const CoordMode& coordinate_mode = {},
                        const std::vector<Point>& points = {});

  Future<void> PolyFillRectangle(const PolyFillRectangleRequest& request);

  Future<void> PolyFillRectangle(const Drawable& drawable = {},
                                 const GraphicsContext& gc = {},
                                 const std::vector<Rectangle>& rectangles = {});

  Future<void> PolyFillArc(const PolyFillArcRequest& request);

  Future<void> PolyFillArc(const Drawable& drawable = {},
                           const GraphicsContext& gc = {},
                           const std::vector<Arc>& arcs = {});

  Future<void> PutImage(const PutImageRequest& request);

  Future<void> PutImage(const ImageFormat& format = {},
                        const Drawable& drawable = {},
                        const GraphicsContext& gc = {},
                        const uint16_t& width = {},
                        const uint16_t& height = {},
                        const int16_t& dst_x = {},
                        const int16_t& dst_y = {},
                        const uint8_t& left_pad = {},
                        const uint8_t& depth = {},
                        const scoped_refptr<base::RefCountedMemory>& data = {});

  Future<GetImageReply> GetImage(const GetImageRequest& request);

  Future<GetImageReply> GetImage(const ImageFormat& format = {},
                                 const Drawable& drawable = {},
                                 const int16_t& x = {},
                                 const int16_t& y = {},
                                 const uint16_t& width = {},
                                 const uint16_t& height = {},
                                 const uint32_t& plane_mask = {});

  Future<void> PolyText8(const PolyText8Request& request);

  Future<void> PolyText8(const Drawable& drawable = {},
                         const GraphicsContext& gc = {},
                         const int16_t& x = {},
                         const int16_t& y = {},
                         const std::vector<uint8_t>& items = {});

  Future<void> PolyText16(const PolyText16Request& request);

  Future<void> PolyText16(const Drawable& drawable = {},
                          const GraphicsContext& gc = {},
                          const int16_t& x = {},
                          const int16_t& y = {},
                          const std::vector<uint8_t>& items = {});

  Future<void> ImageText8(const ImageText8Request& request);

  Future<void> ImageText8(const Drawable& drawable = {},
                          const GraphicsContext& gc = {},
                          const int16_t& x = {},
                          const int16_t& y = {},
                          const std::string& string = {});

  Future<void> ImageText16(const ImageText16Request& request);

  Future<void> ImageText16(const Drawable& drawable = {},
                           const GraphicsContext& gc = {},
                           const int16_t& x = {},
                           const int16_t& y = {},
                           const std::vector<Char16>& string = {});

  Future<void> CreateColormap(const CreateColormapRequest& request);

  Future<void> CreateColormap(const ColormapAlloc& alloc = {},
                              const ColorMap& mid = {},
                              const Window& window = {},
                              const VisualId& visual = {});

  Future<void> FreeColormap(const FreeColormapRequest& request);

  Future<void> FreeColormap(const ColorMap& cmap = {});

  Future<void> CopyColormapAndFree(const CopyColormapAndFreeRequest& request);

  Future<void> CopyColormapAndFree(const ColorMap& mid = {},
                                   const ColorMap& src_cmap = {});

  Future<void> InstallColormap(const InstallColormapRequest& request);

  Future<void> InstallColormap(const ColorMap& cmap = {});

  Future<void> UninstallColormap(const UninstallColormapRequest& request);

  Future<void> UninstallColormap(const ColorMap& cmap = {});

  Future<ListInstalledColormapsReply> ListInstalledColormaps(
      const ListInstalledColormapsRequest& request);

  Future<ListInstalledColormapsReply> ListInstalledColormaps(
      const Window& window = {});

  Future<AllocColorReply> AllocColor(const AllocColorRequest& request);

  Future<AllocColorReply> AllocColor(const ColorMap& cmap = {},
                                     const uint16_t& red = {},
                                     const uint16_t& green = {},
                                     const uint16_t& blue = {});

  Future<AllocNamedColorReply> AllocNamedColor(
      const AllocNamedColorRequest& request);

  Future<AllocNamedColorReply> AllocNamedColor(const ColorMap& cmap = {},
                                               const std::string& name = {});

  Future<AllocColorCellsReply> AllocColorCells(
      const AllocColorCellsRequest& request);

  Future<AllocColorCellsReply> AllocColorCells(const uint8_t& contiguous = {},
                                               const ColorMap& cmap = {},
                                               const uint16_t& colors = {},
                                               const uint16_t& planes = {});

  Future<AllocColorPlanesReply> AllocColorPlanes(
      const AllocColorPlanesRequest& request);

  Future<AllocColorPlanesReply> AllocColorPlanes(const uint8_t& contiguous = {},
                                                 const ColorMap& cmap = {},
                                                 const uint16_t& colors = {},
                                                 const uint16_t& reds = {},
                                                 const uint16_t& greens = {},
                                                 const uint16_t& blues = {});

  Future<void> FreeColors(const FreeColorsRequest& request);

  Future<void> FreeColors(const ColorMap& cmap = {},
                          const uint32_t& plane_mask = {},
                          const std::vector<uint32_t>& pixels = {});

  Future<void> StoreColors(const StoreColorsRequest& request);

  Future<void> StoreColors(const ColorMap& cmap = {},
                           const std::vector<ColorItem>& items = {});

  Future<void> StoreNamedColor(const StoreNamedColorRequest& request);

  Future<void> StoreNamedColor(const ColorFlag& flags = {},
                               const ColorMap& cmap = {},
                               const uint32_t& pixel = {},
                               const std::string& name = {});

  Future<QueryColorsReply> QueryColors(const QueryColorsRequest& request);

  Future<QueryColorsReply> QueryColors(
      const ColorMap& cmap = {},
      const std::vector<uint32_t>& pixels = {});

  Future<LookupColorReply> LookupColor(const LookupColorRequest& request);

  Future<LookupColorReply> LookupColor(const ColorMap& cmap = {},
                                       const std::string& name = {});

  Future<void> CreateCursor(const CreateCursorRequest& request);

  Future<void> CreateCursor(const Cursor& cid = {},
                            const Pixmap& source = {},
                            const Pixmap& mask = {},
                            const uint16_t& fore_red = {},
                            const uint16_t& fore_green = {},
                            const uint16_t& fore_blue = {},
                            const uint16_t& back_red = {},
                            const uint16_t& back_green = {},
                            const uint16_t& back_blue = {},
                            const uint16_t& x = {},
                            const uint16_t& y = {});

  Future<void> CreateGlyphCursor(const CreateGlyphCursorRequest& request);

  Future<void> CreateGlyphCursor(const Cursor& cid = {},
                                 const Font& source_font = {},
                                 const Font& mask_font = {},
                                 const uint16_t& source_char = {},
                                 const uint16_t& mask_char = {},
                                 const uint16_t& fore_red = {},
                                 const uint16_t& fore_green = {},
                                 const uint16_t& fore_blue = {},
                                 const uint16_t& back_red = {},
                                 const uint16_t& back_green = {},
                                 const uint16_t& back_blue = {});

  Future<void> FreeCursor(const FreeCursorRequest& request);

  Future<void> FreeCursor(const Cursor& cursor = {});

  Future<void> RecolorCursor(const RecolorCursorRequest& request);

  Future<void> RecolorCursor(const Cursor& cursor = {},
                             const uint16_t& fore_red = {},
                             const uint16_t& fore_green = {},
                             const uint16_t& fore_blue = {},
                             const uint16_t& back_red = {},
                             const uint16_t& back_green = {},
                             const uint16_t& back_blue = {});

  Future<QueryBestSizeReply> QueryBestSize(const QueryBestSizeRequest& request);

  Future<QueryBestSizeReply> QueryBestSize(const QueryShapeOf& c_class = {},
                                           const Drawable& drawable = {},
                                           const uint16_t& width = {},
                                           const uint16_t& height = {});

  Future<QueryExtensionReply> QueryExtension(
      const QueryExtensionRequest& request);

  Future<QueryExtensionReply> QueryExtension(const std::string& name = {});

  Future<ListExtensionsReply> ListExtensions(
      const ListExtensionsRequest& request);

  Future<ListExtensionsReply> ListExtensions();

  Future<void> ChangeKeyboardMapping(
      const ChangeKeyboardMappingRequest& request);

  Future<void> ChangeKeyboardMapping(const uint8_t& keycode_count = {},
                                     const KeyCode& first_keycode = {},
                                     const uint8_t& keysyms_per_keycode = {},
                                     const std::vector<KeySym>& keysyms = {});

  Future<GetKeyboardMappingReply> GetKeyboardMapping(
      const GetKeyboardMappingRequest& request);

  Future<GetKeyboardMappingReply> GetKeyboardMapping(
      const KeyCode& first_keycode = {},
      const uint8_t& count = {});

  Future<void> ChangeKeyboardControl(
      const ChangeKeyboardControlRequest& request);

  Future<void> ChangeKeyboardControl(
      const std::optional<int32_t>& key_click_percent = std::nullopt,
      const std::optional<int32_t>& bell_percent = std::nullopt,
      const std::optional<int32_t>& bell_pitch = std::nullopt,
      const std::optional<int32_t>& bell_duration = std::nullopt,
      const std::optional<uint32_t>& led = std::nullopt,
      const std::optional<LedMode>& led_mode = std::nullopt,
      const std::optional<KeyCode32>& key = std::nullopt,
      const std::optional<AutoRepeatMode>& auto_repeat_mode = std::nullopt);

  Future<GetKeyboardControlReply> GetKeyboardControl(
      const GetKeyboardControlRequest& request);

  Future<GetKeyboardControlReply> GetKeyboardControl();

  Future<void> Bell(const BellRequest& request);

  Future<void> Bell(const int8_t& percent = {});

  Future<void> ChangePointerControl(const ChangePointerControlRequest& request);

  Future<void> ChangePointerControl(
      const int16_t& acceleration_numerator = {},
      const int16_t& acceleration_denominator = {},
      const int16_t& threshold = {},
      const uint8_t& do_acceleration = {},
      const uint8_t& do_threshold = {});

  Future<GetPointerControlReply> GetPointerControl(
      const GetPointerControlRequest& request);

  Future<GetPointerControlReply> GetPointerControl();

  Future<void> SetScreenSaver(const SetScreenSaverRequest& request);

  Future<void> SetScreenSaver(const int16_t& timeout = {},
                              const int16_t& interval = {},
                              const Blanking& prefer_blanking = {},
                              const Exposures& allow_exposures = {});

  Future<GetScreenSaverReply> GetScreenSaver(
      const GetScreenSaverRequest& request);

  Future<GetScreenSaverReply> GetScreenSaver();

  Future<void> ChangeHosts(const ChangeHostsRequest& request);

  Future<void> ChangeHosts(const HostMode& mode = {},
                           const Family& family = {},
                           const std::vector<uint8_t>& address = {});

  Future<ListHostsReply> ListHosts(const ListHostsRequest& request);

  Future<ListHostsReply> ListHosts();

  Future<void> SetAccessControl(const SetAccessControlRequest& request);

  Future<void> SetAccessControl(const AccessControl& mode = {});

  Future<void> SetCloseDownMode(const SetCloseDownModeRequest& request);

  Future<void> SetCloseDownMode(const CloseDown& mode = {});

  Future<void> KillClient(const KillClientRequest& request);

  Future<void> KillClient(const uint32_t& resource = {});

  Future<void> RotateProperties(const RotatePropertiesRequest& request);

  Future<void> RotateProperties(const Window& window = {},
                                const int16_t& delta = {},
                                const std::vector<Atom>& atoms = {});

  Future<void> ForceScreenSaver(const ForceScreenSaverRequest& request);

  Future<void> ForceScreenSaver(const ScreenSaverMode& mode = {});

  Future<SetPointerMappingReply> SetPointerMapping(
      const SetPointerMappingRequest& request);

  Future<SetPointerMappingReply> SetPointerMapping(
      const std::vector<uint8_t>& map = {});

  Future<GetPointerMappingReply> GetPointerMapping(
      const GetPointerMappingRequest& request);

  Future<GetPointerMappingReply> GetPointerMapping();

  Future<SetModifierMappingReply> SetModifierMapping(
      const SetModifierMappingRequest& request);

  Future<SetModifierMappingReply> SetModifierMapping(
      const uint8_t& keycodes_per_modifier = {},
      const std::vector<KeyCode>& keycodes = {});

  Future<GetModifierMappingReply> GetModifierMapping(
      const GetModifierMappingRequest& request);

  Future<GetModifierMappingReply> GetModifierMapping();

  Future<void> NoOperation(const NoOperationRequest& request);

  Future<void> NoOperation();

 private:
  Connection* const connection_;
};

}  // namespace x11

inline constexpr x11::VisualClass operator|(x11::VisualClass l,
                                            x11::VisualClass r) {
  using T = std::underlying_type_t<x11::VisualClass>;
  return static_cast<x11::VisualClass>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::VisualClass operator&(x11::VisualClass l,
                                            x11::VisualClass r) {
  using T = std::underlying_type_t<x11::VisualClass>;
  return static_cast<x11::VisualClass>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::EventMask operator|(x11::EventMask l, x11::EventMask r) {
  using T = std::underlying_type_t<x11::EventMask>;
  return static_cast<x11::EventMask>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::EventMask operator&(x11::EventMask l, x11::EventMask r) {
  using T = std::underlying_type_t<x11::EventMask>;
  return static_cast<x11::EventMask>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::BackingStore operator|(x11::BackingStore l,
                                             x11::BackingStore r) {
  using T = std::underlying_type_t<x11::BackingStore>;
  return static_cast<x11::BackingStore>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::BackingStore operator&(x11::BackingStore l,
                                             x11::BackingStore r) {
  using T = std::underlying_type_t<x11::BackingStore>;
  return static_cast<x11::BackingStore>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ImageOrder operator|(x11::ImageOrder l,
                                           x11::ImageOrder r) {
  using T = std::underlying_type_t<x11::ImageOrder>;
  return static_cast<x11::ImageOrder>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ImageOrder operator&(x11::ImageOrder l,
                                           x11::ImageOrder r) {
  using T = std::underlying_type_t<x11::ImageOrder>;
  return static_cast<x11::ImageOrder>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ModMask operator|(x11::ModMask l, x11::ModMask r) {
  using T = std::underlying_type_t<x11::ModMask>;
  return static_cast<x11::ModMask>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ModMask operator&(x11::ModMask l, x11::ModMask r) {
  using T = std::underlying_type_t<x11::ModMask>;
  return static_cast<x11::ModMask>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::KeyButMask operator|(x11::KeyButMask l,
                                           x11::KeyButMask r) {
  using T = std::underlying_type_t<x11::KeyButMask>;
  return static_cast<x11::KeyButMask>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::KeyButMask operator&(x11::KeyButMask l,
                                           x11::KeyButMask r) {
  using T = std::underlying_type_t<x11::KeyButMask>;
  return static_cast<x11::KeyButMask>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Window operator|(x11::Window l, x11::Window r) {
  using T = std::underlying_type_t<x11::Window>;
  return static_cast<x11::Window>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Window operator&(x11::Window l, x11::Window r) {
  using T = std::underlying_type_t<x11::Window>;
  return static_cast<x11::Window>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ButtonMask operator|(x11::ButtonMask l,
                                           x11::ButtonMask r) {
  using T = std::underlying_type_t<x11::ButtonMask>;
  return static_cast<x11::ButtonMask>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ButtonMask operator&(x11::ButtonMask l,
                                           x11::ButtonMask r) {
  using T = std::underlying_type_t<x11::ButtonMask>;
  return static_cast<x11::ButtonMask>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Motion operator|(x11::Motion l, x11::Motion r) {
  using T = std::underlying_type_t<x11::Motion>;
  return static_cast<x11::Motion>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Motion operator&(x11::Motion l, x11::Motion r) {
  using T = std::underlying_type_t<x11::Motion>;
  return static_cast<x11::Motion>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::NotifyDetail operator|(x11::NotifyDetail l,
                                             x11::NotifyDetail r) {
  using T = std::underlying_type_t<x11::NotifyDetail>;
  return static_cast<x11::NotifyDetail>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::NotifyDetail operator&(x11::NotifyDetail l,
                                             x11::NotifyDetail r) {
  using T = std::underlying_type_t<x11::NotifyDetail>;
  return static_cast<x11::NotifyDetail>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::NotifyMode operator|(x11::NotifyMode l,
                                           x11::NotifyMode r) {
  using T = std::underlying_type_t<x11::NotifyMode>;
  return static_cast<x11::NotifyMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::NotifyMode operator&(x11::NotifyMode l,
                                           x11::NotifyMode r) {
  using T = std::underlying_type_t<x11::NotifyMode>;
  return static_cast<x11::NotifyMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Visibility operator|(x11::Visibility l,
                                           x11::Visibility r) {
  using T = std::underlying_type_t<x11::Visibility>;
  return static_cast<x11::Visibility>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Visibility operator&(x11::Visibility l,
                                           x11::Visibility r) {
  using T = std::underlying_type_t<x11::Visibility>;
  return static_cast<x11::Visibility>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Place operator|(x11::Place l, x11::Place r) {
  using T = std::underlying_type_t<x11::Place>;
  return static_cast<x11::Place>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Place operator&(x11::Place l, x11::Place r) {
  using T = std::underlying_type_t<x11::Place>;
  return static_cast<x11::Place>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Property operator|(x11::Property l, x11::Property r) {
  using T = std::underlying_type_t<x11::Property>;
  return static_cast<x11::Property>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Property operator&(x11::Property l, x11::Property r) {
  using T = std::underlying_type_t<x11::Property>;
  return static_cast<x11::Property>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Time operator|(x11::Time l, x11::Time r) {
  using T = std::underlying_type_t<x11::Time>;
  return static_cast<x11::Time>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Time operator&(x11::Time l, x11::Time r) {
  using T = std::underlying_type_t<x11::Time>;
  return static_cast<x11::Time>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Atom operator|(x11::Atom l, x11::Atom r) {
  using T = std::underlying_type_t<x11::Atom>;
  return static_cast<x11::Atom>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Atom operator&(x11::Atom l, x11::Atom r) {
  using T = std::underlying_type_t<x11::Atom>;
  return static_cast<x11::Atom>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ColormapState operator|(x11::ColormapState l,
                                              x11::ColormapState r) {
  using T = std::underlying_type_t<x11::ColormapState>;
  return static_cast<x11::ColormapState>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ColormapState operator&(x11::ColormapState l,
                                              x11::ColormapState r) {
  using T = std::underlying_type_t<x11::ColormapState>;
  return static_cast<x11::ColormapState>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Colormap operator|(x11::Colormap l, x11::Colormap r) {
  using T = std::underlying_type_t<x11::Colormap>;
  return static_cast<x11::Colormap>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Colormap operator&(x11::Colormap l, x11::Colormap r) {
  using T = std::underlying_type_t<x11::Colormap>;
  return static_cast<x11::Colormap>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Mapping operator|(x11::Mapping l, x11::Mapping r) {
  using T = std::underlying_type_t<x11::Mapping>;
  return static_cast<x11::Mapping>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Mapping operator&(x11::Mapping l, x11::Mapping r) {
  using T = std::underlying_type_t<x11::Mapping>;
  return static_cast<x11::Mapping>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::WindowClass operator|(x11::WindowClass l,
                                            x11::WindowClass r) {
  using T = std::underlying_type_t<x11::WindowClass>;
  return static_cast<x11::WindowClass>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::WindowClass operator&(x11::WindowClass l,
                                            x11::WindowClass r) {
  using T = std::underlying_type_t<x11::WindowClass>;
  return static_cast<x11::WindowClass>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::CreateWindowAttribute operator|(
    x11::CreateWindowAttribute l,
    x11::CreateWindowAttribute r) {
  using T = std::underlying_type_t<x11::CreateWindowAttribute>;
  return static_cast<x11::CreateWindowAttribute>(static_cast<T>(l) |
                                                 static_cast<T>(r));
}

inline constexpr x11::CreateWindowAttribute operator&(
    x11::CreateWindowAttribute l,
    x11::CreateWindowAttribute r) {
  using T = std::underlying_type_t<x11::CreateWindowAttribute>;
  return static_cast<x11::CreateWindowAttribute>(static_cast<T>(l) &
                                                 static_cast<T>(r));
}

inline constexpr x11::BackPixmap operator|(x11::BackPixmap l,
                                           x11::BackPixmap r) {
  using T = std::underlying_type_t<x11::BackPixmap>;
  return static_cast<x11::BackPixmap>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::BackPixmap operator&(x11::BackPixmap l,
                                           x11::BackPixmap r) {
  using T = std::underlying_type_t<x11::BackPixmap>;
  return static_cast<x11::BackPixmap>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Gravity operator|(x11::Gravity l, x11::Gravity r) {
  using T = std::underlying_type_t<x11::Gravity>;
  return static_cast<x11::Gravity>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Gravity operator&(x11::Gravity l, x11::Gravity r) {
  using T = std::underlying_type_t<x11::Gravity>;
  return static_cast<x11::Gravity>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::MapState operator|(x11::MapState l, x11::MapState r) {
  using T = std::underlying_type_t<x11::MapState>;
  return static_cast<x11::MapState>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::MapState operator&(x11::MapState l, x11::MapState r) {
  using T = std::underlying_type_t<x11::MapState>;
  return static_cast<x11::MapState>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::SetMode operator|(x11::SetMode l, x11::SetMode r) {
  using T = std::underlying_type_t<x11::SetMode>;
  return static_cast<x11::SetMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::SetMode operator&(x11::SetMode l, x11::SetMode r) {
  using T = std::underlying_type_t<x11::SetMode>;
  return static_cast<x11::SetMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ConfigWindow operator|(x11::ConfigWindow l,
                                             x11::ConfigWindow r) {
  using T = std::underlying_type_t<x11::ConfigWindow>;
  return static_cast<x11::ConfigWindow>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ConfigWindow operator&(x11::ConfigWindow l,
                                             x11::ConfigWindow r) {
  using T = std::underlying_type_t<x11::ConfigWindow>;
  return static_cast<x11::ConfigWindow>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::StackMode operator|(x11::StackMode l, x11::StackMode r) {
  using T = std::underlying_type_t<x11::StackMode>;
  return static_cast<x11::StackMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::StackMode operator&(x11::StackMode l, x11::StackMode r) {
  using T = std::underlying_type_t<x11::StackMode>;
  return static_cast<x11::StackMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Circulate operator|(x11::Circulate l, x11::Circulate r) {
  using T = std::underlying_type_t<x11::Circulate>;
  return static_cast<x11::Circulate>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Circulate operator&(x11::Circulate l, x11::Circulate r) {
  using T = std::underlying_type_t<x11::Circulate>;
  return static_cast<x11::Circulate>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::PropMode operator|(x11::PropMode l, x11::PropMode r) {
  using T = std::underlying_type_t<x11::PropMode>;
  return static_cast<x11::PropMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::PropMode operator&(x11::PropMode l, x11::PropMode r) {
  using T = std::underlying_type_t<x11::PropMode>;
  return static_cast<x11::PropMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::GetPropertyType operator|(x11::GetPropertyType l,
                                                x11::GetPropertyType r) {
  using T = std::underlying_type_t<x11::GetPropertyType>;
  return static_cast<x11::GetPropertyType>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::GetPropertyType operator&(x11::GetPropertyType l,
                                                x11::GetPropertyType r) {
  using T = std::underlying_type_t<x11::GetPropertyType>;
  return static_cast<x11::GetPropertyType>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::SendEventDest operator|(x11::SendEventDest l,
                                              x11::SendEventDest r) {
  using T = std::underlying_type_t<x11::SendEventDest>;
  return static_cast<x11::SendEventDest>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::SendEventDest operator&(x11::SendEventDest l,
                                              x11::SendEventDest r) {
  using T = std::underlying_type_t<x11::SendEventDest>;
  return static_cast<x11::SendEventDest>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::GrabMode operator|(x11::GrabMode l, x11::GrabMode r) {
  using T = std::underlying_type_t<x11::GrabMode>;
  return static_cast<x11::GrabMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::GrabMode operator&(x11::GrabMode l, x11::GrabMode r) {
  using T = std::underlying_type_t<x11::GrabMode>;
  return static_cast<x11::GrabMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::GrabStatus operator|(x11::GrabStatus l,
                                           x11::GrabStatus r) {
  using T = std::underlying_type_t<x11::GrabStatus>;
  return static_cast<x11::GrabStatus>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::GrabStatus operator&(x11::GrabStatus l,
                                           x11::GrabStatus r) {
  using T = std::underlying_type_t<x11::GrabStatus>;
  return static_cast<x11::GrabStatus>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Cursor operator|(x11::Cursor l, x11::Cursor r) {
  using T = std::underlying_type_t<x11::Cursor>;
  return static_cast<x11::Cursor>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Cursor operator&(x11::Cursor l, x11::Cursor r) {
  using T = std::underlying_type_t<x11::Cursor>;
  return static_cast<x11::Cursor>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ButtonIndex operator|(x11::ButtonIndex l,
                                            x11::ButtonIndex r) {
  using T = std::underlying_type_t<x11::ButtonIndex>;
  return static_cast<x11::ButtonIndex>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ButtonIndex operator&(x11::ButtonIndex l,
                                            x11::ButtonIndex r) {
  using T = std::underlying_type_t<x11::ButtonIndex>;
  return static_cast<x11::ButtonIndex>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Grab operator|(x11::Grab l, x11::Grab r) {
  using T = std::underlying_type_t<x11::Grab>;
  return static_cast<x11::Grab>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Grab operator&(x11::Grab l, x11::Grab r) {
  using T = std::underlying_type_t<x11::Grab>;
  return static_cast<x11::Grab>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Allow operator|(x11::Allow l, x11::Allow r) {
  using T = std::underlying_type_t<x11::Allow>;
  return static_cast<x11::Allow>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Allow operator&(x11::Allow l, x11::Allow r) {
  using T = std::underlying_type_t<x11::Allow>;
  return static_cast<x11::Allow>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::InputFocus operator|(x11::InputFocus l,
                                           x11::InputFocus r) {
  using T = std::underlying_type_t<x11::InputFocus>;
  return static_cast<x11::InputFocus>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::InputFocus operator&(x11::InputFocus l,
                                           x11::InputFocus r) {
  using T = std::underlying_type_t<x11::InputFocus>;
  return static_cast<x11::InputFocus>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::FontDraw operator|(x11::FontDraw l, x11::FontDraw r) {
  using T = std::underlying_type_t<x11::FontDraw>;
  return static_cast<x11::FontDraw>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::FontDraw operator&(x11::FontDraw l, x11::FontDraw r) {
  using T = std::underlying_type_t<x11::FontDraw>;
  return static_cast<x11::FontDraw>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::GraphicsContextAttribute operator|(
    x11::GraphicsContextAttribute l,
    x11::GraphicsContextAttribute r) {
  using T = std::underlying_type_t<x11::GraphicsContextAttribute>;
  return static_cast<x11::GraphicsContextAttribute>(static_cast<T>(l) |
                                                    static_cast<T>(r));
}

inline constexpr x11::GraphicsContextAttribute operator&(
    x11::GraphicsContextAttribute l,
    x11::GraphicsContextAttribute r) {
  using T = std::underlying_type_t<x11::GraphicsContextAttribute>;
  return static_cast<x11::GraphicsContextAttribute>(static_cast<T>(l) &
                                                    static_cast<T>(r));
}

inline constexpr x11::Gx operator|(x11::Gx l, x11::Gx r) {
  using T = std::underlying_type_t<x11::Gx>;
  return static_cast<x11::Gx>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Gx operator&(x11::Gx l, x11::Gx r) {
  using T = std::underlying_type_t<x11::Gx>;
  return static_cast<x11::Gx>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::LineStyle operator|(x11::LineStyle l, x11::LineStyle r) {
  using T = std::underlying_type_t<x11::LineStyle>;
  return static_cast<x11::LineStyle>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::LineStyle operator&(x11::LineStyle l, x11::LineStyle r) {
  using T = std::underlying_type_t<x11::LineStyle>;
  return static_cast<x11::LineStyle>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::CapStyle operator|(x11::CapStyle l, x11::CapStyle r) {
  using T = std::underlying_type_t<x11::CapStyle>;
  return static_cast<x11::CapStyle>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::CapStyle operator&(x11::CapStyle l, x11::CapStyle r) {
  using T = std::underlying_type_t<x11::CapStyle>;
  return static_cast<x11::CapStyle>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::JoinStyle operator|(x11::JoinStyle l, x11::JoinStyle r) {
  using T = std::underlying_type_t<x11::JoinStyle>;
  return static_cast<x11::JoinStyle>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::JoinStyle operator&(x11::JoinStyle l, x11::JoinStyle r) {
  using T = std::underlying_type_t<x11::JoinStyle>;
  return static_cast<x11::JoinStyle>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::FillStyle operator|(x11::FillStyle l, x11::FillStyle r) {
  using T = std::underlying_type_t<x11::FillStyle>;
  return static_cast<x11::FillStyle>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::FillStyle operator&(x11::FillStyle l, x11::FillStyle r) {
  using T = std::underlying_type_t<x11::FillStyle>;
  return static_cast<x11::FillStyle>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::FillRule operator|(x11::FillRule l, x11::FillRule r) {
  using T = std::underlying_type_t<x11::FillRule>;
  return static_cast<x11::FillRule>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::FillRule operator&(x11::FillRule l, x11::FillRule r) {
  using T = std::underlying_type_t<x11::FillRule>;
  return static_cast<x11::FillRule>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::SubwindowMode operator|(x11::SubwindowMode l,
                                              x11::SubwindowMode r) {
  using T = std::underlying_type_t<x11::SubwindowMode>;
  return static_cast<x11::SubwindowMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::SubwindowMode operator&(x11::SubwindowMode l,
                                              x11::SubwindowMode r) {
  using T = std::underlying_type_t<x11::SubwindowMode>;
  return static_cast<x11::SubwindowMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ArcMode operator|(x11::ArcMode l, x11::ArcMode r) {
  using T = std::underlying_type_t<x11::ArcMode>;
  return static_cast<x11::ArcMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ArcMode operator&(x11::ArcMode l, x11::ArcMode r) {
  using T = std::underlying_type_t<x11::ArcMode>;
  return static_cast<x11::ArcMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ClipOrdering operator|(x11::ClipOrdering l,
                                             x11::ClipOrdering r) {
  using T = std::underlying_type_t<x11::ClipOrdering>;
  return static_cast<x11::ClipOrdering>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ClipOrdering operator&(x11::ClipOrdering l,
                                             x11::ClipOrdering r) {
  using T = std::underlying_type_t<x11::ClipOrdering>;
  return static_cast<x11::ClipOrdering>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::CoordMode operator|(x11::CoordMode l, x11::CoordMode r) {
  using T = std::underlying_type_t<x11::CoordMode>;
  return static_cast<x11::CoordMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::CoordMode operator&(x11::CoordMode l, x11::CoordMode r) {
  using T = std::underlying_type_t<x11::CoordMode>;
  return static_cast<x11::CoordMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::PolyShape operator|(x11::PolyShape l, x11::PolyShape r) {
  using T = std::underlying_type_t<x11::PolyShape>;
  return static_cast<x11::PolyShape>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::PolyShape operator&(x11::PolyShape l, x11::PolyShape r) {
  using T = std::underlying_type_t<x11::PolyShape>;
  return static_cast<x11::PolyShape>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ImageFormat operator|(x11::ImageFormat l,
                                            x11::ImageFormat r) {
  using T = std::underlying_type_t<x11::ImageFormat>;
  return static_cast<x11::ImageFormat>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ImageFormat operator&(x11::ImageFormat l,
                                            x11::ImageFormat r) {
  using T = std::underlying_type_t<x11::ImageFormat>;
  return static_cast<x11::ImageFormat>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ColormapAlloc operator|(x11::ColormapAlloc l,
                                              x11::ColormapAlloc r) {
  using T = std::underlying_type_t<x11::ColormapAlloc>;
  return static_cast<x11::ColormapAlloc>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ColormapAlloc operator&(x11::ColormapAlloc l,
                                              x11::ColormapAlloc r) {
  using T = std::underlying_type_t<x11::ColormapAlloc>;
  return static_cast<x11::ColormapAlloc>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ColorFlag operator|(x11::ColorFlag l, x11::ColorFlag r) {
  using T = std::underlying_type_t<x11::ColorFlag>;
  return static_cast<x11::ColorFlag>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::ColorFlag operator&(x11::ColorFlag l, x11::ColorFlag r) {
  using T = std::underlying_type_t<x11::ColorFlag>;
  return static_cast<x11::ColorFlag>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Pixmap operator|(x11::Pixmap l, x11::Pixmap r) {
  using T = std::underlying_type_t<x11::Pixmap>;
  return static_cast<x11::Pixmap>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Pixmap operator&(x11::Pixmap l, x11::Pixmap r) {
  using T = std::underlying_type_t<x11::Pixmap>;
  return static_cast<x11::Pixmap>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Font operator|(x11::Font l, x11::Font r) {
  using T = std::underlying_type_t<x11::Font>;
  return static_cast<x11::Font>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Font operator&(x11::Font l, x11::Font r) {
  using T = std::underlying_type_t<x11::Font>;
  return static_cast<x11::Font>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::QueryShapeOf operator|(x11::QueryShapeOf l,
                                             x11::QueryShapeOf r) {
  using T = std::underlying_type_t<x11::QueryShapeOf>;
  return static_cast<x11::QueryShapeOf>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::QueryShapeOf operator&(x11::QueryShapeOf l,
                                             x11::QueryShapeOf r) {
  using T = std::underlying_type_t<x11::QueryShapeOf>;
  return static_cast<x11::QueryShapeOf>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Keyboard operator|(x11::Keyboard l, x11::Keyboard r) {
  using T = std::underlying_type_t<x11::Keyboard>;
  return static_cast<x11::Keyboard>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Keyboard operator&(x11::Keyboard l, x11::Keyboard r) {
  using T = std::underlying_type_t<x11::Keyboard>;
  return static_cast<x11::Keyboard>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::LedMode operator|(x11::LedMode l, x11::LedMode r) {
  using T = std::underlying_type_t<x11::LedMode>;
  return static_cast<x11::LedMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::LedMode operator&(x11::LedMode l, x11::LedMode r) {
  using T = std::underlying_type_t<x11::LedMode>;
  return static_cast<x11::LedMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::AutoRepeatMode operator|(x11::AutoRepeatMode l,
                                               x11::AutoRepeatMode r) {
  using T = std::underlying_type_t<x11::AutoRepeatMode>;
  return static_cast<x11::AutoRepeatMode>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::AutoRepeatMode operator&(x11::AutoRepeatMode l,
                                               x11::AutoRepeatMode r) {
  using T = std::underlying_type_t<x11::AutoRepeatMode>;
  return static_cast<x11::AutoRepeatMode>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Blanking operator|(x11::Blanking l, x11::Blanking r) {
  using T = std::underlying_type_t<x11::Blanking>;
  return static_cast<x11::Blanking>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Blanking operator&(x11::Blanking l, x11::Blanking r) {
  using T = std::underlying_type_t<x11::Blanking>;
  return static_cast<x11::Blanking>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Exposures operator|(x11::Exposures l, x11::Exposures r) {
  using T = std::underlying_type_t<x11::Exposures>;
  return static_cast<x11::Exposures>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Exposures operator&(x11::Exposures l, x11::Exposures r) {
  using T = std::underlying_type_t<x11::Exposures>;
  return static_cast<x11::Exposures>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::HostMode operator|(x11::HostMode l, x11::HostMode r) {
  using T = std::underlying_type_t<x11::HostMode>;
  return static_cast<x11::HostMode>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::HostMode operator&(x11::HostMode l, x11::HostMode r) {
  using T = std::underlying_type_t<x11::HostMode>;
  return static_cast<x11::HostMode>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Family operator|(x11::Family l, x11::Family r) {
  using T = std::underlying_type_t<x11::Family>;
  return static_cast<x11::Family>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Family operator&(x11::Family l, x11::Family r) {
  using T = std::underlying_type_t<x11::Family>;
  return static_cast<x11::Family>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::AccessControl operator|(x11::AccessControl l,
                                              x11::AccessControl r) {
  using T = std::underlying_type_t<x11::AccessControl>;
  return static_cast<x11::AccessControl>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::AccessControl operator&(x11::AccessControl l,
                                              x11::AccessControl r) {
  using T = std::underlying_type_t<x11::AccessControl>;
  return static_cast<x11::AccessControl>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::CloseDown operator|(x11::CloseDown l, x11::CloseDown r) {
  using T = std::underlying_type_t<x11::CloseDown>;
  return static_cast<x11::CloseDown>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::CloseDown operator&(x11::CloseDown l, x11::CloseDown r) {
  using T = std::underlying_type_t<x11::CloseDown>;
  return static_cast<x11::CloseDown>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Kill operator|(x11::Kill l, x11::Kill r) {
  using T = std::underlying_type_t<x11::Kill>;
  return static_cast<x11::Kill>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Kill operator&(x11::Kill l, x11::Kill r) {
  using T = std::underlying_type_t<x11::Kill>;
  return static_cast<x11::Kill>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::ScreenSaverMode operator|(x11::ScreenSaverMode l,
                                                x11::ScreenSaverMode r) {
  using T = std::underlying_type_t<x11::ScreenSaverMode>;
  return static_cast<x11::ScreenSaverMode>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::ScreenSaverMode operator&(x11::ScreenSaverMode l,
                                                x11::ScreenSaverMode r) {
  using T = std::underlying_type_t<x11::ScreenSaverMode>;
  return static_cast<x11::ScreenSaverMode>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::MappingStatus operator|(x11::MappingStatus l,
                                              x11::MappingStatus r) {
  using T = std::underlying_type_t<x11::MappingStatus>;
  return static_cast<x11::MappingStatus>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::MappingStatus operator&(x11::MappingStatus l,
                                              x11::MappingStatus r) {
  using T = std::underlying_type_t<x11::MappingStatus>;
  return static_cast<x11::MappingStatus>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::MapIndex operator|(x11::MapIndex l, x11::MapIndex r) {
  using T = std::underlying_type_t<x11::MapIndex>;
  return static_cast<x11::MapIndex>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::MapIndex operator&(x11::MapIndex l, x11::MapIndex r) {
  using T = std::underlying_type_t<x11::MapIndex>;
  return static_cast<x11::MapIndex>(static_cast<T>(l) & static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XPROTO_H_
