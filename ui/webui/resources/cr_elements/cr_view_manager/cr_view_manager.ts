// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrLazyRenderLitElement} from '../cr_lazy_render/cr_lazy_render_lit.js';

import {getCss} from './cr_view_manager.css.js';
import {getHtml} from './cr_view_manager.html.js';

function getEffectiveView<T extends HTMLElement>(
    element: CrLazyRenderLitElement<T>|T): HTMLElement {
  return element.matches('cr-lazy-render, cr-lazy-render-lit') ?
      (element as CrLazyRenderLitElement<T>).get() :
      element;
}

function dispatchCustomEvent(element: Element, eventType: string) {
  element.dispatchEvent(
      new CustomEvent(eventType, {bubbles: true, composed: true}));
}

const viewAnimations: Map<string, (element: Element) => Promise<Animation>> =
    new Map();
viewAnimations.set('fade-in', element => {
  const animation = element.animate([{opacity: 0}, {opacity: 1}], {
    duration: 180,
    easing: 'ease-in-out',
    iterations: 1,
  });

  return animation.finished;
});
viewAnimations.set('fade-out', element => {
  const animation = element.animate([{opacity: 1}, {opacity: 0}], {
    duration: 180,
    easing: 'ease-in-out',
    iterations: 1,
  });

  return animation.finished;
});
viewAnimations.set('slide-in-fade-in-ltr', element => {
  const animation = element.animate(
      [
        {transform: 'translateX(-8px)', opacity: 0},
        {transform: 'translateX(0)', opacity: 1},
      ],
      {
        duration: 300,
        easing: 'cubic-bezier(0.0, 0.0, 0.2, 1)',
        fill: 'forwards',
        iterations: 1,
      });

  return animation.finished;
});
viewAnimations.set('slide-in-fade-in-rtl', element => {
  const animation = element.animate(
      [
        {transform: 'translateX(8px)', opacity: 0},
        {transform: 'translateX(0)', opacity: 1},
      ],
      {
        duration: 300,
        easing: 'cubic-bezier(0.0, 0.0, 0.2, 1)',
        fill: 'forwards',
        iterations: 1,
      });

  return animation.finished;
});

export class CrViewManagerElement extends CrLitElement {
  static get is() {
    return 'cr-view-manager';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private exit_(element: HTMLElement, animation: string): Promise<void> {
    const animationFunction = viewAnimations.get(animation);
    element.classList.remove('active');
    element.classList.add('closing');
    dispatchCustomEvent(element, 'view-exit-start');
    if (!animationFunction) {
      // Nothing to animate. Immediately resolve.
      element.classList.remove('closing');
      dispatchCustomEvent(element, 'view-exit-finish');
      return Promise.resolve();
    }
    return animationFunction(element).then(() => {
      element.classList.remove('closing');
      dispatchCustomEvent(element, 'view-exit-finish');
    });
  }

  private enter_(view: HTMLElement, animation: string): Promise<void> {
    const animationFunction = viewAnimations.get(animation);
    const effectiveView = getEffectiveView(view);
    effectiveView.classList.add('active');
    dispatchCustomEvent(effectiveView, 'view-enter-start');
    if (!animationFunction) {
      // Nothing to animate. Immediately resolve.
      dispatchCustomEvent(effectiveView, 'view-enter-finish');
      return Promise.resolve();
    }
    return animationFunction(effectiveView).then(() => {
      dispatchCustomEvent(effectiveView, 'view-enter-finish');
    });
  }

  switchView(
      newViewId: string, enterAnimation?: string,
      exitAnimation?: string): Promise<void> {
    return this.switchViews([newViewId], enterAnimation, exitAnimation);
  }

  // Each view should have 'position: initial' for being able to show multiple
  // views at the same time.
  switchViews(
      newViewIds: string[], enterAnimation?: string,
      exitAnimation?: string): Promise<void> {
    let previousViews = new Set(this.querySelectorAll<HTMLElement>('.active'));
    let newViews = new Set(
        newViewIds.length === 0 ?
            [] :
            this.querySelectorAll<HTMLElement>(
                newViewIds.map(id => `#${id}`).join(',')));
    assert(newViews.size === newViewIds.length);

    // Calculate views that are already active, and remove them from both
    // `previousViews` and `newViews` as they don't need to be exited/entered
    // again.
    const commonViews = previousViews.intersection(newViews);
    previousViews = previousViews.difference(commonViews);
    newViews = newViews.difference(commonViews);

    const promises = [];

    for (const view of previousViews) {
      promises.push(this.exit_(view, exitAnimation || 'fade-out'));
    }
    for (const view of newViews) {
      promises.push(this.enter_(
          view,
          enterAnimation ||
              (previousViews.size === 0 ? 'no-animation' : 'fade-out')));
    }

    return Promise.all(promises).then(() => {});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-view-manager': CrViewManagerElement;
  }
}

customElements.define(CrViewManagerElement.is, CrViewManagerElement);
