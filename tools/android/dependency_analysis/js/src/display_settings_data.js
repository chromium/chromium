// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HullDisplay} from './class_view_consts.js';
import {UrlProcessor, URL_PARAM_KEYS} from './url_processor.js';

/**
 * Various different graph edge color schemes.
 *
 * @enum {string}
 */
const GraphEdgeColor = {
  DEFAULT: 'default',
  GREY_GRADIENT: 'grey-gradient',
  BLUE_TO_RED: 'blue-to-red',
};

/**
 * Various different display setting presets.
 *
 * @enum {string}
 */
const DisplaySettingsPreset = {
  CUSTOM: 'Custom',
  COLOR_ON_HOVER: 'Red-blue on hover',
  GREY_VIEW: 'Constant grey gradient',
};

// A map from DisplaySettingsPreset to display setting objects. The keys in each
// object should correspond to properties of NodeFilterData.
const PRESET_SETTINGS = {
  [DisplaySettingsPreset.CUSTOM]: {}, // Applying the custom preset is a no-op.
  [DisplaySettingsPreset.COLOR_ON_HOVER]: {
    'curveEdges': true,
    'colorOnlyOnHover': true,
    'graphEdgeColor': GraphEdgeColor.BLUE_TO_RED,
  },
  [DisplaySettingsPreset.GREY_VIEW]: {
    'curveEdges': false,
    'colorOnlyOnHover': false,
    'graphEdgeColor': GraphEdgeColor.GREY_GRADIENT,
  },
};

/**
 * Underlying data for node filtering. The UI shows a "filter list" that
 * displays nodes of interest, and each can be toggled on/off using a checkbox.
 * Each node is classified as:
 * 1. Ignored: Not in filter list; hidden in visualizer.
 * 2. Unchecked: In filter list; hidden in visualizer.
 * 3. Checked (Selected): In filter list; shown in visualizer.
 */
class NodeFilterData {
  constructor() {
    /**
     * @typedef {object} NodeFilterEntry An entry in the filter list.
     * @property {string} name The name of the node to be filtered.
     * @property {boolean} checked Whether the node is checked (selected). If
     *   true, then the node is shown in the visualizer.
     */

    /**
     * List of filter list entries, i.e., nodes in unchecked or checked state.
     *
     * @public {!Array<!NodeFilterEntry>}
     */
    this.filterList = [];
  }

  /**
   * Finds a node in the filter list, creating and adding one if necessary.
   *
   * @param {string} nodeName The name of the node to find.
   * @return {!NodeFilterEntry} The node's entry in the filter list.
   */
  addOrFindNode(nodeName) {
    const foundIndex = this.filterList.findIndex(
        filterEntry => filterEntry.name === nodeName);
    if (foundIndex >= 0) {
      return this.filterList[foundIndex];
    }
    const entryToAdd = {
      name: nodeName,
      checked: true,
    };
    this.filterList.push(entryToAdd);
    return entryToAdd;
  }

  /**
   * Delists a node from the filter list (i.e., set state to ignored) if it
   *   exists.
   *
   * @param {string} nodeName The name of the node to delist.
   */
  delistNode(nodeName) {
    const deleteIndex = this.filterList.findIndex(
        filterEntry => filterEntry.name === nodeName);
    if (deleteIndex >= 0) {
      this.filterList.splice(deleteIndex, 1);
    }
  }

  /**
   * Sets all nodes in the filter list to checked.
   */
  checkAll() {
    for (const filterEntry of this.filterList) {
      filterEntry.checked = true;
    }
  }

  /**
   * Sets all nodes in the filter list to unchecked.
   */
  uncheckAll() {
    for (const filterEntry of this.filterList) {
      filterEntry.checked = false;
    }
  }

  /**
   * Delists all unchecked nodes from the filter list.
   */
  delistUnchecked() {
    this.filterList = this.filterList.filter(
        filterEntry => filterEntry.checked);
  }

  /**
   * @return {!Set<string>} A set of nodes that are checked in the filter.
   */
  getSelectedNodeSet() {
    return new Set(this.filterList.filter(filterEntry => filterEntry.checked)
        .map(filterEntry => filterEntry.name));
  }
}

/** Data store containing graph display-related settings. */
class DisplaySettingsData {
  /** Sets up default values for display settings. */
  constructor() {
    /** @public {!DisplaySettingsPreset} */
    this.displaySettingsPreset = DisplaySettingsPreset.CUSTOM;
    /** @public {!NodeFilterData} */
    this.nodeFilterData = new NodeFilterData();
    /** @public {number} */
    this.inboundDepth = 0;
    /** @public {number} */
    this.outboundDepth = 1;
    /** @public {boolean} */
    this.curveEdges = true;
    /** @public {boolean} */
    this.colorOnlyOnHover = false;
    /** @public {string} */
    this.graphEdgeColor = GraphEdgeColor.GREY_GRADIENT;
  }

  /**
   * Applies a preset by copying all its properties to the current display
   * settings, overwriting existing values.
   *
   * @param {string} presetName The key of the preset to apply.
   */
  applyPreset(presetName) {
    Object.assign(this, PRESET_SETTINGS[presetName]);
  }

  /**
   * Updates a UrlProcessor with all contained data.
   *
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to update.
   */
  updateUrlProcessor(urlProcessor) {
    urlProcessor.append(URL_PARAM_KEYS.DISPLAY_SETTINGS_PRESET,
        this.displaySettingsPreset);
    urlProcessor.append(URL_PARAM_KEYS.INBOUND_DEPTH, this.inboundDepth);
    urlProcessor.append(URL_PARAM_KEYS.OUTBOUND_DEPTH, this.outboundDepth);
    urlProcessor.append(URL_PARAM_KEYS.CURVE_EDGES, this.curveEdges);
    urlProcessor.append(
        URL_PARAM_KEYS.COLOR_ONLY_ON_HOVER, this.colorOnlyOnHover);
    urlProcessor.append(URL_PARAM_KEYS.EDGE_COLOR, this.graphEdgeColor);
    if (this.nodeFilterData.filterList.length > 0) {
      urlProcessor.appendArray(URL_PARAM_KEYS.FILTER_NAMES,
          this.nodeFilterData.filterList.map(filterEntry => filterEntry.name));
      urlProcessor.appendArray(URL_PARAM_KEYS.FILTER_CHECKED,
          this.nodeFilterData.filterList.map(
              filterEntry => filterEntry.checked));
    }
  }

  /**
   * Reads all contained data from a UrlProcessor.
   *
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to read from.
   */
  readUrlProcessor(urlProcessor) {
    this.displaySettingsPreset = urlProcessor.getString(
        URL_PARAM_KEYS.DISPLAY_SETTINGS_PRESET, this.displaySettingsPreset);
    this.inboundDepth = urlProcessor.getInt(
        URL_PARAM_KEYS.INBOUND_DEPTH, this.inboundDepth);
    this.outboundDepth = urlProcessor.getInt(
        URL_PARAM_KEYS.OUTBOUND_DEPTH, this.outboundDepth);
    this.curveEdges = urlProcessor.getBool(
        URL_PARAM_KEYS.CURVE_EDGES, this.curveEdges);
    this.colorOnlyOnHover = urlProcessor.getBool(
        URL_PARAM_KEYS.COLOR_ONLY_ON_HOVER, this.colorOnlyOnHover);
    this.graphEdgeColor = urlProcessor.getString(
        URL_PARAM_KEYS.EDGE_COLOR, this.graphEdgeColor);

    const filterNames = urlProcessor.getArray(URL_PARAM_KEYS.FILTER_NAMES, []);
    const filterChecked = urlProcessor.getArray(
        URL_PARAM_KEYS.FILTER_CHECKED, []);
    for (const [filterIdx, filterName] of filterNames.entries()) {
      const filterEntry = this.nodeFilterData.addOrFindNode(filterName);
      // If there is no corresponding entry in `filterChecked` (e.g., if the
      // checked param is empty), use true as a default value.
      if (filterIdx < filterChecked.length) {
        const filterElemChecked = (filterChecked[filterIdx] === 'true');
        filterEntry.checked = filterElemChecked;
      } else {
        filterEntry.checked = true;
      }
    }
  }
}

/** Data store containing class graph display-related settings. */
class ClassDisplaySettingsData extends DisplaySettingsData {
  /** Sets up default values for display settings. */
  constructor() {
    super();
    /** @public {string} */
    this.hullDisplay = HullDisplay.BUILD_TARGET;
  }

  /**
   * Updates a UrlProcessor with all contained data.
   *
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to update.
   */
  updateUrlProcessor(urlProcessor) {
    super.updateUrlProcessor(urlProcessor);
    urlProcessor.append(URL_PARAM_KEYS.HULL_DISPLAY, this.hullDisplay);
  }

  /**
   * Reads all contained data from a UrlProcessor.
   *
   * @param {!UrlProcessor} urlProcessor The UrlProcessor to read from.
   */
  readUrlProcessor(urlProcessor) {
    super.readUrlProcessor(urlProcessor);
    this.hullDisplay = urlProcessor.getString(
        URL_PARAM_KEYS.HULL_DISPLAY, this.hullDisplay);
  }
}

/** Data store containing package graph display-related settings. */
class PackageDisplaySettingsData extends DisplaySettingsData {}

/** Data store containing target graph display-related settings. */
class TargetDisplaySettingsData extends DisplaySettingsData {}

export {
  ClassDisplaySettingsData,
  DisplaySettingsPreset,
  GraphEdgeColor,
  PackageDisplaySettingsData,
  TargetDisplaySettingsData,
};
