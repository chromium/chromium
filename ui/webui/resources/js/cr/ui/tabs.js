// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * Returns the TabBox for a Tab or a TabPanel.
   * @param {cr.ui.Tab|cr.ui.Tabs|cr.ui.TabPanel} el The tab or tabpanel
   *     element.
   * @return {cr.ui.TabBox} The tab box if found.
   */
  function getTabBox(el) {
    return /** @type {cr.ui.TabBox} */ (findAncestor(el, function(node) {
      return node.tagName == 'TABBOX';
    }));
  }

  /**
   * Returns whether an element is a tab related object.
   * @param {HTMLElement} el The element whose tag is being checked
   * @return {boolean} Whether the element is a tab related element.
   */
  function isTabElement(el) {
    return el.tagName == 'TAB' || el.tagName == 'TABPANEL';
  }

  /**
   * Decorates all the children of an element.
   * @this {HTMLElement}
   */
  function decorateChildren() {
    const map = {
      TABBOX: TabBox,
      TABS: Tabs,
      TAB: Tab,
      TABPANELS: TabPanels,
      TABPANEL: TabPanel
    };

    Object.keys(map).forEach(function(tagName) {
      const children = this.getElementsByTagName(tagName);
      const constr = map[tagName];
      for (const child of children) {
        cr.ui.decorate(child, constr);
      }
    }.bind(this));
  }

  /**
   * Set hook for TabBox selectedIndex.
   * @param {number} selectedIndex The new selected index.
   * @this {cr.ui.TabBox}
   */
  function selectedIndexSetHook(selectedIndex) {
    let child, tabChild, element;
    element = this.querySelector('tabs');
    if (element) {
      let i;
      for (i = 0; child = element.children[i]; i++) {
        const isSelected = i == selectedIndex;
        child.selected = isSelected;

        // Update tabIndex for a11y
        child.setAttribute('tabindex', isSelected ? 0 : -1);

        // Update aria-selected attribute for a11y
        const firstSelection = !child.hasAttribute('aria-selected');
        child.setAttribute('aria-selected', isSelected);

        // Update focus, but don't override initial focus.
        if (isSelected && !firstSelection) {
          child.focus();
        }
      }
    }

    element = this.querySelector('tabpanels');
    if (element) {
      let i;
      for (i = 0; child = element.children[i]; i++) {
        child.selected = i == selectedIndex;
      }
    }
  }

  /**
   * Creates a new tabbox element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
  const TabBox = cr.ui.define('tabbox');

  TabBox.prototype = {
    __proto__: HTMLElement.prototype,
    decorate: function() {
      decorateChildren.call(this);
      this.addEventListener('selectedChange', this.handleSelectedChange_, true);
      this.selectedIndex = 0;
    },

    /**
     * Callback for when a Tab or TabPanel changes its selected property.
     * @param {Event} e The property change event.
     * @private
     */
    handleSelectedChange_: function(e) {
      const target = /** @type {cr.ui.Tab|cr.ui.TabPanel}} */ (e.target);
      if (e.newValue && isTabElement(target) && getTabBox(target) == this) {
        const index =
            Array.prototype.indexOf.call(target.parentElement.children, target);
        this.selectedIndex = index;
      }
    },

    selectedIndex_: -1
  };

  /**
   * The index of the selected tab or -1 if no tab is selected.
   * @type {number}
   */
  cr.defineProperty(
      TabBox, 'selectedIndex', cr.PropertyKind.JS, selectedIndexSetHook);

  /**
   * Creates a new tabs element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
  const Tabs = cr.ui.define('tabs');
  Tabs.prototype = {
    __proto__: HTMLElement.prototype,
    decorate: function() {
      decorateChildren.call(this);

      this.addEventListener('keydown', this.handleKeyDown_.bind(this));

      // Get (and initializes a focus outline manager.
      this.focusOutlineManager_ = cr.ui.FocusOutlineManager.forDocument(
          /** @type {!Document} */ (this.ownerDocument));
    },

    /**
     * Handle keydown to change the selected tab when the user presses the
     * arrow keys.
     * @param {Event} e The keyboard event.
     * @private
     */
    handleKeyDown_: function(e) {
      let delta = 0;
      switch (e.key) {
        case 'ArrowLeft':
        case 'ArrowUp':
          delta = -1;
          break;
        case 'ArrowRight':
        case 'ArrowDown':
          delta = 1;
          break;
      }

      if (!delta) {
        return;
      }

      const cs = this.ownerDocument.defaultView.getComputedStyle(this);
      if (cs.direction == 'rtl') {
        delta *= -1;
      }

      const count = this.children.length;
      const tabbox = getTabBox(this);
      const index = tabbox.selectedIndex;
      tabbox.selectedIndex = (index + delta + count) % count;

      // Show focus outline since we used the keyboard.
      this.focusOutlineManager_.visible = true;
    }
  };

  /**
   * Creates a new tab element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
  const Tab = cr.ui.define('tab');
  Tab.prototype = {
    __proto__: HTMLElement.prototype,
    decorate: function() {
      const self = this;
      this.addEventListener(cr.isMac ? 'click' : 'mousedown', function() {
        self.selected = true;
      });
    }
  };

  /**
   * Whether the tab is selected.
   * @type {boolean}
   */
  cr.defineProperty(Tab, 'selected', cr.PropertyKind.BOOL_ATTR);

  /**
   * Creates a new tabpanels element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
  const TabPanels = cr.ui.define('tabpanels');
  TabPanels.prototype = {
    __proto__: HTMLElement.prototype,
    decorate: decorateChildren
  };

  /**
   * Creates a new tabpanel element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLElement}
   */
  const TabPanel = cr.ui.define('tabpanel');
  TabPanel.prototype = {
    __proto__: HTMLElement.prototype,
    decorate: function() {}
  };

  /**
   * Whether the tab is selected.
   * @type {boolean}
   */
  cr.defineProperty(TabPanel, 'selected', cr.PropertyKind.BOOL_ATTR);

  return {
    TabBox: TabBox,
    Tabs: Tabs,
    Tab: Tab,
    TabPanels: TabPanels,
    TabPanel: TabPanel
  };
});
