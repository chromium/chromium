// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gtk/gtk_util.h"

#include <locale.h>
#include <stddef.h>

#include <memory>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/functional/callback.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_types.h"
#include "ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/linux/linux_ui.h"
#include "ui/native_theme/common_theme.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace gtk {

namespace {

const char kAuraTransientParent[] = "aura-transient-parent";

GskRenderNode* GetRenderNodeChild(GskRenderNode* node) {
  switch (gsk_render_node_get_node_type(node)) {
    case GSK_TRANSFORM_NODE:
      return gsk_transform_node_get_child(node);
    case GSK_OPACITY_NODE:
      return gsk_opacity_node_get_child(node);
    case GSK_COLOR_MATRIX_NODE:
      return gsk_color_matrix_node_get_child(node);
    case GSK_REPEAT_NODE:
      return gsk_repeat_node_get_child(node);
    case GSK_CLIP_NODE:
      return gsk_clip_node_get_child(node);
    case GSK_ROUNDED_CLIP_NODE:
      return gsk_rounded_clip_node_get_child(node);
    case GSK_SHADOW_NODE:
      return gsk_shadow_node_get_child(node);
    case GSK_BLUR_NODE:
      return gsk_blur_node_get_child(node);
    case GSK_DEBUG_NODE:
      return gsk_debug_node_get_child(node);
    default:
      return nullptr;
  }
}

std::vector<GskRenderNode*> GetRenderNodeChildren(GskRenderNode* node) {
  std::vector<GskRenderNode*> result;
  size_t n_children = 0;
  GskRenderNode* (*get_child)(UI_GTK_CONST GskRenderNode*, guint) = nullptr;
  switch (gsk_render_node_get_node_type(node)) {
    case GSK_CONTAINER_NODE:
      n_children = gsk_container_node_get_n_children(node);
      get_child = gsk_container_node_get_child;
      break;
    case GSK_GL_SHADER_NODE:
      n_children = gsk_gl_shader_node_get_n_children(node);
      get_child = gsk_gl_shader_node_get_child;
      break;
    default:
      return result;
  }
  result.reserve(n_children);
  for (size_t i = 0; i < n_children; i++) {
    result.push_back(get_child(node, i));
  }
  return result;
}

GtkCssContext AppendCssNodeToStyleContextImpl(
    GtkCssContext context,
    const std::string& name,
    const std::string& object_name,
    const std::vector<std::string>& classes,
    GtkStateFlags state,
    float scale) {
  if (GtkCheckVersion(4)) {
    // GTK_TYPE_BOX is used instead of GTK_TYPE_WIDGET because:
    // 1. Widgets are abstract and cannot be created directly.
    // 2. The widget must be a container type so that it unrefs child widgets
    //    on destruction.
    auto* widget_object = object_name.empty()
                              ? g_object_new(GTK_TYPE_BOX, nullptr)
                              : g_object_new(GTK_TYPE_BOX, "css-name",
                                             object_name.c_str(), nullptr);
    auto widget = TakeGObject(GTK_WIDGET(widget_object));

    if (!name.empty()) {
      gtk_widget_set_name(widget, name.c_str());
    }

    std::vector<const char*> css_classes;
    css_classes.reserve(classes.size() + 1);
    for (const auto& css_class : classes) {
      css_classes.push_back(css_class.c_str());
    }
    css_classes.push_back(nullptr);
    gtk_widget_set_css_classes(widget, css_classes.data());

    gtk_widget_set_state_flags(widget, state, false);

    if (context) {
      gtk_widget_set_parent(widget, context.widget());
    }

    gtk_style_context_set_scale(gtk_widget_get_style_context(widget), scale);

    return GtkCssContext(widget, context ? context.root() : widget);
  } else {
    GtkWidgetPath* path =
        context ? gtk_widget_path_copy(gtk_style_context_get_path(context))
                : gtk_widget_path_new();
    gtk_widget_path_append_type(path, G_TYPE_NONE);

    if (!object_name.empty()) {
      gtk_widget_path_iter_set_object_name(path, -1, object_name.c_str());
    }

    if (!name.empty()) {
      gtk_widget_path_iter_set_name(path, -1, name.c_str());
    }

    for (const auto& css_class : classes) {
      gtk_widget_path_iter_add_class(path, -1, css_class.c_str());
    }

    gtk_widget_path_iter_set_state(path, -1, state);

    GtkCssContext child_context(TakeGObject(gtk_style_context_new()));
    gtk_style_context_set_path(child_context, path);
    gtk_style_context_set_state(child_context, state);
    gtk_style_context_set_scale(child_context, scale);
    gtk_style_context_set_parent(child_context, context);

    gtk_widget_path_unref(path);
    return GtkCssContext(child_context);
  }
}

GtkWidget* CreateDummyWindow() {
  GtkWidget* window = GtkToplevelWindowNew();
  gtk_widget_realize(window);
  return window;
}

double GetOpacityFromRenderNode(GskRenderNode* node) {
  DCHECK(GtkCheckVersion(4));
  if (!node) {
    return 1;
  }

  if (gsk_render_node_get_node_type(node) == GSK_OPACITY_NODE) {
    return gsk_opacity_node_get_opacity(node);
  }
  return GetOpacityFromRenderNode(GetRenderNodeChild(node));
}

}  // namespace

const char* GtkCssMenu() {
  return GtkCheckVersion(4) ? "popover.background.menu contents" : "menu";
}

const char* GtkCssMenuItem() {
  return GtkCheckVersion(4) ? "modelbutton.flat" : "menuitem";
}

const char* GtkCssMenuScrollbar() {
  return GtkCheckVersion(4) ? "scrollbar range" : "scrollbar trough";
}

bool GtkInitFromCommandLine(int* argc, char** argv) {
  // Callers should have already called setlocale(LC_ALL, "") and
  // setlocale(LC_NUMERIC, "C") by now. Chrome does this in
  // service_manager::Main.
  DCHECK_EQ(strcmp(setlocale(LC_NUMERIC, nullptr), "C"), 0);
  // This prevents GTK from calling setlocale(LC_ALL, ""), which potentially
  // overwrites the LC_NUMERIC locale to something other than "C".
  gtk_disable_setlocale();
  return GtkInitCheck(argc, argv);
}

void SetGtkTransientForAura(GtkWidget* dialog, aura::Window* parent) {
  if (!parent || !parent->GetHost()) {
    return;
  }

  gtk_widget_realize(dialog);
  gfx::AcceleratedWidget parent_id = parent->GetHost()->GetAcceleratedWidget();
  GtkUi::GetPlatform()->SetGtkWidgetTransientFor(dialog, parent_id);

  // We also set the |parent| as a property of |dialog|, so that we can unlink
  // the two later.
  g_object_set_data(G_OBJECT(dialog), kAuraTransientParent, parent);
}

aura::Window* GetAuraTransientParent(GtkWidget* dialog) {
  return reinterpret_cast<aura::Window*>(
      g_object_get_data(G_OBJECT(dialog), kAuraTransientParent));
}

void ClearAuraTransientParent(GtkWidget* dialog, aura::Window* parent) {
  g_object_set_data(G_OBJECT(dialog), kAuraTransientParent, nullptr);
  GtkUi::GetPlatform()->ClearTransientFor(
      parent->GetHost()->GetAcceleratedWidget());
}

base::OnceClosure DisableHostInputHandling(GtkWidget* dialog,
                                           aura::Window* parent) {
  if (!parent) {
    return {};
  }
  auto* host =
      static_cast<views::DesktopWindowTreeHostLinux*>(parent->GetHost());
  if (!host) {
    return {};
  }

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  // In some circumstances the mouse has been captured and by turning off event
  // listening, it is never released. So we manually ensure there is no current
  // capture.
  host->ReleaseCapture();
  return host->DisableEventListening();
}

void ParseButtonLayout(const std::string& button_string,
                       std::vector<views::FrameButton>* leading_buttons,
                       std::vector<views::FrameButton>* trailing_buttons) {
  leading_buttons->clear();
  trailing_buttons->clear();
  bool left_side = true;
  base::StringTokenizer tokenizer(button_string, ":,");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    if (tokenizer.token_is_delim()) {
      if (*tokenizer.token_begin() == ':') {
        left_side = false;
      }
    } else {
      std::string_view token = tokenizer.token_piece();
      if (token == "minimize") {
        (left_side ? leading_buttons : trailing_buttons)
            ->push_back(views::FrameButton::kMinimize);
      } else if (token == "maximize") {
        (left_side ? leading_buttons : trailing_buttons)
            ->push_back(views::FrameButton::kMaximize);
      } else if (token == "close") {
        (left_side ? leading_buttons : trailing_buttons)
            ->push_back(views::FrameButton::kClose);
      }
    }
  }
}

CairoSurface::CairoSurface(SkBitmap& bitmap)
    : surface_(cairo_image_surface_create_for_data(
          static_cast<unsigned char*>(bitmap.getAddr(0, 0)),
          CAIRO_FORMAT_ARGB32,
          bitmap.width(),
          bitmap.height(),
          cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, bitmap.width()))),
      cairo_(cairo_create(surface_)) {}

CairoSurface::CairoSurface(const gfx::Size& size)
    : surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                          size.width(),
                                          size.height())),
      cairo_(cairo_create(surface_)) {
  DCHECK(cairo_surface_status(surface_) == CAIRO_STATUS_SUCCESS);
  // Clear the surface.
  cairo_save(cairo_);
  cairo_set_source_rgba(cairo_, 0, 0, 0, 0);
  cairo_set_operator(cairo_, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_);
  cairo_restore(cairo_);
}

CairoSurface::~CairoSurface() {
  // `cairo_destroy` and `cairo_surface_destroy` decrease the reference count on
  // `cairo_` and `surface_` objects respectively. The underlying memory is
  // freed if the reference count goes to zero. We use ExtractAsDangling() here
  // to avoid holding a briefly dangling ptr in case the memory is freed.
  cairo_destroy(cairo_.ExtractAsDangling());
  cairo_surface_destroy(surface_.ExtractAsDangling());
}

SkColor CairoSurface::GetAveragePixelValue(bool frame) {
  cairo_surface_flush(surface_);
  SkColor* data =
      reinterpret_cast<SkColor*>(cairo_image_surface_get_data(surface_));
  int width = cairo_image_surface_get_width(surface_);
  int height = cairo_image_surface_get_height(surface_);
  DCHECK(4 * width == cairo_image_surface_get_stride(surface_));
  long a = 0, r = 0, g = 0, b = 0;
  unsigned int max_alpha = 0;
  for (int i = 0; i < width * height; i++) {
    SkColor color = data[i];
    max_alpha = std::max(SkColorGetA(color), max_alpha);
    a += SkColorGetA(color);
    r += SkColorGetR(color);
    g += SkColorGetG(color);
    b += SkColorGetB(color);
  }
  if (a == 0) {
    return SK_ColorTRANSPARENT;
  }
  return SkColorSetARGB(frame ? max_alpha : a / (width * height), r * 255 / a,
                        g * 255 / a, b * 255 / a);
}

GtkCssContext::GtkCssContext(GtkWidget* widget, GtkWidget* root)
    : widget_(widget), root_(WrapGObject(root)) {
  DCHECK(GtkCheckVersion(4));
}

GtkCssContext::GtkCssContext(GtkStyleContext* context)
    : context_(WrapGObject(context)) {
  DCHECK(!GtkCheckVersion(4));
}

GtkCssContext::GtkCssContext() = default;
GtkCssContext::GtkCssContext(const GtkCssContext&) = default;
GtkCssContext::GtkCssContext(GtkCssContext&&) = default;
GtkCssContext& GtkCssContext::operator=(const GtkCssContext&) = default;
GtkCssContext& GtkCssContext::operator=(GtkCssContext&&) = default;
GtkCssContext::~GtkCssContext() {
  widget_.ExtractAsDangling();
}

GtkCssContext::operator GtkStyleContext*() {
  if (GtkCheckVersion(4)) {
    return widget_ ? gtk_widget_get_style_context(widget_) : nullptr;
  }
  return context_;
}

GtkCssContext GtkCssContext::GetParent() {
  if (GtkCheckVersion(4)) {
    return GtkCssContext(WrapGObject(gtk_widget_get_parent(widget_)),
                         root_ == widget_ ? ScopedGObject<GtkWidget>() : root_);
  }
  return GtkCssContext(WrapGObject(gtk_style_context_get_parent(context_)));
}

GtkWidget* GtkCssContext::widget() {
  DCHECK(GtkCheckVersion(4));
  return widget_;
}

GtkWidget* GtkCssContext::root() {
  DCHECK(GtkCheckVersion(4));
  return root_;
}

GtkStateFlags StateToStateFlags(ui::NativeTheme::State state) {
  switch (state) {
    case ui::NativeTheme::kDisabled:
      return GTK_STATE_FLAG_INSENSITIVE;
    case ui::NativeTheme::kHovered:
      return GTK_STATE_FLAG_PRELIGHT;
    case ui::NativeTheme::kNormal:
      return GTK_STATE_FLAG_NORMAL;
    case ui::NativeTheme::kPressed:
      return static_cast<GtkStateFlags>(GTK_STATE_FLAG_PRELIGHT |
                                        GTK_STATE_FLAG_ACTIVE);
    default:
      NOTREACHED_IN_MIGRATION();
      return GTK_STATE_FLAG_NORMAL;
  }
}

NO_SANITIZE("cfi-icall")
GtkCssContext AppendCssNodeToStyleContext(GtkCssContext context,
                                          const std::string& css_node) {
  enum {
    CSS_NAME,
    CSS_OBJECT_NAME,
    CSS_CLASS,
    CSS_PSEUDOCLASS,
    CSS_NONE,
  } part_type = CSS_OBJECT_NAME;

  static const struct {
    const char* name;
    GtkStateFlags state_flag;
  } pseudo_classes[] = {
      {"active", GTK_STATE_FLAG_ACTIVE},
      {"hover", GTK_STATE_FLAG_PRELIGHT},
      {"selected", GTK_STATE_FLAG_SELECTED},
      {"disabled", GTK_STATE_FLAG_INSENSITIVE},
      {"indeterminate", GTK_STATE_FLAG_INCONSISTENT},
      {"focus", GTK_STATE_FLAG_FOCUSED},
      {"backdrop", GTK_STATE_FLAG_BACKDROP},
      {"link", GTK_STATE_FLAG_LINK},
      {"visited", GTK_STATE_FLAG_VISITED},
      {"checked", GTK_STATE_FLAG_CHECKED},
  };

  std::string name;
  std::string object_name;
  std::vector<std::string> classes;
  GtkStateFlags state = GTK_STATE_FLAG_NORMAL;

  base::StringTokenizer t(css_node, ".:()");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      switch (*t.token_begin()) {
        case '(':
          part_type = CSS_NAME;
          break;
        case ')':
          part_type = CSS_NONE;
          break;
        case '.':
          part_type = CSS_CLASS;
          break;
        case ':':
          part_type = CSS_PSEUDOCLASS;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    } else {
      switch (part_type) {
        case CSS_NAME:
          name = t.token();
          break;
        case CSS_OBJECT_NAME:
          object_name = t.token();
          break;
        case CSS_CLASS:
          classes.push_back(t.token());
          break;
        case CSS_PSEUDOCLASS: {
          GtkStateFlags state_flag = GTK_STATE_FLAG_NORMAL;
          for (const auto& pseudo_class_entry : pseudo_classes) {
            if (strcmp(pseudo_class_entry.name, t.token().c_str()) == 0) {
              state_flag = pseudo_class_entry.state_flag;
              break;
            }
          }
          state = static_cast<GtkStateFlags>(state | state_flag);
          break;
        }
        case CSS_NONE:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }

  // Always add a "chromium" class so that themes can style chromium
  // widgets specially if they want to.
  classes.push_back("chromium");

  float scale = std::round(GetDeviceScaleFactor());

  return AppendCssNodeToStyleContextImpl(context, name, object_name, classes,
                                         state, scale);
}

GtkCssContext GetStyleContextFromCss(const std::string& css_selector) {
  // Prepend a window node to the selector since all widgets must live
  // in a window, but we don't want to specify that every time.
  auto context = AppendCssNodeToStyleContext({}, "window.background");

  for (const auto& widget_type :
       base::SplitString(css_selector, base::kWhitespaceASCII,
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    context = AppendCssNodeToStyleContext(context, widget_type);
  }
  return context;
}

SkColor GetBgColorFromStyleContext(GtkCssContext context) {
  // Backgrounds are more general than solid colors (eg. gradients),
  // but chromium requires us to boil this down to one color.  We
  // cannot use the background-color here because some themes leave it
  // set to a garbage color because a background-image will cover it
  // anyway.  So we instead render the background into a 24x24 bitmap,
  // removing any borders, and hope that we get a good color.
  ApplyCssToContext(context,
                    "* {"
                    "border-radius: 0px;"
                    "border-style: none;"
                    "box-shadow: none;"
                    "}");
  gfx::Size size(24, 24);
  CairoSurface surface(size);
  RenderBackground(size, surface.cairo(), context);
  return surface.GetAveragePixelValue(false);
}

SkColor GetFgColor(const std::string& css_selector) {
  return GtkStyleContextGetColor(GetStyleContextFromCss(css_selector));
}

ScopedCssProvider GetCssProvider(const std::string& css) {
  auto provider = TakeGObject(gtk_css_provider_new());
  GtkCssProviderLoadFromData(provider, css.c_str(), -1);
  return provider;
}

void ApplyCssProviderToContext(GtkCssContext context,
                               GtkCssProvider* provider) {
  while (context) {
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                   G_MAXUINT);
    context = context.GetParent();
  }
}

void ApplyCssToContext(GtkCssContext context, const std::string& css) {
  auto provider = GetCssProvider(css);
  ApplyCssProviderToContext(context, provider);
}

void RenderBackground(const gfx::Size& size,
                      cairo_t* cr,
                      GtkCssContext context) {
  if (!context) {
    return;
  }
  RenderBackground(size, cr, context.GetParent());
  gtk_render_background(context, cr, 0, 0, size.width(), size.height());
}

SkColor GetBgColor(const std::string& css_selector) {
  return GetBgColorFromStyleContext(GetStyleContextFromCss(css_selector));
}

SkColor GetBorderColor(const std::string& css_selector) {
  // Borders have the same issue as backgrounds, due to the
  // border-image property.
  auto context = GetStyleContextFromCss(css_selector);
  gfx::Size size(24, 24);
  CairoSurface surface(size);
  gtk_render_frame(context, surface.cairo(), 0, 0, size.width(), size.height());
  return surface.GetAveragePixelValue(true);
}

SkColor GetSelectionBgColor(const std::string& css_selector) {
  return GetBgColorFromStyleContext(GetStyleContextFromCss(css_selector));
}

bool ContextHasClass(GtkCssContext context, const std::string& style_class) {
  bool has_class = gtk_style_context_has_class(context, style_class.c_str());
  if (!GtkCheckVersion(4)) {
    has_class |= gtk_widget_path_iter_has_class(
        gtk_style_context_get_path(context), -1, style_class.c_str());
  }
  return has_class;
}

SkColor GetSeparatorColor(const std::string& css_selector) {
  auto context = GetStyleContextFromCss(css_selector);
  bool horizontal = ContextHasClass(context, "horizontal");

  int w = 1, h = 1;
  if (GtkCheckVersion(4)) {
    auto size = GetSeparatorSize(horizontal);
    w = size.width();
    h = size.height();
  } else {
    GtkStyleContextGet(context, "min-width", &w, "min-height", &h, nullptr);
  }
  auto border = GtkStyleContextGetBorder(context);
  auto padding = GtkStyleContextGetPadding(context);
  w += border.left() + padding.left() + padding.right() + border.right();
  h += border.top() + padding.top() + padding.bottom() + border.bottom();

  if (horizontal) {
    w = 24;
    h = std::max(h, 1);
  } else {
    DCHECK(ContextHasClass(context, "vertical"));
    h = 24;
    w = std::max(w, 1);
  }

  CairoSurface surface(gfx::Size(w, h));
  gtk_render_background(context, surface.cairo(), 0, 0, w, h);
  gtk_render_frame(context, surface.cairo(), 0, 0, w, h);
  return surface.GetAveragePixelValue(false);
}

std::string GetGtkSettingsStringProperty(GtkSettings* settings,
                                         const gchar* prop_name) {
  GValue layout = G_VALUE_INIT;
  g_value_init(&layout, G_TYPE_STRING);
  g_object_get_property(G_OBJECT(settings), prop_name, &layout);
  DCHECK(G_VALUE_HOLDS_STRING(&layout));
  std::string prop_value(g_value_get_string(&layout));
  g_value_unset(&layout);
  return prop_value;
}

int BuildXkbStateFromGdkEvent(unsigned int state, unsigned char group) {
  return state | ((group & 0x3) << 13);
}

GdkModifierType ExtractGdkEventStateFromKeyEventFlags(int flags) {
  auto event_flags = static_cast<ui::EventFlags>(flags);
  static const struct {
    ui::EventFlags event_flag;
    GdkModifierType gdk_modifier;
  } mapping[] = {
      {ui::EF_SHIFT_DOWN, GDK_SHIFT_MASK},
      {ui::EF_CAPS_LOCK_ON, GDK_LOCK_MASK},
      {ui::EF_CONTROL_DOWN, GDK_CONTROL_MASK},
      {ui::EF_ALT_DOWN, GDK_ALT_MASK},
      {ui::EF_LEFT_MOUSE_BUTTON, GDK_BUTTON1_MASK},
      {ui::EF_MIDDLE_MOUSE_BUTTON, GDK_BUTTON2_MASK},
      {ui::EF_RIGHT_MOUSE_BUTTON, GDK_BUTTON3_MASK},
      {ui::EF_BACK_MOUSE_BUTTON, GDK_BUTTON4_MASK},
      {ui::EF_FORWARD_MOUSE_BUTTON, GDK_BUTTON5_MASK},
  };
  unsigned int gdk_modifier_type = 0;
  for (const auto& map : mapping) {
    if (event_flags & map.event_flag) {
      gdk_modifier_type = gdk_modifier_type | map.gdk_modifier;
    }
  }
  return static_cast<GdkModifierType>(gdk_modifier_type);
}

int GetKeyEventProperty(const ui::KeyEvent& key_event,
                        const char* property_key) {
  auto* properties = key_event.properties();
  if (!properties) {
    return 0;
  }
  auto it = properties->find(property_key);
  DCHECK(it == properties->end() || it->second.size() == 1);
  return (it != properties->end()) ? it->second[0] : 0;
}

GdkModifierType GetGdkKeyEventState(const ui::KeyEvent& key_event) {
  // ui::KeyEvent uses a normalized modifier state which is not respected by
  // Gtk, so instead we obtain the original value from annotated properties.
  // See also x11_event_translation.cc where it is annotated.
  // cf) https://crbug.com/1086946#c11.
  const ui::Event::Properties* properties = key_event.properties();
  if (!properties) {
    return static_cast<GdkModifierType>(0);
  }
  auto it = properties->find(ui::kPropertyKeyboardState);
  if (it == properties->end()) {
    return static_cast<GdkModifierType>(0);
  }
  DCHECK_EQ(it->second.size(), 4u);
  // Stored in little endian.
  int result = 0;
  int bitshift = 0;
  for (uint8_t value : it->second) {
    result |= value << bitshift;
    bitshift += 8;
  }
  return static_cast<GdkModifierType>(result);
}

GdkEvent* GdkEventFromKeyEvent(const ui::KeyEvent& key_event) {
  DCHECK(!GtkCheckVersion(4));
  GdkEventType event_type = key_event.type() == ui::EventType::kKeyPressed
                                ? GdkKeyPress()
                                : GdkKeyRelease();
  auto event_time = key_event.time_stamp() - base::TimeTicks();
  int hw_code = GetKeyEventProperty(key_event, ui::kPropertyKeyboardHwKeyCode);
  int group = GetKeyEventProperty(key_event, ui::kPropertyKeyboardGroup);

  // Get GdkKeymap
  GdkKeymap* keymap = GtkUi::GetPlatform()->GetGdkKeymap();

  // Get keyval and state
  GdkModifierType state = GetGdkKeyEventState(key_event);
  guint keyval = GDK_KEY_VoidSymbol;
  GdkModifierType consumed;
  gdk_keymap_translate_keyboard_state(keymap, hw_code, state, group, &keyval,
                                      nullptr, nullptr, &consumed);
  gdk_keymap_add_virtual_modifiers(keymap, &state);
  DCHECK(keyval != GDK_KEY_VoidSymbol);

  // Build GdkEvent
  GdkEvent* gdk_event = gdk_event_new(event_type);
  GdkEventKey* gdk_event_key = reinterpret_cast<GdkEventKey*>(gdk_event);
  gdk_event_key->type = event_type;
  gdk_event_key->time = event_time.InMilliseconds();
  gdk_event_key->hardware_keycode = hw_code;
  gdk_event_key->keyval = keyval;
  gdk_event_key->state = BuildXkbStateFromGdkEvent(state, group);
  gdk_event_key->group = group;
  gdk_event_key->send_event = key_event.flags() & ui::EF_FINAL;
  gdk_event_key->is_modifier = state & GDK_MODIFIER_MASK;
  gdk_event_key->length = 0;
  gdk_event_key->string = nullptr;

  return gdk_event;
}

GtkIconTheme* GetDefaultIconTheme() {
  return GtkCheckVersion(4)
             ? gtk_icon_theme_get_for_display(gdk_display_get_default())
             : gtk_icon_theme_get_default();
}

void GtkWindowDestroy(GtkWidget* widget) {
  if (GtkCheckVersion(4)) {
    gtk_window_destroy(GTK_WINDOW(widget));
  } else {
    gtk_widget_destroy(widget);
  }
}

GtkWidget* GetDummyWindow() {
  static GtkWidget* window = CreateDummyWindow();
  return window;
}

gfx::Size GetSeparatorSize(bool horizontal) {
  auto widget = TakeGObject(gtk_separator_new(
      horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL));
  GtkRequisition natural_size;
  gtk_widget_get_preferred_size(widget, nullptr, &natural_size);
  return {natural_size.width, natural_size.height};
}

float GetDeviceScaleFactor() {
  if (const auto* linux_ui = ui::LinuxUi::instance()) {
    return linux_ui->display_config().primary_scale;
  }
  return 1.0f;
}

GdkTexture* GetTextureFromRenderNode(GskRenderNode* node) {
  DCHECK(GtkCheckVersion(4));
  if (!node) {
    return nullptr;
  }

  if (gsk_render_node_get_node_type(node) == GSK_TEXTURE_NODE) {
    return gsk_texture_node_get_texture(node);
  }

  if (auto* texture = GetTextureFromRenderNode(GetRenderNodeChild(node))) {
    return texture;
  }
  for (GskRenderNode* child : GetRenderNodeChildren(node)) {
    if (auto* texture = GetTextureFromRenderNode(child)) {
      return texture;
    }
  }
  return nullptr;
}

double GetOpacityFromContext(GtkStyleContext* context) {
  double opacity = 1;
  if (!GtkCheckVersion(4)) {
    GtkStyleContextGet(context, "opacity", &opacity, nullptr);
    return opacity;
  }

  auto* snapshot = gtk_snapshot_new();
  gtk_snapshot_render_background(snapshot, context, 0, 0, 1, 1);
  if (auto* node = gtk_snapshot_free_to_node(snapshot)) {
    opacity = GetOpacityFromRenderNode(node);
    gsk_render_node_unref(node);
  }
  return opacity;
}

}  // namespace gtk
