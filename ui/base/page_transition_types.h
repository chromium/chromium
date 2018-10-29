// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PAGE_TRANSITION_TYPES_H_
#define UI_BASE_PAGE_TRANSITION_TYPES_H_

#include <stdint.h>

#include "ui/base/ui_base_export.h"

namespace ui {

// Types of transitions between pages. These are stored in the history
// database to separate visits, and are reported by the renderer for page
// navigations.
//
// WARNING: don't change these numbers. They are written directly into the
// history database, so future versions will need the same values to match
// the enums.
//
// A type is made of a core value and a set of qualifiers. A type has one
// core value and 0 or or more qualifiers.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
enum PageTransition {
  PAGE_TRANSITION_FIRST = 0,

  // User got to this page by clicking a link on another page.
  PAGE_TRANSITION_LINK = PAGE_TRANSITION_FIRST,

  // User got this page by typing the URL in the URL bar.  This should not be
  // used for cases where the user selected a choice that didn't look at all
  // like a URL; see GENERATED below.
  //
  // We also use this for other "explicit" navigation actions.
  PAGE_TRANSITION_TYPED = 1,

  // User got to this page through a suggestion in the UI, for example)
  // through the destinations page.
  PAGE_TRANSITION_AUTO_BOOKMARK = 2,

  // This is a subframe navigation. This is any content that is automatically
  // loaded in a non-toplevel frame. For example, if a page consists of
  // several frames containing ads, those ad URLs will have this transition
  // type. The user may not even realize the content in these pages is a
  // separate frame, so may not care about the URL (see MANUAL below).
  PAGE_TRANSITION_AUTO_SUBFRAME = 3,

  // For subframe navigations that are explicitly requested by the user and
  // generate new navigation entries in the back/forward list. These are
  // probably more important than frames that were automatically loaded in
  // the background because the user probably cares about the fact that this
  // link was loaded.
  PAGE_TRANSITION_MANUAL_SUBFRAME = 4,

  // User got to this page by typing in the URL bar and selecting an entry
  // that did not look like a URL.  For example, a match might have the URL
  // of a Google search result page, but appear like "Search Google for ...".
  // These are not quite the same as TYPED navigations because the user
  // didn't type or see the destination URL.
  // See also KEYWORD.
  PAGE_TRANSITION_GENERATED = 5,

  // This is a toplevel navigation. This is any content that is automatically
  // loaded in a toplevel frame.  For example, opening a tab to show the ASH
  // screen saver, opening the devtools window, opening the NTP after the safe
  // browsing warning, opening web-based dialog boxes are examples of
  // AUTO_TOPLEVEL navigations.
  PAGE_TRANSITION_AUTO_TOPLEVEL = 6,

  // The user filled out values in a form and submitted it. NOTE that in
  // some situations submitting a form does not result in this transition
  // type. This can happen if the form uses script to submit the contents.
  PAGE_TRANSITION_FORM_SUBMIT = 7,

  // The user "reloaded" the page, either by hitting the reload button or by
  // hitting enter in the address bar.  NOTE: This is distinct from the
  // concept of whether a particular load uses "reload semantics" (i.e.
  // bypasses cached data).  For this reason, lots of code needs to pass
  // around the concept of whether a load should be treated as a "reload"
  // separately from their tracking of this transition type, which is mainly
  // used for proper scoring for consumers who care about how frequently a
  // user typed/visited a particular URL.
  //
  // SessionRestore and undo tab close use this transition type too.
  PAGE_TRANSITION_RELOAD = 8,

  // The url was generated from a replaceable keyword other than the default
  // search provider. If the user types a keyword (which also applies to
  // tab-to-search) in the omnibox this qualifier is applied to the transition
  // type of the generated url. TemplateURLModel then may generate an
  // additional visit with a transition type of KEYWORD_GENERATED against the
  // url 'http://' + keyword. For example, if you do a tab-to-search against
  // wikipedia the generated url has a transition qualifer of KEYWORD, and
  // TemplateURLModel generates a visit for 'wikipedia.org' with a transition
  // type of KEYWORD_GENERATED.
  PAGE_TRANSITION_KEYWORD = 9,

  // Corresponds to a visit generated for a keyword. See description of
  // KEYWORD for more details.
  PAGE_TRANSITION_KEYWORD_GENERATED = 10,

  // ADDING NEW CORE VALUE? Be sure to update the LAST_CORE and CORE_MASK
  // values below.  Also update CoreTransitionString().
  PAGE_TRANSITION_LAST_CORE = PAGE_TRANSITION_KEYWORD_GENERATED,
  PAGE_TRANSITION_CORE_MASK = 0xFF,

  // Qualifiers
  // Any of the core values above can be augmented by one or more qualifiers.
  // These qualifiers further define the transition.

  // A managed user attempted to visit a URL but was blocked.
  PAGE_TRANSITION_BLOCKED = 0x00800000,

  // User used the Forward or Back button to navigate among browsing history.
  PAGE_TRANSITION_FORWARD_BACK = 0x01000000,

  // User used the address bar to trigger this navigation.
  PAGE_TRANSITION_FROM_ADDRESS_BAR = 0x02000000,

  // User is navigating to the home page.
  PAGE_TRANSITION_HOME_PAGE = 0x04000000,

  // The transition originated from an external application; the exact
  // definition of this is embedder dependent.
  PAGE_TRANSITION_FROM_API = 0x08000000,

  // The beginning of a navigation chain.
  PAGE_TRANSITION_CHAIN_START = 0x10000000,

  // The last transition in a redirect chain.
  PAGE_TRANSITION_CHAIN_END = 0x20000000,

  // Redirects caused by JavaScript or a meta refresh tag on the page.
  PAGE_TRANSITION_CLIENT_REDIRECT = 0x40000000,

  // Redirects sent from the server by HTTP headers. It might be nice to
  // break this out into 2 types in the future, permanent or temporary, if we
  // can get that information from WebKit.
  PAGE_TRANSITION_SERVER_REDIRECT = 0x80000000,

  // Used to test whether a transition involves a redirect.
  PAGE_TRANSITION_IS_REDIRECT_MASK = 0xC0000000,

  // General mask defining the bits used for the qualifiers.
  PAGE_TRANSITION_QUALIFIER_MASK = 0xFFFFFF00,
};

// Compares two PageTransition types ignoring qualifiers. |rhs| is taken to
// be a compile time constant, and hence must not contain any qualifiers.
UI_BASE_EXPORT bool PageTransitionCoreTypeIs(PageTransition lhs,
                                             PageTransition rhs);

// Compares two PageTransition types including qualifiers. Rarely useful,
// PageTransitionCoreTypeIs() is more likely what you need.
UI_BASE_EXPORT bool PageTransitionTypeIncludingQualifiersIs(PageTransition lhs,
                                                            PageTransition rhs);

// Simplifies the provided transition by removing any qualifier
UI_BASE_EXPORT PageTransition PageTransitionStripQualifier(
    PageTransition type);

UI_BASE_EXPORT bool PageTransitionIsValidType(int32_t type);

UI_BASE_EXPORT PageTransition PageTransitionFromInt(int32_t type);

// Returns true if the given transition is a top-level frame transition, or
// false if the transition was for a subframe.
UI_BASE_EXPORT bool PageTransitionIsMainFrame(PageTransition type);

// Returns whether a transition involves a redirection
UI_BASE_EXPORT bool PageTransitionIsRedirect(PageTransition type);

// Returns whether a transition is a new navigation (rather than a return
// to a previously committed navigation).
UI_BASE_EXPORT bool PageTransitionIsNewNavigation(PageTransition type);

// Return the qualifier
UI_BASE_EXPORT int32_t PageTransitionGetQualifier(PageTransition type);

// Returns true if the transition can be triggered by the web instead of
// through UI or similar.
UI_BASE_EXPORT bool PageTransitionIsWebTriggerable(PageTransition type);

// Return a string version of the core type values.
UI_BASE_EXPORT const char* PageTransitionGetCoreTransitionString(
    PageTransition type);

// Declare a dummy class that is intentionally never defined.
class DontUseOperatorEquals;

// Ban operator== and operator!= as it's way too easy to forget to strip the
// qualifiers. Use PageTransitionCoreTypeIs() instead or, in rare cases,
// PageTransitionTypeIncludingQualifiersIs().
DontUseOperatorEquals operator==(PageTransition, PageTransition);
DontUseOperatorEquals operator==(PageTransition, int);
DontUseOperatorEquals operator==(int, PageTransition);
DontUseOperatorEquals operator!=(PageTransition, PageTransition);
DontUseOperatorEquals operator!=(PageTransition, int);
DontUseOperatorEquals operator!=(int, PageTransition);

}  // namespace ui

#endif  // UI_BASE_PAGE_TRANSITION_TYPES_H_
