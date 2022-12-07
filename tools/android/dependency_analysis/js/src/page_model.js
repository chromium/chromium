// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraphModel} from './graph_model.js';

/**
 * A container containing the page-wide state. This is the single source of
 * truth, parts of the container will be observed by Vue components on the page.
 */
class PageModel {
  /**
   * @param {!GraphModel} graphModel The graph data to visualize.
   */
  constructor(graphModel) {
    /** @public {!GraphModel} */
    this.graphModel = graphModel;

    /**
     * The data for the selected node details UI component.
     *
     * @typedef {object} SelectedNodeDetailsData
     * @property {?Node} selectedNode The selected node, if it exists.
     */
    /** @public {!SelectedNodeDetailsData} */
    this.selectedNodeDetailsData = {
      selectedNode: null,
    };
  }

  /**
   * Gets the ids of all the nodes in the graph.
   *
   * @return {!Array<string>} An array with the all node ids.
   */
  getNodeIds() {
    return [...this.graphModel.nodes.keys()];
  }
}

export {
  PageModel,
};
