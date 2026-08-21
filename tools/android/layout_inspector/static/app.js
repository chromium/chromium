// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** MainElements ********/
/**
 * Cached registry of all permanent DOM nodes accessed by the UI.
 */
class MainElements {
  constructor() {
    const getById = (id) => document.getElementById(id);
    this.btnLoad = getById('btn-load');
    this.divInfoBar = getById('div-info-bar');
    this.divMain = getById('div-main');
    this.divOverlay = getById('div-overlay');
    this.divPaneScreenshot = getById('div-pane-screenshot');
    this.divScreenshot = getById('div-screenshot');
  }
}

/******** MainVis ********/
/**
 * Orchestrates top-level visual state spanning multiple visualizers. Modifies
 * the DOM but has no layout logic.
 */
class MainVis {
  constructor(el, model) {
    this.el = el;
    this.model = model;

    this.screenshotVis =
        new ScreenshotVis(this.el.divScreenshot, this.model.visOpts);
    this.infoBarVis = new InfoBarVis(this.el.divInfoBar);
    this.overlayVis = new OverlayVis(this.el.divOverlay);
  }

  clearUI() {
    this.screenshotVis.clear();
  }
}

/******** MainController ********/
/**
 * Root coordinator mapping DOM events to model state changes and visual
 * updates to child controllers.
 */
class MainController {
  constructor(model, vis) {
    this.model = model;
    this.vis = vis;
    this.el = this.vis.el;

    this.hintCtrl = new HintController(this.vis.infoBarVis);
  }

  // Event Handlers - File Operations/Markup
  async handleLoad() {
    this.vis.clearUI();
    this.vis.overlayVis.show('overlay-loading', 'Loading...');
    try {
      await this.model.fetchData();

      await this.vis.screenshotVis.drawScreenshot(this.model.screenshotBlob,
                                                  this.model.visOpts);
    } catch (error) {
      console.error('Failed to load screen data:', error);
      alert(`An error occurred: ${error.message}`);
    } finally {
      this.vis.overlayVis.hide();
    }
  }

  bindAll() {
    this.el.btnLoad.addEventListener('click', () => this.handleLoad());
    this.el.btnLoad.addEventListener('mouseover', () => {
      this.hintCtrl.setHint(HINT.CTRL_LOAD);
    });
    this.el.btnLoad.addEventListener('mouseout', () => {
      this.hintCtrl.clear();
    });
  }

  init() {
    this.bindAll();
    this.el.btnLoad.focus();
  }
}

/******** Initialization ********/

document.addEventListener('DOMContentLoaded', () => {
  const el = new MainElements();
  const model = new MainModel();
  const vis = new MainVis(el, model);
  const ctrl = new MainController(model, vis);
  ctrl.init();
});
