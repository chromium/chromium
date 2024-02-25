// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_FRAGMENT_H_
#define UI_VIEWS_CONTROLS_LINK_FRAGMENT_H_

#include <functional>
#include <type_traits>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/link.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"

namespace views {

// A `LinkFragment` can be used to represent a logical link that spans across
// multiple lines. Connected `LinkFragment`s adjust their style if any single
// one of them is hovered over of focused.
class VIEWS_EXPORT LinkFragment : public Link {
  METADATA_HEADER(LinkFragment, Link)

 public:
  explicit LinkFragment(const std::u16string& title = std::u16string(),
                        int text_context = style::CONTEXT_LABEL,
                        int text_style = style::STYLE_LINK,
                        LinkFragment* other_fragment = nullptr);
  ~LinkFragment() override;

  LinkFragment(const LinkFragment&) = delete;
  LinkFragment& operator=(const LinkFragment&) = delete;

 private:
  // Returns the short-circuiting logical-"or" of invoking `f` on all linked
  // fragments, beginning with `initial_fragment`.
  template <
      typename F,
      typename Fragment,  // Templated to allow const or non-const LinkFragments
      typename = std::enable_if_t<std::is_invocable_r_v<bool, F, Fragment*>>>
  static bool InvokeOnFragments(F&& f, Fragment* initial_fragment) {
    Fragment* fragment = initial_fragment;
    bool result = false;
    do {
      result = std::invoke(std::forward<F>(f), fragment);
      fragment = fragment->next_fragment_;
    } while (!result && fragment != initial_fragment);
    return result;
  }

  // Returns whether this fragment indicates that the entire link represented
  // by it should be underlined.
  bool IsUnderlined() const;

  // Connects `this` to the `other_fragment`.
  void Connect(LinkFragment* other_fragment);

  // Disconnects `this` from any other fragments that it may be connected to.
  void Disconnect();

  // Recalculates the font style for this link fragment and, if it is changed,
  // updates both this fragment and all other that are connected to it.
  void RecalculateFont() override;

  // Pointers to the previous and the next `LinkFragment` if the logical link
  // represented by `this` consists of multiple such fragments (e.g. due to
  // line breaks).
  // If the logical link is just a single `LinkFragment` component, then these
  // pointers point to `this`.
  raw_ptr<LinkFragment> prev_fragment_;
  raw_ptr<LinkFragment> next_fragment_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LINK_FRAGMENT_H_
