<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <a
      class="link-to-graph-container"
      :href="url">
    <img
        class="link-to-graph-icon"
        :src="graphIcon">
    <div class="link-to-graph-text">
      {{ text }}
    </div>
  </a>
</template>

<script>
import {PagePathName, UrlProcessor, URL_PARAM_KEYS} from '../url_processor.js';

import TargetGraphIcon from '../assets/target_graph_icon.png';
import PackageGraphIcon from '../assets/package_graph_icon.png';
import ClassGraphIcon from '../assets/class_graph_icon.png';

const GRAPH_NAME_TO_ICON = {
  [PagePathName.TARGET]: TargetGraphIcon,
  [PagePathName.PACKAGE]: PackageGraphIcon,
  [PagePathName.CLASS]: ClassGraphIcon,
};

// @vue/component
const LinkToGraph = {
  props: {
    filter: Array,
    graphType: String,
    text: String,
  },
  computed: {
    graphIcon: function() {
      return GRAPH_NAME_TO_ICON[this.graphType] || '';
    },
    url: function() {
      const urlProcessor = UrlProcessor.createForOutput();
      urlProcessor.appendArray(URL_PARAM_KEYS.FILTER_NAMES, this.filter);
      return urlProcessor.getUrl(document.URL, this.graphType);
    },
  },
};

export default LinkToGraph;
</script>

<style scoped>
.link-to-graph-container {
  align-items: center;
  /**
   * !important because the vue-material theme specifically styles
   * `.md-theme-default a` elements (md-theme-default is a class on the root
   * <html/>) and is imported later by webpack, giving it precedence over any
   * class selectors.
   */
  color: #448aff !important;
  display: flex;
  flex-direction: row;
}

.link-to-graph-container:hover {
  color: #448aff !important;
}

.link-to-graph-icon {
  height: 24px;
  margin-right: 10px;
  width: 24px;
}

.link-to-graph-text {
  color: #448aff;
  min-width: 0;
  white-space: normal;
  word-wrap: break-word;
}
</style>
