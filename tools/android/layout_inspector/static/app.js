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
    this.divControls = getById('div-controls');
    this.divInfoBar = getById('div-info-bar');
    this.divMain = getById('div-main');
    this.divMainSplitter = getById('div-main-splitter');
    this.divOverlay = getById('div-overlay');
    this.divPaneScreenshot = getById('div-pane-screenshot');
    this.divScreenshot = getById('div-screenshot');
    this.divScreenshotInfo = getById('div-screenshot-info');
    this.divViewTree = getById('div-view-tree');
    this.selZoom = getById('sel-zoom');
  }
}

/******** MainVis ********/
/**
 * Orchestrates top-level visual state spanning multiple visualizers (like the
 * Screenshot, or View Tree). Modifies the DOM but has no layout logic.
 */
class MainVis {
  constructor(el, model) {
    this.el = el;
    this.model = model;

    this.infoBarVis = new InfoBarVis(this.el.divInfoBar);
    this.overlayVis = new OverlayVis(this.el.divOverlay);
  }

  populateZoomOptions() {
    this.el.selZoom.innerHTML = '';
    ZOOM_LEVELS.forEach((level, index) => {
      const option = document.createElement('option');
      option.text = level.title;
      option.value = index;
      this.el.selZoom.appendChild(option);
    });
    this.el.selZoom.value = ZOOM_LEVEL_DEFAULT_INDEX;
  }
}

/******** MainController ********/
/**
 * Root coordinator mapping DOM events to model state changes and cascading
 * layout or visual updates to child controllers.
 */
class MainController {
  constructor(model, vis) {
    this.model = model;
    this.vis = vis;
    this.el = this.vis.el;

    this.hintCtrl = new HintController(this.vis.infoBarVis);

    this.layoutCtrl = new LayoutController(
        this.model, this.el.divMain, this.el.divPaneScreenshot,
        this.el.divMainSplitter, this.el.divScreenshot,
        this.el.divScreenshotInfo, this.hintCtrl);

    this.screenshotCtrl = new ScreenshotController(
        this.model, this.el.divScreenshot, this.el.divScreenshotInfo);

    this.treeCtrl = new TreeController(this.model, this.el.divViewTree);
  }

  clearUI() {
    this.screenshotCtrl.clear();
    this.treeCtrl.clear();
  }

  // Event Handlers - File Operations/Markup
  async handleLoad() {
    this.clearUI();
    this.vis.overlayVis.show('overlay-loading', 'Loading...');
    await this.model.unload();

    try {
      await this.model.load();  // Also updates `visOpts`.

      // Load the currently selected zoom level.
      this.model.visOpts.setZoomIndex(parseInt(this.el.selZoom.value, 10));

      // Initializing screenshot also updates zoom.
      this.screenshotCtrl.initScreenshot();

      const {wDims} = this.model.visOpts;
      this.layoutCtrl.setLayoutMode(LayoutMode.fromVector(-wDims.h, -wDims.w));

      // The model is the source of truth for view data.
      this.treeCtrl.populate();

    } catch (error) {
      console.error('Failed to load screen data:', error);
      alert(`An error occurred: ${error.message}`);
    } finally {
      this.vis.overlayVis.hide();
    }
  }

  bindAll() {
    this.el.btnLoad.addEventListener('click', () => this.handleLoad());
    this.el.selZoom.addEventListener('change', (e) => {
      this.model.visOpts.setZoomIndex(parseInt(e.target.value, 10));
      this.screenshotCtrl.updateZoom();
    });

    // Delegated hover hints for controls.
    this.el.divControls.addEventListener('mouseover', (e) => {
      const target = e.target.closest('[data-hint]');
      if (target) {
        const hintKey = target.getAttribute('data-hint');
        if (HINT[hintKey]) this.hintCtrl.setHint(HINT[hintKey]);
      }
    });
    this.el.divControls.addEventListener('mouseout', (e) => {
      if (!e.relatedTarget || !this.el.divControls.contains(e.relatedTarget)) {
        this.hintCtrl.clear();
      }
    });
  }

  init() {
    this.vis.populateZoomOptions();
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
