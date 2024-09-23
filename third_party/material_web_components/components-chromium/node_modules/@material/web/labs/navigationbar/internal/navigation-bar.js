/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { __decorate } from "tslib";
import '../../../elevation/elevation.js';
import { html, LitElement, nothing } from 'lit';
import { property, queryAssignedElements } from 'lit/decorators.js';
import { mixinDelegatesAria } from '../../../internal/aria/delegate.js';
import { isRtl } from '../../../internal/controller/is-rtl.js';
// Separate variable needed for closure.
const navigationBarBaseClass = mixinDelegatesAria(LitElement);
/**
 * b/265346501 - add docs
 *
 * @fires navigation-bar-activated {CustomEvent<tab: NavigationTab, activeIndex: number>}
 * Dispatched whenever the `activeIndex` changes. --bubbles --composed
 */
export class NavigationBar extends navigationBarBaseClass {
    constructor() {
        super(...arguments);
        this.activeIndex = 0;
        this.hideInactiveLabels = false;
        this.tabs = [];
    }
    render() {
        // Needed for closure conformance
        const { ariaLabel } = this;
        return html `<div
      class="md3-navigation-bar"
      role="tablist"
      aria-label=${ariaLabel || nothing}
      @keydown="${this.handleKeydown}"
      @navigation-tab-interaction="${this.handleNavigationTabInteraction}"
      @navigation-tab-rendered=${this.handleNavigationTabConnected}
      ><md-elevation part="elevation"></md-elevation
      ><div class="md3-navigation-bar__tabs-slot-container"><slot></slot></div
    ></div>`;
    }
    updated(changedProperties) {
        if (changedProperties.has('activeIndex')) {
            this.onActiveIndexChange(this.activeIndex);
            this.dispatchEvent(new CustomEvent('navigation-bar-activated', {
                detail: {
                    tab: this.tabs[this.activeIndex],
                    activeIndex: this.activeIndex,
                },
                bubbles: true,
                composed: true,
            }));
        }
        if (changedProperties.has('hideInactiveLabels')) {
            this.onHideInactiveLabelsChange(this.hideInactiveLabels);
        }
        if (changedProperties.has('tabs')) {
            this.onHideInactiveLabelsChange(this.hideInactiveLabels);
            this.onActiveIndexChange(this.activeIndex);
        }
    }
    firstUpdated(changedProperties) {
        super.firstUpdated(changedProperties);
        this.layout();
    }
    layout() {
        if (!this.tabsElement)
            return;
        const navTabs = [];
        for (const node of this.tabsElement) {
            navTabs.push(node);
        }
        this.tabs = navTabs;
    }
    handleNavigationTabConnected(event) {
        const target = event.target;
        if (this.tabs.indexOf(target) === -1) {
            this.layout();
        }
    }
    handleNavigationTabInteraction(event) {
        this.activeIndex = this.tabs.indexOf(event.detail.state);
    }
    handleKeydown(event) {
        const key = event.key;
        const focusedTabIndex = this.tabs.findIndex((tab) => {
            return tab.matches(':focus-within');
        });
        const isRTL = isRtl(this);
        const maxIndex = this.tabs.length - 1;
        if (key === 'Enter' || key === ' ') {
            this.activeIndex = focusedTabIndex;
            return;
        }
        if (key === 'Home') {
            this.tabs[0].focus();
            return;
        }
        if (key === 'End') {
            this.tabs[maxIndex].focus();
            return;
        }
        const toNextTab = (key === 'ArrowRight' && !isRTL) || (key === 'ArrowLeft' && isRTL);
        if (toNextTab && focusedTabIndex === maxIndex) {
            this.tabs[0].focus();
            return;
        }
        if (toNextTab) {
            this.tabs[focusedTabIndex + 1].focus();
            return;
        }
        const toPreviousTab = (key === 'ArrowLeft' && !isRTL) || (key === 'ArrowRight' && isRTL);
        if (toPreviousTab && focusedTabIndex === 0) {
            this.tabs[maxIndex].focus();
            return;
        }
        if (toPreviousTab) {
            this.tabs[focusedTabIndex - 1].focus();
            return;
        }
    }
    onActiveIndexChange(value) {
        if (!this.tabs[value]) {
            throw new Error('NavigationBar: activeIndex is out of bounds.');
        }
        for (let i = 0; i < this.tabs.length; i++) {
            this.tabs[i].active = i === value;
        }
    }
    onHideInactiveLabelsChange(value) {
        for (const tab of this.tabs) {
            tab.hideInactiveLabel = value;
        }
    }
}
__decorate([
    property({ type: Number, attribute: 'active-index' })
], NavigationBar.prototype, "activeIndex", void 0);
__decorate([
    property({ type: Boolean, attribute: 'hide-inactive-labels' })
], NavigationBar.prototype, "hideInactiveLabels", void 0);
__decorate([
    queryAssignedElements({ flatten: true })
], NavigationBar.prototype, "tabsElement", void 0);
//# sourceMappingURL=navigation-bar.js.map