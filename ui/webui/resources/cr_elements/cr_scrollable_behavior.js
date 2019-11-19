// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for scrollable containers with <iron-list>.
 *
 * Any containers with the 'scrollable' attribute set will have the following
 * classes toggled appropriately: can-scroll, is-scrolled, scrolled-to-bottom.
 * These classes are used to style the container div and list elements
 * appropriately, see shared_style_css.html.
 *
 * The associated HTML should look something like:
 *   <div id="container" scrollable>
 *     <iron-list items="[[items]]" scroll-target="container">
 *       <template>
 *         <my-element item="[[item]] tabindex$="[[tabIndex]]"></my-element>
 *       </template>
 *     </iron-list>
 *   </div>
 *
 * In order to get correct keyboard focus (tab) behavior within the list,
 * any elements with tabbable sub-elements also need to set tabindex, e.g:
 *
 * <dom-module id="my-element>
 *   <template>
 *     ...
 *     <paper-icon-button toggles active="{{opened}}" tabindex$="[[tabindex]]">
 *   </template>
 * </dom-module>
 *
 * NOTE: If 'container' is not fixed size, it is important to call
 * updateScrollableContents() when [[items]] changes, otherwise the container
 * will not be sized correctly.
 */

/** @polymerBehavior */
const CrScrollableBehavior = {

  /** @private {number|null} */
  intervalId_: null,

  ready: function() {
    const readyAsync = () => {
      this.requestUpdateScroll();

      // Listen to the 'scroll' event for each scrollable container.
      const scrollableElements = this.root.querySelectorAll('[scrollable]');
      for (let i = 0; i < scrollableElements.length; i++) {
        scrollableElements[i].addEventListener(
            'scroll', this.updateScrollEvent_.bind(this));
      }
    };

    // TODO(dpapad): Remove Polymer 1 codepath when Polymer 2 migration has
    // completed.
    if (Polymer.DomIf) {
      Polymer.RenderStatus.beforeNextRender(this, readyAsync);
      return;
    }
    readyAsync();
  },

  detached: function() {
    if (this.intervalId_ !== null) {
      clearInterval(this.intervalId_);
    }
  },

  /**
   * Called any time the contents of a scrollable container may have changed.
   * This ensures that the <iron-list> contents of dynamically sized
   * containers are resized correctly.
   */
  updateScrollableContents: function() {
    if (this.intervalId_ !== null) {
      return;
    }  // notifyResize is already in progress.

    this.requestUpdateScroll();

    const nodeList = this.root.querySelectorAll('[scrollable] iron-list');
    if (!nodeList.length) {
      return;
    }

    let nodesToResize = Array.from(nodeList).map(node => ({
                                                   node: node,
                                                   lastScrollHeight: 0,
                                                 }));
    // Use setInterval to avoid initial render / sizing issues.
    this.intervalId_ = window.setInterval(() => {
      const checkAgain = [];
      nodesToResize.forEach(({node, lastScrollHeight}) => {
        const scrollHeight = node.parentNode.scrollHeight;
        // A hidden scroll-container has a height of 0. When not hidden, it has
        // a min-height of 1px and the iron-list needs a resize to show the
        // initial items and update the |scrollHeight|. The initial item count
        // is determined by the |scrollHeight|. A scrollHeight of 1px will
        // result in the minimum default item count (currently 3). After the
        // |scrollHeight| is updated to be greater than 1px, another resize is
        // needed to correctly calculate the number of physical iron-list items
        // to render.
        if (scrollHeight != lastScrollHeight) {
          const ironList = /** @type {!IronListElement} */ (node);
          ironList.notifyResize();
        }
        if (scrollHeight <= 1) {
          checkAgain.push({
            node: node,
            lastScrollHeight: scrollHeight,
          });
        }
      });
      if (checkAgain.length == 0) {
        window.clearInterval(this.intervalId_);
        this.intervalId_ = null;
      } else {
        nodesToResize = checkAgain;
      }
    }, 10);
  },

  /**
   * Setup the initial scrolling related classes for each scrollable container.
   * Called from ready() and updateScrollableContents(). May also be called
   * directly when the contents change (e.g. when not using iron-list).
   */
  requestUpdateScroll: function() {
    requestAnimationFrame(function() {
      const scrollableElements = this.root.querySelectorAll('[scrollable]');
      for (let i = 0; i < scrollableElements.length; i++) {
        this.updateScroll_(/** @type {!HTMLElement} */ (scrollableElements[i]));
      }
    }.bind(this));
  },

  /** @param {!IronListElement} list */
  saveScroll: function(list) {
    // Store a FIFO of saved scroll positions so that multiple updates in a
    // frame are applied correctly. Specifically we need to track when '0' is
    // saved (but not apply it), and still handle patterns like [30, 0, 32].
    list.savedScrollTops = list.savedScrollTops || [];
    list.savedScrollTops.push(list.scrollTarget.scrollTop);
  },

  /** @param {!IronListElement} list */
  restoreScroll: function(list) {
    this.async(function() {
      const scrollTop = list.savedScrollTops.shift();
      // Ignore scrollTop of 0 in case it was intermittent (we do not need to
      // explicitly scroll to 0).
      if (scrollTop != 0) {
        list.scroll(0, scrollTop);
      }
    });
  },

  /**
   * Event wrapper for updateScroll_.
   * @param {!Event} event
   * @private
   */
  updateScrollEvent_: function(event) {
    const scrollable = /** @type {!HTMLElement} */ (event.target);
    this.updateScroll_(scrollable);
  },

  /**
   * This gets called once initially and any time a scrollable container
   * scrolls.
   * @param {!HTMLElement} scrollable
   * @private
   */
  updateScroll_: function(scrollable) {
    scrollable.classList.toggle(
        'can-scroll', scrollable.clientHeight < scrollable.scrollHeight);
    scrollable.classList.toggle('is-scrolled', scrollable.scrollTop > 0);
    scrollable.classList.toggle(
        'scrolled-to-bottom',
        scrollable.scrollTop + scrollable.clientHeight >=
            scrollable.scrollHeight);
  },
};
