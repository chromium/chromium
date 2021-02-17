// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/nav_button_provider_gtk.h"

#include <gtk/gtk.h>

#include "ui/base/glib/scoped_gobject.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gtk/gtk_util.h"
#include "ui/views/widget/widget.h"

namespace gtk {

namespace {

#if GTK_CHECK_VERSION(3, 90, 0)
using NavButtonIcon = ScopedGObject<GdkTexture>;
#else
using NavButtonIcon = ScopedGObject<GdkPixbuf>;
#endif

// gtkheaderbar.c uses GTK_ICON_SIZE_MENU, which is 16px.
const int kNavButtonIconSize = 16;

// Specified in GtkHeaderBar spec.
const int kHeaderSpacing = 6;

const char* ButtonStyleClassFromButtonType(
    views::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case views::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return "minimize";
    case views::NavButtonProvider::FrameButtonDisplayType::kMaximize:
    case views::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return "maximize";
    case views::NavButtonProvider::FrameButtonDisplayType::kClose:
      return "close";
    default:
      NOTREACHED();
      return "";
  }
}

GtkStateFlags GtkStateFlagsFromButtonState(views::Button::ButtonState state) {
  switch (state) {
    case views::Button::STATE_NORMAL:
      return GTK_STATE_FLAG_NORMAL;
    case views::Button::STATE_HOVERED:
      return GTK_STATE_FLAG_PRELIGHT;
    case views::Button::STATE_PRESSED:
      return static_cast<GtkStateFlags>(GTK_STATE_FLAG_PRELIGHT |
                                        GTK_STATE_FLAG_ACTIVE);
    case views::Button::STATE_DISABLED:
      return GTK_STATE_FLAG_INSENSITIVE;
    default:
      NOTREACHED();
      return GTK_STATE_FLAG_NORMAL;
  }
}

const char* IconNameFromButtonType(
    views::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case views::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return "window-minimize-symbolic";
    case views::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      return "window-maximize-symbolic";
    case views::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return "window-restore-symbolic";
    case views::NavButtonProvider::FrameButtonDisplayType::kClose:
      return "window-close-symbolic";
    default:
      NOTREACHED();
      return "";
  }
}

gfx::Insets InsetsFromGtkBorder(const GtkBorder& border) {
  return gfx::Insets(border.top, border.left, border.bottom, border.right);
}

gfx::Insets PaddingFromStyleContext(GtkStyleContext* context,
                                    GtkStateFlags state) {
  GtkBorder padding;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get_padding(context, &padding);
#else
  gtk_style_context_get_padding(context, state, &padding);
#endif
  return InsetsFromGtkBorder(padding);
}

gfx::Insets BorderFromStyleContext(GtkStyleContext* context,
                                   GtkStateFlags state) {
  GtkBorder border;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get_border(context, &border);
#else
  gtk_style_context_get_border(context, state, &border);
#endif
  return InsetsFromGtkBorder(border);
}

gfx::Insets MarginFromStyleContext(GtkStyleContext* context,
                                   GtkStateFlags state) {
  GtkBorder margin;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get_margin(context, &margin);
#else
  gtk_style_context_get_margin(context, state, &margin);
#endif
  return InsetsFromGtkBorder(margin);
}

gfx::Size LoadNavButtonIcon(
    views::NavButtonProvider::FrameButtonDisplayType type,
    GtkStyleContext* button_context,
    int scale,
    NavButtonIcon* icon = nullptr) {
  const char* icon_name = IconNameFromButtonType(type);
#if GTK_CHECK_VERSION(3, 90, 0)
  ScopedGObject<GtkIconPaintable> icon_paintable(gtk_icon_theme_lookup_icon(
      GetDefaultIconTheme(), icon_name, nullptr, kNavButtonIconSize, scale,
      GTK_TEXT_DIR_NONE, static_cast<GtkIconLookupFlags>(0)));
  auto* paintable = GDK_PAINTABLE(icon_paintable);
  int width = gdk_paintable_get_intrinsic_width(paintable);
  int height = gdk_paintable_get_intrinsic_height(paintable);
  if (icon) {
    auto* snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable, snapshot, width, height);
    ScopedGObject<GskRenderNode> node(gtk_snapshot_free_to_node(snapshot));
    auto rect = GRAPHENE_RECT_INIT(0, 0, width, height);
    ScopedGObject<GskRenderer> renderer(gsk_cairo_renderer_new());
    *icon = NavButtonIcon(gsk_renderer_render_texture(renderer, node, &rect));
  }
  return {width, height};
#else
  ScopedGObject<GtkIconInfo> icon_info(gtk_icon_theme_lookup_icon_for_scale(
      GetDefaultIconTheme(), icon_name, kNavButtonIconSize, scale,
      static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_USE_BUILTIN |
                                      GTK_ICON_LOOKUP_GENERIC_FALLBACK)));
  NavButtonIcon icon_pixbuf(gtk_icon_info_load_symbolic_for_context(
      icon_info, button_context, nullptr, nullptr));
  gfx::Size size{gdk_pixbuf_get_width(icon_pixbuf),
                 gdk_pixbuf_get_height(icon_pixbuf)};
  if (icon)
    *icon = std::move(icon_pixbuf);
  return size;
#endif
}

gfx::Size GetMinimumWidgetSize(gfx::Size content_size,
                               GtkStyleContext* content_context,
                               GtkStyleContext* widget_context,
                               GtkStateFlags state) {
  gfx::Rect widget_rect = gfx::Rect(content_size);
  if (content_context)
    widget_rect.Inset(-MarginFromStyleContext(content_context, state));
#if !GTK_CHECK_VERSION(3, 90, 0)
  if (GtkCheckVersion(3, 20)) {
    int min_width, min_height;
    gtk_style_context_get(widget_context, state, "min-width", &min_width,
                          "min-height", &min_height, nullptr);
    widget_rect.set_width(std::max(widget_rect.width(), min_width));
    widget_rect.set_height(std::max(widget_rect.height(), min_height));
  }
#endif
  widget_rect.Inset(-PaddingFromStyleContext(widget_context, state));
  widget_rect.Inset(-BorderFromStyleContext(widget_context, state));
  return widget_rect.size();
}

ScopedStyleContext CreateHeaderContext(bool maximized) {
  std::string window_selector = "GtkWindow#window.background";
  if (maximized)
    window_selector += ".maximized";
  return AppendCssNodeToStyleContext(
      AppendCssNodeToStyleContext(nullptr, window_selector),
      "GtkHeaderBar#headerbar.header-bar.titlebar");
}

void CalculateUnscaledButtonSize(
    views::NavButtonProvider::FrameButtonDisplayType type,
    bool maximized,
    gfx::Size* button_size,
    gfx::Insets* button_margin) {
  // views::ImageButton expects the images for each state to be of the
  // same size, but GTK can, in general, use a differnetly-sized
  // button for each state.  For this reason, render buttons for all
  // states at the size of a GTK_STATE_FLAG_NORMAL button.
  auto button_context = AppendCssNodeToStyleContext(
      CreateHeaderContext(maximized),
      "GtkButton#button.titlebutton." +
          std::string(ButtonStyleClassFromButtonType(type)));

  auto icon_size = LoadNavButtonIcon(type, button_context, 1);

  auto image_context =
      AppendCssNodeToStyleContext(button_context, "GtkImage#image");
  gfx::Size image_size = GetMinimumWidgetSize(icon_size, nullptr, image_context,
                                              GTK_STATE_FLAG_NORMAL);

  *button_size = GetMinimumWidgetSize(image_size, image_context, button_context,
                                      GTK_STATE_FLAG_NORMAL);
  *button_margin =
      MarginFromStyleContext(button_context, GTK_STATE_FLAG_NORMAL);
}

class NavButtonImageSource : public gfx::ImageSkiaSource {
 public:
  NavButtonImageSource(views::NavButtonProvider::FrameButtonDisplayType type,
                       views::Button::ButtonState state,
                       bool maximized,
                       bool active,
                       gfx::Size button_size)
      : type_(type),
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
    if (button_size_.IsEmpty())
      return gfx::ImageSkiaRep();

    auto button_context = AppendCssNodeToStyleContext(
        CreateHeaderContext(maximized_), "GtkButton#button.titlebutton");
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
    cairo_pattern_t* cr_pattern = nullptr;
    cairo_surface_t* cr_surface = nullptr;
#if GTK_CHECK_VERSION(3, 90, 0)
    gtk_style_context_get(button_context, GTK_STYLE_PROPERTY_BACKGROUND_IMAGE,
                          &cr_pattern, nullptr);
#else
    gtk_style_context_get(button_context, button_state,
                          GTK_STYLE_PROPERTY_BACKGROUND_IMAGE, &cr_pattern,
                          nullptr);
#endif
    if (cr_pattern &&
        cairo_pattern_get_surface(cr_pattern, &cr_surface) ==
            CAIRO_STATUS_SUCCESS &&
        cr_surface &&
        cairo_surface_get_type(cr_surface) == CAIRO_SURFACE_TYPE_IMAGE &&
        (cairo_image_surface_get_width(cr_surface) > button_size_.width() ||
         cairo_image_surface_get_height(cr_surface) > button_size_.height())) {
      ApplyCssToContext(button_context,
                        ".titlebutton { background-size: contain; }");
    }
    cairo_pattern_destroy(cr_pattern);

    // Gtk doesn't support fractional scale factors, but chrome does.
    // Rendering the button background and border at a fractional
    // scale factor is easy, since we can adjust the cairo context
    // transform.  But the icon is loaded from a pixbuf, so we pick
    // the next-highest integer scale and manually downsize.
    int pixbuf_scale = scale == static_cast<int>(scale) ? scale : scale + 1;
    NavButtonIcon icon_pixbuf;
    auto icon_size =
        LoadNavButtonIcon(type_, button_context, pixbuf_scale, &icon_pixbuf);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(scale * button_size_.width(),
                          scale * button_size_.height());
    bitmap.eraseColor(0);

    CairoSurface surface(bitmap);
    cairo_t* cr = surface.cairo();

    cairo_save(cr);
    cairo_scale(cr, scale, scale);
    if (GtkCheckVersion(3, 11, 3) ||
        (button_state & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE))) {
      gtk_render_background(button_context, cr, 0, 0, button_size_.width(),
                            button_size_.height());
      gtk_render_frame(button_context, cr, 0, 0, button_size_.width(),
                       button_size_.height());
    }
    cairo_restore(cr);
    cairo_save(cr);
    float pixbuf_extra_scale = scale / pixbuf_scale;
    cairo_scale(cr, pixbuf_extra_scale, pixbuf_extra_scale);
    gtk_render_icon(
        button_context, cr, icon_pixbuf,
        ((pixbuf_scale * button_size_.width() - icon_size.width()) / 2),
        ((pixbuf_scale * button_size_.height() - icon_size.height()) / 2));
    cairo_restore(cr);

    return gfx::ImageSkiaRep(bitmap, scale);
  }

  bool HasRepresentationAtAllScales() const override { return true; }

 private:
  views::NavButtonProvider::FrameButtonDisplayType type_;
  views::Button::ButtonState state_;
  bool maximized_;
  bool active_;
  gfx::Size button_size_;
};

}  // namespace

NavButtonProviderGtk::NavButtonProviderGtk() = default;

NavButtonProviderGtk::~NavButtonProviderGtk() = default;

void NavButtonProviderGtk::RedrawImages(int top_area_height,
                                        bool maximized,
                                        bool active) {
  auto header_context = CreateHeaderContext(maximized);

  GtkBorder header_padding;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get_padding(header_context, &header_padding);
#else
  gtk_style_context_get_padding(header_context, GTK_STATE_FLAG_NORMAL,
                                &header_padding);
#endif

  double scale = 1.0f;
  std::map<views::NavButtonProvider::FrameButtonDisplayType, gfx::Size>
      button_sizes;
  std::map<views::NavButtonProvider::FrameButtonDisplayType, gfx::Insets>
      button_margins;
  std::vector<views::NavButtonProvider::FrameButtonDisplayType> display_types{
      views::NavButtonProvider::FrameButtonDisplayType::kMinimize,
      maximized ? views::NavButtonProvider::FrameButtonDisplayType::kRestore
                : views::NavButtonProvider::FrameButtonDisplayType::kMaximize,
      views::NavButtonProvider::FrameButtonDisplayType::kClose,
  };
  for (auto type : display_types) {
    CalculateUnscaledButtonSize(type, maximized, &button_sizes[type],
                                &button_margins[type]);
    int button_unconstrained_height = button_sizes[type].height() +
                                      button_margins[type].top() +
                                      button_margins[type].bottom();

    int needed_height = header_padding.top + button_unconstrained_height +
                        header_padding.bottom;

    if (needed_height > top_area_height)
      scale =
          std::min(scale, static_cast<double>(top_area_height) / needed_height);
  }

  top_area_spacing_ = InsetsFromGtkBorder(header_padding);
  top_area_spacing_ =
      gfx::Insets(std::round(scale * top_area_spacing_.top()),
                  std::round(scale * top_area_spacing_.left()),
                  std::round(scale * top_area_spacing_.bottom()),
                  std::round(scale * top_area_spacing_.right()));

  inter_button_spacing_ = std::round(scale * kHeaderSpacing);

  for (auto type : display_types) {
    double button_height =
        scale * (button_sizes[type].height() + button_margins[type].top() +
                 button_margins[type].bottom());
    double available_height =
        top_area_height - scale * (header_padding.top + header_padding.bottom);
    double scaled_button_offset = (available_height - button_height) / 2;

    gfx::Size size = button_sizes[type];
    size = gfx::Size(std::round(scale * size.width()),
                     std::round(scale * size.height()));
    gfx::Insets margin = button_margins[type];
    margin =
        gfx::Insets(std::round(scale * (header_padding.top + margin.top()) +
                               scaled_button_offset),
                    std::round(scale * margin.left()), 0,
                    std::round(scale * margin.right()));

    button_margins_[type] = margin;

    for (size_t state = 0; state < views::Button::STATE_COUNT; state++) {
      button_images_[type][state] = gfx::ImageSkia(
          std::make_unique<NavButtonImageSource>(
              type, static_cast<views::Button::ButtonState>(state), maximized,
              active, size),
          size);
    }
  }
}

gfx::ImageSkia NavButtonProviderGtk::GetImage(
    views::NavButtonProvider::FrameButtonDisplayType type,
    views::Button::ButtonState state) const {
  auto it = button_images_.find(type);
  DCHECK(it != button_images_.end());
  return it->second[state];
}

gfx::Insets NavButtonProviderGtk::GetNavButtonMargin(
    views::NavButtonProvider::FrameButtonDisplayType type) const {
  auto it = button_margins_.find(type);
  DCHECK(it != button_margins_.end());
  return it->second;
}

gfx::Insets NavButtonProviderGtk::GetTopAreaSpacing() const {
  return top_area_spacing_;
}

int NavButtonProviderGtk::GetInterNavButtonSpacing() const {
  return inter_button_spacing_;
}

}  // namespace gtk
