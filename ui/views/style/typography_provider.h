// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_
#define UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_

#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"
#include "ui/views/views_export.h"

namespace gfx {
class FontList;
}

namespace views {

// Provides a default provider of fonts to use in toolkit-views UI.
class VIEWS_EXPORT TypographyProvider {
 public:
  static const TypographyProvider& Get();

  TypographyProvider() = default;

  TypographyProvider(const TypographyProvider&) = delete;
  TypographyProvider& operator=(const TypographyProvider&) = delete;

  virtual ~TypographyProvider() = default;

  // Convenience method for getting a `FontList` corresponding to the details
  // from `GetFontDetails()`.
  const gfx::FontList& GetFont(int context, int style) const;

  // Public APIs. These assert the context and style validity and then invoke
  // the protected virtual `Impl` methods below. This allows subclasses to
  // override the implementations without having to do any common preamble.
  ui::ResourceBundle::FontDetails GetFontDetails(int context, int style) const;
  ui::ColorId GetColorId(int context, int style) const;
  int GetLineHeight(int context, int style) const;

 protected:
  // Returns the weight that will result in the ResourceBundle returning an
  // appropriate "medium" weight for UI. This caters for systems that are known
  // to be unable to provide a system font with weight other than NORMAL or BOLD
  // and for user configurations where the NORMAL font is already BOLD. In both
  // of these cases, NORMAL is returned instead.
  static gfx::Font::Weight MediumWeightForUI();

  // Returns whether the provided `context` and `style` combination is legal.
  virtual bool StyleAllowedForContext(int context, int style) const;

  // Implementations of the public API methods, which can assume the context and
  // style values are valid.
  virtual ui::ResourceBundle::FontDetails GetFontDetailsImpl(int context,
                                                             int style) const;
  virtual ui::ColorId GetColorIdImpl(int context, int style) const;
  virtual int GetLineHeightImpl(int context, int style) const;

 private:
  // `CHECK`s that the provided `context` and `style` are a legal combination of
  // values inside the expected ranges.
  void AssertContextAndStyleAreValid(int context, int style) const;
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_
