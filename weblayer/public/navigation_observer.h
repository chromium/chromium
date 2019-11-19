// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
#define WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_

namespace weblayer {
class Navigation;

// An interface for a WebLayer embedder to get notified about navigations. For
// now this only notifies for the main frame.
//
// The lifecycle of a navigation:
// 1) A navigation is initiated, such as by NavigationController::Navigate()
//    or within the page
// 2) LoadStateChanged() first invoked
// 3) NavigationStarted
// 4) 0 or more NavigationRedirected
// 5) 0 or 1 ReadyToCommitNavigation
// 6) NavigationCompleted or NavigationFailed
// 7) Main frame completes loading, LoadStateChanged() last invoked
class NavigationObserver {
 public:
  virtual ~NavigationObserver() {}

  // Called when a navigation started in the Tab. |navigation| is
  // unique to a specific navigation. The same |navigation| will be  provided on
  // subsequent calls to NavigationRedirected, NavigationCommitted,
  // NavigationCompleted and NavigationFailed when related to this navigation.
  // Observers should clear any references to |navigation| in
  // NavigationCompleted or NavigationFailed, just before it is destroyed.
  //
  // Note that this is only fired by navigations in the main frame.
  //
  // Note that this is fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use Navigation::IsSameDocument.
  //
  // Note that more than one navigation can be ongoing in the Tab
  // at the same time. Each will get its own Navigation object.
  //
  // Note that there is no guarantee that NavigationCompleted/NavigationFailed
  // will be called for any particular navigation before NavigationStarted is
  // called on the next.
  virtual void NavigationStarted(Navigation* navigation) {}

  // Called when a navigation encountered a server redirect.
  virtual void NavigationRedirected(Navigation* navigation) {}

  // Called when the navigation is ready to be committed in a renderer. This
  // occurs when the response code isn't 204/205 (which tell the browser that
  // the request is successful but there's no content that follows) or a
  // download (either from a response header or based on mime sniffing the
  // response). The browser then is ready to switch rendering the new document.
  // Most observers should use NavigationCompleted or NavigationFailed instead,
  // which happens right after the navigation commits. This method is for
  // observers that want to initialize renderer-side state just before the
  // Tab commits the navigation.
  //
  // This is the first point in time where a Tab is associated
  // with the navigation.
  virtual void ReadyToCommitNavigation(Navigation* navigation) {}

  // Called when a navigation completes successfully in the Tab.
  //
  // The document load will still be ongoing in the Tab. Use the
  // document loads events such as OnFirstContentfulPaint and related methods to
  // listen for continued events from this Tab.
  //
  // Note that this is fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use NavigationHandle::IsSameDocument.
  //
  // Note that |navigation| will be destroyed at the end of this call, so do not
  // keep a reference to it afterward.
  virtual void NavigationCompleted(Navigation* navigation) {}

  // Called when a navigation aborts in the Tab.
  //
  // Note that |navigation| will be destroyed at the end of this call, so do not
  // keep a reference to it afterward.
  virtual void NavigationFailed(Navigation* navigation) {}

  // Indicates that loading has started (|is_loading| is true) or is done
  // (|is_loading| is false). |to_different_document| will be true unless the
  // load is a fragment navigation, or triggered by
  // history.pushState/replaceState.
  virtual void LoadStateChanged(bool is_loading, bool to_different_document) {}

  // Indicates that the load progress of the page has changed. |progress|
  // ranges from 0.0 to 1.0.
  virtual void LoadProgressChanged(double progress) {}

  // This is fired after each navigation has completed to indicate that the
  // first paint after a non-empty layout has finished.
  virtual void OnFirstContentfulPaint() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
