<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="page-container">
    <v-dialog/>
    <div id="title-and-graph-container">
      <div
          id="title-and-metadata">
        <div
            id="title"
            class="md-headline">
          <a href="/">Clank Dependency Viewer</a> - Package Graph
        </div>
        <div
            id="graph-metadata-info">
          <GraphMetadataInfo
              :graph-metadata="graphMetadata"/>
        </div>
      </div>
      <GraphVisualization
          :graph-update-triggers="[
            displaySettingsData,
          ]"
          :page-model="pageModel"
          :display-settings-data="displaySettingsData"
          @[CUSTOM_EVENTS.NODE_CLICKED]="graphNodeClicked"
          @[CUSTOM_EVENTS.NODE_DOUBLE_CLICKED]="graphNodeDoubleClicked"/>
    </div>
    <div
        id="page-sidebar"
        class="md-elevation-3">
      <MdSubheader class="sidebar-subheader">
        Node Filter
      </MdSubheader>
      <GraphFilterItems
          id="graph-filter-items"
          :node-filter-data="displaySettingsData.nodeFilterData"
          :get-display-data="filterGetDisplayData"
          @[CUSTOM_EVENTS.FILTER_DELIST]="filterDelistNode"
          @[CUSTOM_EVENTS.FILTER_CHECK_ALL]="filterCheckAll"
          @[CUSTOM_EVENTS.FILTER_UNCHECK_ALL]="filterUncheckAll"
          @[CUSTOM_EVENTS.FILTER_DELIST_UNCHECKED]="filterDelistUnchecked"/>
      <GraphFilterInput
          :node-ids="pageModel.getNodeIds()"
          :nodes-already-in-filter="
            displaySettingsData.nodeFilterData.filterList"
          :get-short-name="filterGetShortName"
          @[CUSTOM_EVENTS.FILTER_SUBMITTED]="filterAddOrCheckNode"/>
      <MdSubheader class="sidebar-subheader">
        Display Options
      </MdSubheader>
      <div id="inbound-outbound-depth-inputs">
        <NumericInput
            description="Inbound Depth"
            input-id="inbound-input"
            :input-value.sync="displaySettingsData.inboundDepth"
            :min-value="0"/>
        <NumericInput
            description="Outbound Depth"
            input-id="outbound-input"
            :input-value.sync="displaySettingsData.outboundDepth"
            :min-value="0"/>
      </div>
      <GraphDisplayPanel
          :display-settings-data="displaySettingsData"
          :display-settings-preset.sync="
            displaySettingsData.displaySettingsPreset">
        <GraphDisplaySettings
            :display-settings-data="displaySettingsData"
            @[CUSTOM_EVENTS.DISPLAY_OPTION_CHANGED]="displayOptionChanged"/>
      </GraphDisplayPanel>
      <MdSubheader class="sidebar-subheader">
        Node Details
      </MdSubheader>
      <GraphSelectedNodeDetails
          :selected-node-details-data="pageModel.selectedNodeDetailsData"
          @[CUSTOM_EVENTS.DETAILS_CHECK_NODE]="filterAddOrCheckNode"
          @[CUSTOM_EVENTS.DETAILS_UNCHECK_NODE]="filterUncheckNode">
        <PackageDetailsPanel
            :selected-package="pageModel.selectedNodeDetailsData.selectedNode"/>
      </GraphSelectedNodeDetails>
    </div>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {PagePathName, UrlProcessor} from '../url_processor.js';

import {GraphNode} from '../graph_model.js';
import {PageModel} from '../page_model.js';
import {
  PackageDisplaySettingsData,
  DisplaySettingsPreset,
} from '../display_settings_data.js';
import {parsePackageGraphModelFromJson} from '../process_graph_json.js';
import {shortenPackageName} from '../chrome_hooks.js';

import GraphDisplayPanel from './graph_display_panel.vue';
import GraphDisplaySettings from './graph_display_settings.vue';
import GraphFilterInput from './graph_filter_input.vue';
import GraphFilterItems from './graph_filter_items.vue';
import GraphMetadataInfo from './graph_metadata_info.vue';
import GraphSelectedNodeDetails from './graph_selected_node_details.vue';
import GraphVisualization from './graph_visualization.vue';
import NumericInput from './numeric_input.vue';
import PackageDetailsPanel from './package_details_panel.vue';

// @vue/component
const PackageGraphPage = {
  components: {
    GraphDisplayPanel,
    GraphDisplaySettings,
    GraphFilterInput,
    GraphFilterItems,
    GraphMetadataInfo,
    GraphSelectedNodeDetails,
    GraphVisualization,
    NumericInput,
    PackageDetailsPanel,
  },
  props: {
    graphJson: Object,
    graphMetadata: Object,
  },

  /**
   * Various references to objects used across the entire package page.
   *
   * @typedef {object} PackagePageData
   * @property {!PageModel} pageModel The data store for the page.
   * @property {!PackageDisplaySettingsData} displaySettingsData Additional data
   *   store for the graph's display settings.
   * @property {PagePathName} pagePathName The pathname for the page.
   */

  /**
   * @return {PackagePageData} The objects used throughout the page.
   */
  data: function() {
    const graphModel = parsePackageGraphModelFromJson(this.graphJson);
    const pageModel = new PageModel(graphModel);
    const displaySettingsData = new PackageDisplaySettingsData();

    return {
      pageModel,
      displaySettingsData,
      pagePathName: PagePathName.PACKAGE,
    };
  },
  computed: {
    CUSTOM_EVENTS: () => CUSTOM_EVENTS,
  },
  watch: {
    displaySettingsData: {
      handler: function() {
        this.updateDocumentUrl();
      },
      deep: true,
    },
  },
  /**
   * Parses out data from the current URL to initialize the visualization with.
   */
  mounted: function() {
    const pageUrl = new URL(document.URL);
    const pageUrlProcessor = new UrlProcessor(pageUrl.searchParams);
    this.displaySettingsData.readUrlProcessor(pageUrlProcessor);

    if (this.displaySettingsData.nodeFilterData.filterList.length === 0) {
      // Default package to be displayed when the page is first loaded.
      [
        'org.chromium.chrome.browser.tab',
      ].forEach(nodeName => this.filterAddOrCheckNode(nodeName));
    }
  },
  methods: {
    displayOptionChanged: function() {
      this.displaySettingsData.displaySettingsPreset =
        DisplaySettingsPreset.CUSTOM;
    },
    updateDocumentUrl() {
      const urlProcessor = UrlProcessor.createForOutput();
      this.displaySettingsData.updateUrlProcessor(urlProcessor);

      const pageUrl = urlProcessor.getUrl(document.URL, PagePathName.PACKAGE);
      history.replaceState(null, '', pageUrl);
    },
    filterGetShortName: shortenPackageName,
    filterGetDisplayData: function(fullPackageName) {
      return {
        firstLine: shortenPackageName(fullPackageName),
        secondLine: '',
      };
    },
    filterDelistNode: function(nodeName) {
      this.displaySettingsData.nodeFilterData.delistNode(nodeName);
    },
    filterAddOrCheckNode: function(nodeName) {
      this.displaySettingsData.nodeFilterData.addOrFindNode(
          nodeName).checked = true;
    },
    filterUncheckNode: function(nodeName) {
      this.displaySettingsData.nodeFilterData.addOrFindNode(
          nodeName).checked = false;
    },
    filterCheckAll: function() {
      this.displaySettingsData.nodeFilterData.checkAll();
    },
    filterUncheckAll: function() {
      this.displaySettingsData.nodeFilterData.uncheckAll();
    },
    filterDelistUnchecked: function() {
      this.displaySettingsData.nodeFilterData.delistUnchecked();
    },
    /**
     * @param {number} depth The new inbound depth.
     */
    setInboundDepth: function(depth) {
      this.displaySettingsData.inboundDepth = depth;
    },
    /**
     * @param {number} depth The new outbound depth.
     */
    setOutboundDepth: function(depth) {
      this.displaySettingsData.outboundDepth = depth;
    },
    /**
     * @param {?GraphNode} node The selected node. May be `null`, which will
     *     reset the selection to the state with no node.
     */
    graphNodeClicked: function(node) {
      this.pageModel.selectedNodeDetailsData.selectedNode = node;
    },
    /**
     * @param {!GraphNode} node The double-clicked node.
     */
    graphNodeDoubleClicked: function(node) {
      if (node.visualizationState.selectedByFilter) {
        this.filterUncheckNode(node.id);
      } else {
        this.filterAddOrCheckNode(node.id);
      }
    },
  },
};

export default PackageGraphPage;
</script>

<style lang="scss">
@import "~vue-material/dist/theme/engine";

@include md-register-theme("default", (
  primary: #448aff,
  accent: #448aff,
));

@import "~vue-material/dist/theme/all";
</style>

<style scoped>

#title-and-metadata {
  align-items: flex-start;
  display: flex;
  flex-direction: row;
  justify-content: space-between;
}

#title, #graph-metadata-info {
  padding: 10px;
}

#page-container {
  display: flex;
  flex-direction: row;
  height: 100vh;
  width: 100vw;
}

#title-and-graph-container {
  display: flex;
  flex-direction: column;
  flex-grow: 1;
}

#page-sidebar {
  display: flex;
  flex-direction: column;
  flex-grow: 0;
  overflow-y: scroll;
  padding: 0 20px 20px 20px;
  width: 30vw;
}

.sidebar-subheader {
  padding: 0;
}

#graph-filter-items {
  margin-bottom: 10px;
}

#inbound-outbound-depth-inputs {
  display: flex;
  flex-direction: row;
}

#page-controls {
  display: flex;
  flex-direction: row;
  height: 15vh;
}
</style>
