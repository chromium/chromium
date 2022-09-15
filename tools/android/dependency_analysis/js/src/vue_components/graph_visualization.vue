<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <svg
      id="graph-svg"
      width="100%"
      height="100%"/>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {GraphView} from '../graph_view.js';

// @vue/component
const GraphVisualization = {
  props: {
    /**
     * `graphUpdateTriggers` is an array of model properties that trigger an
     * update in the graph.
     *
     * Background: The need to trigger updates make it hard to integrate
     * `graph_view.js` into the reactive Vue framework. This is solved with
     * `graphUpdateTriggers`, which lists the parts of `pageModel` to observe
     * and propagate updates to `graph_view`. This makes the graph "reactive" to
     * changes in members of `graphUpdateTriggers`.
     *
     * Note: Observing `pageModel` in entirety is undesirable since it would
     * lead to circular rerendering.
     */
    graphUpdateTriggers: Array,
    pageModel: Object,
    displaySettingsData: Object,
    getNodeGroup: {
      type: Function,
      default: () => null,
    },

  },
  watch: {
    graphUpdateTriggers: {
      handler: function() {
        const d3Data = this.pageModel.graphModel.getDataForD3(
            this.displaySettingsData.nodeFilterData.getSelectedNodeSet(),
            this.displaySettingsData.inboundDepth,
            this.displaySettingsData.outboundDepth,
        );
        this.graphView.registerGetNodeGroup(this.getNodeGroup);
        this.graphView.updateGraphData(d3Data);
        this.graphView.updateDisplaySettings(this.displaySettingsData);
      },
      deep: true,
    },
  },
  /**
   * Initializes the `GraphView` backing this visualization component. It's
   * important we initialize on `mounted`, since GraphView's constructor binds
   * to a DOM element which must exist at the time of construction.
   */
  mounted: function() {
    this.graphView = new GraphView();
    this.graphView.registerOnNodeClicked(
        node => this.$emit(CUSTOM_EVENTS.NODE_CLICKED, node));
    this.graphView.registerOnNodeDoubleClicked(
        node => this.$emit(CUSTOM_EVENTS.NODE_DOUBLE_CLICKED, node));
    this.graphView.registerGetNodeGroup(this.getNodeGroup);
  },
};

export default GraphVisualization;
</script>

<style>
svg text {
  font-family: Roboto;
}

.graph-hull-labels text {
  dominant-baseline: baseline;
  font-size: 10px;
  font-weight: bold;
  text-anchor: middle;
}

.graph-edges path.non-hovered-edge {
  opacity: 0.4;
}

.graph-nodes circle {
  stroke-width: 1.5px;
}

.graph-labels text {
  font-size: 12px;
}

.graph-labels text.non-hovered-text {
  opacity: 0.4;
}

.graph-nodes circle.locked {
  stroke-width: 3.5px;
}
</style>

<style scoped>
#graph-svg {
  background-color: #eee;
}
</style>
