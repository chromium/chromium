// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
#define WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_

class GURL;

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace weblayer {
class Navigation;
class Page;

// An interface for a WebLayer embedder to get notified about navigations. For
// now this only notifies for the main frame.
//
// The lifecycle of a navigation:
// 1) A navigation is initiated, such as by NavigationController::Navigate()
//    or within the page
// 2) LoadStateChanged() first invoked
// 3) NavigationStarted
// 4) 0 or more NavigationRedirected
// 5) NavigationCompleted or NavigationFailed
// 6) Main frame completes loading, LoadStateChanged() last invoked
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
  // (|is_loading| is false). |should_show_loading_ui| will be true unless the
  // load is a fragment navigation, or triggered by
  // history.pushState/replaceState.
  virtual void LoadStateChanged(bool is_loading, bool should_show_loading_ui) {}

  // Indicates that the load progress of the page has changed. |progress|
  // ranges from 0.0 to 1.0.
  virtual void LoadProgressChanged(double progress) {}

  // This is fired after each navigation has completed to indicate that the
  // first paint after a non-empty layout has finished.
  virtual void OnFirstContentfulPaint() {}

  // Similar to OnFirstContentfulPaint but contains timing information from the
  // renderer process to better align with the Navigation Timing API.
  // |navigation_start| is the navigation start time.
  // |first_contentful_paint| is the duration to first contentful paint from
  // navigation start.
  virtual void OnFirstContentfulPaint(
      const base::TimeTicks& navigation_start,
      const base::TimeDelta& first_contentful_paint) {}

  // This is fired when the largest contentful paint page load metric is
  // available. |navigation_start| is the navigation start time.
  // |largest_contentful_paint| is the duration to largest contentful paint from
  // navigation start.
  virtual void OnLargestContentfulPaint(
      const base::TimeTicks& navigation_start,
      const base::TimeDelta& largest_contentful_paint) {}

  // Called after each navigation to indicate that the old page is no longer
  // being rendered. Note this is not ordered with respect to
  // OnFirstContentfulPaint.
  virtual void OnOldPageNoLongerRendered(const GURL& url) {}

  // Called when a Page is destroyed. For the common case, this is called when
  // the user navigates away from a page to a new one or when the Tab is
  // destroyed. However there are situations when a page is alive when it's not
  // visible, e.g. when it goes into the back-forward cache. In that case this
  // method will either be called when the back-forward cache entry is evicted
  // or if it is used then this cycle repeats.
  virtual void OnPageDestroyed(Page* page) {}

  // Called when the source language for |page| has been determined to be
  // |language|.
  // Note: |language| is an ISO 639 language code (two letters, except for
  // Chinese where a localization is necessary).
  virtual void OnPageLanguageDetermined(Page* page,
                                        const std::string& language) {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_OBSERVER_H_
