<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <MdList class="md-double-line details-list">
    <MdListItem>
      <div class="list-item-entry">
        <!-- Ideally this is the first element under .md-list-item-text for a
             double-line layout, but vue-material doesn't recognize this
             component. Therefore it's pulled outside, and a placeholder <span/>
             is added so the second line would be properly styled. -->
        <LinkToGraph
            :filter="[selectedClass.packageName]"
            :graph-type="PagePathName.PACKAGE"
            :text="selectedClass.packageName"/>
        <div class="md-list-item-text">
          <span/>
          <span>Package Graph URL</span>
        </div>
      </div>
    </MdListItem>
    <MdListItem v-if="selectedClass.buildTargets.length > 0">
      <div class="md-list-item-text">
        <ul class="buildtarget-list">
          <li
              v-for="buildTarget in selectedClass.buildTargets"
              :key="buildTarget">
            {{ buildTarget }}
          </li>
        </ul>
        <span>Build Targets</span>
      </div>
    </MdListItem>
  </MdList>
</template>

<script>
import {PagePathName} from '../url_processor.js';

import LinkToGraph from './link_to_graph.vue';

// @vue/component
const ClassDetailsPanel = {
  components: {
    LinkToGraph,
  },
  props: {
    selectedClass: Object,
  },
  computed: {
    PagePathName: () => PagePathName,
  },
};

export default ClassDetailsPanel;
</script>

<style scoped>
.buildtarget-list {
  padding-left: 0;
}

.details-list {
  padding: 0;
}

.list-item-entry {
  width: 100%;
}
</style>
