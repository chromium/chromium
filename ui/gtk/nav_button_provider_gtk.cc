// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/nav_button_provider_gtk.h"

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/views/widget/widget.h"

namespace gtk {

namespace {

struct NavButtonIcon {
  // Used on Gtk3.
  ScopedGObject<GdkPixbuf> pixbuf;

  // Used on Gtk4.
  ScopedGObject<GdkTexture> texture;
};

// gtkheaderbar.c uses GTK_ICON_SIZE_MENU, which is 16px.
const int kNavButtonIconSize = 16;

// Specified in GtkHeaderBar spec.
const int kHeaderSpacing = 6;

const char* ButtonStyleClassFromButtonType(
    ui::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return "minimize";
    case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
    case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return "maximize";
    case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
      return "close";
    default:
      NOTREACHED();
  }
}

GtkStateFlags GtkStateFlagsFromButtonState(
    ui::NavButtonProvider::ButtonState state) {
  switch (state) {
    case ui::NavButtonProvider::ButtonState::kNormal:
      return GTK_STATE_FLAG_NORMAL;
    case ui::NavButtonProvider::ButtonState::kHovered:
      return GTK_STATE_FLAG_PRELIGHT;
    case ui::NavButtonProvider::ButtonState::kPressed:
      return static_cast<GtkStateFlags>(GTK_STATE_FLAG_PRELIGHT |
                                        GTK_STATE_FLAG_ACTIVE);
    case ui::NavButtonProvider::ButtonState::kDisabled:
      return GTK_STATE_FLAG_INSENSITIVE;
    default:
      NOTREACHED();
  }
}

const char* IconNameFromButtonType(
    ui::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return "window-minimize-symbolic";
    case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      return "window-maximize-symbolic";
    case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return "window-restore-symbolic";
    case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
      return "window-close-symbolic";
    default:
      NOTREACHED();
  }
}

gfx::Size LoadNavButtonIcon(ui::NavButtonProvider::FrameButtonDisplayType type,
                            GtkStyleContext* button_context,
                            int scale,
                            NavButtonIcon* icon = nullptr) {
  const char* icon_name = IconNameFromButtonType(type);
  if (!GtkCheckVersion(4)) {
    auto icon_info = TakeGObject(gtk_icon_theme_lookup_icon_for_scale(
        GetDefaultIconTheme(), icon_name, kNavButtonIconSize, scale,
        static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_USE_BUILTIN |
                                        GTK_ICON_LOOKUP_GENERIC_FALLBACK)));
    auto icon_pixbuf = TakeGObject(gtk_icon_info_load_symbolic_for_context(
        icon_info, button_context, nullptr, nullptr));
    gfx::Size size{gdk_pixbuf_get_width(icon_pixbuf),
                   gdk_pixbuf_get_height(icon_pixbuf)};
    if (icon) {
      icon->pixbuf = std::move(icon_pixbuf);
    }
    return size;
  }
  auto icon_paintable = Gtk4IconThemeLookupIcon(
      GetDefaultIconTheme(), icon_name, nullptr, kNavButtonIconSize, scale,
      GTK_TEXT_DIR_NONE, static_cast<GtkIconLookupFlags>(0));
  auto* paintable =
      GlibCast<GdkPaintable>(icon_paintable.get(), gdk_paintable_get_type());
  int width = scale * gdk_paintable_get_intrinsic_width(paintable);
  int height = scale * gdk_paintable_get_intrinsic_height(paintable);
  if (icon) {
    auto* snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable, snapshot, width, height);
    auto* node = gtk_snapshot_free_to_node(snapshot);

    size_t nbytes = width * height * sizeof(SkColor);
    void* pixels = g_malloc(nbytes);
    UNSAFE_TODO(memset(pixels, 0, nbytes));
    size_t stride = sizeof(SkColor) * width;

    CairoSurface surface(pixels, width, height);
    cairo_t* cr = surface.cairo();
    gsk_render_node_draw(node, cr);

    SkColor fg = GtkStyleContextGetColor(button_context);
    cairo_set_source_rgba(cr, SkColorGetR(fg) / 255.0, SkColorGetG(fg) / 255.0,
                          SkColorGetB(fg) / 255.0, SkColorGetA(fg) / 255.0);
    cairo_set_operator(cr, CAIRO_OPERATOR_IN);
    cairo_paint(cr);

    icon->texture = TakeGObject(
        gdk_memory_texture_new(width, height, GDK_MEMORY_B8G8R8A8,
                               g_bytes_new_take(pixels, nbytes), stride));
    gsk_render_node_unref(node);
  }
  return {width, height};
}

gfx::Size GetMinimumWidgetSize(gfx::Size content_size,
                               GtkStyleContext* content_context,
                               GtkCssContext widget_context) {
  gfx::Rect widget_rect = gfx::Rect(content_size);
  if (content_context) {
    widget_rect.Inset(-GtkStyleContextGetMargin(content_context));
  }

  auto min_size = GetMinimumContentSize(widget_context);
  widget_rect.set_width(std::max(widget_rect.width(), min_size.width()));
  widget_rect.set_height(std::max(widget_rect.height(), min_size.height()));

  widget_rect.Inset(-GtkStyleContextGetPadding(widget_context));
  widget_rect.Inset(-GtkStyleContextGetBorder(widget_context));

  return widget_rect.size();
}

GtkCssContext CreateHeaderContext(ui::FrameType type, bool maximized) {
  std::string window_selector = "window.background.csd";
  if (maximized) {
    window_selector += ".maximized";
  }
  return AppendCssNodeToStyleContext(
      AppendCssNodeToStyleContext({}, window_selector),
      type == ui::FrameType::kBrowser
          ? "headerbar.header-bar.titlebar"
          : "headerbar.default-decoration.titlebar");
}

GtkCssContext CreateWindowControlsContext(ui::FrameType type, bool maximized) {
  return AppendCssNodeToStyleContext(CreateHeaderContext(type, maximized),
                                     "windowcontrols");
}

void CalculateUnscaledButtonSize(
    ui::FrameType frame_type,
    ui::NavButtonProvider::FrameButtonDisplayType type,
    bool maximized,
    gfx::Size* button_size,
    gfx::Insets* button_margin) {
  // views::ImageButton expects the images for each state to be of the
  // same size, but GTK can, in general, use a differently-sized
  // button for each state.  For this reason, render buttons for all
  // states at the size of a GTK_STATE_FLAG_NORMAL button.
  auto button_context = AppendCssNodeToStyleContext(
      CreateWindowControlsContext(frame_type, maximized),
      "button.titlebutton." +
          std::string(ButtonStyleClassFromButtonType(type)));

  auto icon_size = LoadNavButtonIcon(type, button_context, 1);

  auto image_context = AppendCssNodeToStyleContext(button_context, "image");
  gfx::Size image_size =
      GetMinimumWidgetSize(icon_size, nullptr, image_context);

  *button_size =
      GetMinimumWidgetSize(image_size, image_context, button_context);
  *button_margin = GtkStyleContextGetMargin(button_context);
}

class NavButtonImageSource : public gfx::ImageSkiaSource {
 public:
  NavButtonImageSource(ui::FrameType frame_type,
                       ui::NavButtonProvider::FrameButtonDisplayType type,
                       ui::NavButtonProvider::ButtonState state,
                       bool maximized,
                       bool active,
                       gfx::Size button_size)
      : frame_type_(frame_type),
        type_(type),
        state_(state),
        maximized_(maximized),
        active_(active),
        button_size_(button_size) {}

  ~NavButtonImageSource() override = default;

  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    // gfx::ImageSkia kindly caches the result of this function, so
    // RenderNavButton() is called at most once for each needed scale
    // factor.  Additionally, buttons in the HOVERED or PRESSED states
    // are not actually rendered until they are needed.
    if (button_size_.IsEmpty()) {
      return gfx::ImageSkiaRep();
    }

    auto button_context = AppendCssNodeToStyleContext(
        CreateWindowControlsContext(frame_type_, maximized_),
        "button.titlebutton");
    gtk_style_context_add_class(button_context,
                                ButtonStyleClassFromButtonType(type_));
    GtkStateFlags button_state = GtkStateFlagsFromButtonState(state_);
    if (!active_) {
      button_state =
          static_cast<GtkStateFlags>(button_state | GTK_STATE_FLAG_BACKDROP);
    }
    gtk_style_context_set_state(button_context, button_state);

    // Gtk header bars usually have the same height in both maximized and
    // restored windows.  But chrome's tabstrip background has a smaller height
    // when maximized.  To prevent buttons from clipping outside of this region,
    // they are scaled down.  However, this is problematic for themes that do
    // not expect this case and use bitmaps for frame buttons (like the Breeze
    // theme).  When the background-size is set to auto, the background bitmap
    // is not scaled for the (unexpected) smaller button size, and the button's
    // edges appear cut off.  To fix this, manually set the background to scale
    // to the button size when it would have clipped.
    //
    // GTK's "contain" is unlike CSS's "contain".  In CSS, the image would only
    // be downsized when it would have clipped.  In GTK, the image is always
    // scaled to fit the drawing region (preserving aspect ratio).  Only add
    // "contain" if clipping would occur.
    int bg_width = 0;
    int bg_height = 0;
    if (GtkCheckVersion(4)) {
      auto* snapshot = gtk_snapshot_new();
      gtk_snapshot_render_background(snapshot, button_context, 0, 0,
                                     button_size_.width(),
                                     button_size_.height());
      if (auto* node = gtk_snapshot_free_to_node(snapshot)) {
        if (GdkTexture* texture = GetTextureFromRenderNode(node)) {
          bg_width = gdk_texture_get_width(texture);
          bg_height = gdk_texture_get_height(texture);
        }
        gsk_render_node_unref(node);
      }
    } else {
      cairo_pattern_t* cr_pattern = nullptr;
      cairo_surface_t* cr_surface = nullptr;
      GtkStyleContextGet(
          button_context,
          "background-image" /* GTK_STYLE_PROPERTY_BACKGROUND_IMAGE */,
          &cr_pattern, nullptr);
      if (cr_pattern) {
        cairo_pattern_get_surface(cr_pattern, &cr_surface);
        if (cr_surface &&
            cairo_surface_get_type(cr_surface) == CAIRO_SURFACE_TYPE_IMAGE) {
          bg_width = cairo_image_surface_get_width(cr_surface);
          bg_height = cairo_image_surface_get_height(cr_surface);
        }
        cairo_pattern_destroy(cr_pattern);
      }
    }
    if (bg_width > button_size_.width() || bg_height > button_size_.height()) {
      ApplyCssToContext(button_context,
                        ".titlebutton { background-size: contain; }");
    }

    // Gtk doesn't support fractional scale factors, but chrome does.
    // Rendering the button background and border at a fractional
    // scale factor is easy, since we can adjust the cairo context
    // transform.  But the icon is loaded from a pixbuf, so we pick
    // the next-highest integer scale and manually downsize.
    int pixbuf_scale = scale == static_cast<int>(scale) ? scale : scale + 1;
    NavButtonIcon icon;
    auto icon_size =
        LoadNavButtonIcon(type_, button_context, pixbuf_scale, &icon);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(scale * button_size_.width(),
                          scale * button_size_.height());
    bitmap.eraseColor(0);

    CairoSurface surface(bitmap);
    cairo_t* cr = surface.cairo();

    cairo_save(cr);
    cairo_scale(cr, scale, scale);
    gtk_render_background(button_context, cr, 0, 0, button_size_.width(),
                          button_size_.height());
    gtk_render_frame(button_context, cr, 0, 0, button_size_.width(),
                     button_size_.height());
    if (GtkCheckVersion(4)) {
      // In GTK4, themes can style the image child of titlebar buttons rather
      // than the button itself.  Render the image background too.
      auto image_context = AppendCssNodeToStyleContext(button_context, "image");
      int icon_w = icon_size.width() / pixbuf_scale;
      int icon_h = icon_size.height() / pixbuf_scale;
      auto img_size = GetMinimumWidgetSize(gfx::Size(icon_w, icon_h), nullptr,
                                           image_context);
      double img_x = (button_size_.width() - img_size.width()) / 2.0;
      double img_y = (button_size_.height() - img_size.height()) / 2.0;

      gtk_render_background(image_context, cr, img_x, img_y, img_size.width(),
                            img_size.height());
      gtk_render_frame(image_context, cr, img_x, img_y, img_size.width(),
                       img_size.height());
    }
    cairo_restore(cr);
    cairo_save(cr);
    float pixbuf_extra_scale = scale / pixbuf_scale;
    cairo_scale(cr, pixbuf_extra_scale, pixbuf_extra_scale);
    GtkRenderIcon(
        button_context, cr, icon.pixbuf, icon.texture,
        ((pixbuf_scale * button_size_.width() - icon_size.width()) / 2),
        ((pixbuf_scale * button_size_.height() - icon_size.height()) / 2));
    cairo_restore(cr);

    return gfx::ImageSkiaRep(bitmap, scale);
  }

  bool HasRepresentationAtAllScales() const override { return true; }

 private:
  ui::FrameType frame_type_;
  ui::NavButtonProvider::FrameButtonDisplayType type_;
  ui::NavButtonProvider::ButtonState state_;
  bool maximized_;
  bool active_;
  gfx::Size button_size_;
};

}  // namespace

NavButtonProviderGtk::NavButtonProviderGtk(ui::FrameType frame_type)
    : frame_type_(frame_type) {
  GtkSettings* settings = gtk_settings_get_default();
  auto callback = base::BindRepeating(&NavButtonProviderGtk::OnThemeChanged,
                                      base::Unretained(this));
  theme_name_signal_ = ScopedGSignal(settings, "notify::gtk-theme-name",
                                     callback, G_CONNECT_AFTER);
  prefer_dark_signal_ =
      ScopedGSignal(settings, "notify::gtk-application-prefer-dark-theme",
                    callback, G_CONNECT_AFTER);
}

NavButtonProviderGtk::~NavButtonProviderGtk() = default;

void NavButtonProviderGtk::RedrawImages(int top_area_height,
                                        bool maximized,
                                        bool active) {
  auto header_context = CreateHeaderContext(frame_type_, maximized);
  auto header_padding = GtkStyleContextGetPadding(header_context);

  double scale = 1.0f;
  std::map<ui::NavButtonProvider::FrameButtonDisplayType, gfx::Size>
      button_sizes;
  std::map<ui::NavButtonProvider::FrameButtonDisplayType, gfx::Insets>
      button_margins;
  std::vector<ui::NavButtonProvider::FrameButtonDisplayType> display_types{
      ui::NavButtonProvider::FrameButtonDisplayType::kMinimize,
      maximized ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
                : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize,
      ui::NavButtonProvider::FrameButtonDisplayType::kClose,
  };
  for (auto type : display_types) {
    CalculateUnscaledButtonSize(frame_type_, type, maximized,
                                &button_sizes[type], &button_margins[type]);
    int button_unconstrained_height = button_sizes[type].height() +
                                      button_margins[type].top() +
                                      button_margins[type].bottom();

    int needed_height = header_padding.top() + button_unconstrained_height +
                        header_padding.bottom();

    if (needed_height > top_area_height) {
      scale =
          std::min(scale, static_cast<double>(top_area_height) / needed_height);
    }
  }

  top_area_spacing_ =
      gfx::Insets::TLBR(std::round(scale * header_padding.top()),
                        std::round(scale * header_padding.left()),
                        std::round(scale * header_padding.bottom()),
                        std::round(scale * header_padding.right()));

  inter_button_spacing_ = std::round(scale * kHeaderSpacing);

  for (auto type : display_types) {
    double button_height =
        scale * (button_sizes[type].height() + button_margins[type].top() +
                 button_margins[type].bottom());
    double available_height =
        top_area_height -
        scale * (header_padding.top() + header_padding.bottom());
    double scaled_button_offset = (available_height - button_height) / 2;

    gfx::Size size = button_sizes[type];
    size = gfx::Size(std::round(scale * size.width()),
                     std::round(scale * size.height()));
    gfx::Insets margin = button_margins[type];
    margin = gfx::Insets::TLBR(
        std::round(scale * (header_padding.top() + margin.top()) +
                   scaled_button_offset),
        std::round(scale * margin.left()), 0,
        std::round(scale * margin.right()));

    button_margins_[type] = margin;

    for (auto state : {
             ui::NavButtonProvider::ButtonState::kNormal,
             ui::NavButtonProvider::ButtonState::kHovered,
             ui::NavButtonProvider::ButtonState::kPressed,
             ui::NavButtonProvider::ButtonState::kDisabled,
         }) {
      button_images_[type][state] =
          gfx::ImageSkia(std::make_unique<NavButtonImageSource>(
                             frame_type_, type, state, maximized, active, size),
                         size);
    }
  }
}

gfx::ImageSkia NavButtonProviderGtk::GetImage(
    ui::NavButtonProvider::FrameButtonDisplayType type,
    ui::NavButtonProvider::ButtonState state) const {
  auto it = button_images_.find(type);
  CHECK(it != button_images_.end());
  auto it2 = it->second.find(state);
  CHECK(it2 != it->second.end());
  return it2->second;
}

gfx::Insets NavButtonProviderGtk::GetNavButtonMargin(
    ui::NavButtonProvider::FrameButtonDisplayType type) const {
  auto it = button_margins_.find(type);
  CHECK(it != button_margins_.end());
  return it->second;
}

gfx::Insets NavButtonProviderGtk::GetTopAreaSpacing() const {
  return top_area_spacing_;
}

int NavButtonProviderGtk::GetNavButtonHeight(bool maximized) const {
  auto& cached =
      maximized ? nav_button_height_maximized_ : nav_button_height_restored_;
  if (!cached.has_value()) {
    int max_height = 0;
    for (auto type : {
             ui::NavButtonProvider::FrameButtonDisplayType::kMinimize,
             maximized
                 ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
                 : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize,
             ui::NavButtonProvider::FrameButtonDisplayType::kClose,
         }) {
      gfx::Size button_size;
      gfx::Insets button_margin;
      CalculateUnscaledButtonSize(frame_type_, type, maximized, &button_size,
                                  &button_margin);
      max_height =
          std::max(max_height, button_size.height() + button_margin.top() +
                                   button_margin.bottom());
    }
    cached = max_height;
  }
  return *cached;
}

int NavButtonProviderGtk::GetInterNavButtonSpacing() const {
  return inter_button_spacing_;
}

void NavButtonProviderGtk::OnThemeChanged(GtkSettings* settings,
                                          GtkParamSpec* param) {
  nav_button_height_restored_ = std::nullopt;
  nav_button_height_maximized_ = std::nullopt;
}

}  // namespace gtk
