/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../divider/divider.js';
import { LitElement } from 'lit';
import { Tab } from './tab.js';
/**
 * @fires change {Event} Fired when the selected tab changes. The target's
 * `activeTabIndex` or `activeTab` provide information about the selection
 * change. The change event is fired when a user interaction like a space/enter
 * key or click cause a selection change. The tab selection based on these
 * actions can be cancelled by calling preventDefault on the triggering
 * `keydown` or `click` event. --bubbles
 *
 * @example
 * // perform an action if a tab is clicked
 * tabs.addEventListener('change', (event: Event) => {
 *   if (event.target.activeTabIndex === 2)
 *     takeAction();
 *   }
 * });
 *
 * // prevent a click from triggering tab selection under some condition
 * tabs.addEventListener('click', (event: Event) => {
 *   if (notReady)
 *     event.preventDefault();
 *   }
 * });
 *
 */
export declare class Tabs extends LitElement {
    /**
     * The tabs of this tab bar.
     */
    readonly tabs: Tab[];
    /**
     * The currently selected tab, `null` only when there are no tab children.
     *
     * @export
     */
    get activeTab(): Tab | null;
    set activeTab(tab: Tab | null);
    /**
     * The index of the currently selected tab.
     *
     * @export
     */
    get activeTabIndex(): number;
    set activeTabIndex(index: number);
    /**
     * Whether or not to automatically select a tab when it is focused.
     */
    autoActivate: boolean;
    private readonly tabsScrollerElement;
    private readonly slotElement;
    private get focusedTab();
    private readonly internals;
    constructor();
    /**
     * Scrolls the toolbar, if overflowing, to the active tab, or the provided
     * tab.
     *
     * @param tabToScrollTo The tab that should be scrolled to. Defaults to the
     *     active tab.
     * @return A Promise that resolves after the tab has been scrolled to.
     */
    scrollToTab(tabToScrollTo?: Tab | null): Promise<void>;
    protected render(): import("lit-html").TemplateResult<1>;
    private handleTabClick;
    private activateTab;
    private updateFocusableTab;
    private handleKeydown;
    private handleKeyup;
    private handleFocusout;
    private handleSlotChange;
}
