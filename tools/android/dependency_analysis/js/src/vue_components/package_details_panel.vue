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
            :filter="packageClassNames"
            :graph-type="PagePathName.CLASS"
            :text="selectedPackage.displayName"/>
        <div class="md-list-item-text">
          <span/>
          <span>Full Class Graph URL</span>
        </div>
      </div>
    </MdListItem>
    <MdListItem>
      <div class="list-item-entry">
        <!-- Ideally this is the first element under .md-list-item-text for a
             double-line layout, but vue-material doesn't recognize this
             component. Therefore it's pulled outside, and a placeholder <span/>
             is added so the second line would be properly styled. -->
        <MdList
            id="class-list"
            class="md-scrollbar">
          <MdListItem
              v-for="classObj in packageClassObjects"
              :key="classObj.name">
            <LinkToGraph
                class="class-list-entry"
                :filter="[classObj.name]"
                :graph-type="PagePathName.CLASS"
                :text="classObj.shortName"/>
          </MdListItem>
        </MdList>
        <div class="md-list-item-text">
          <span/>
          <span>Individual Class Graph URLs</span>
        </div>
      </div>
    </MdListItem>
  </MdList>
</template>

<script>
import {PagePathName} from '../url_processor.js';
import {shortenClassName} from '../chrome_hooks.js';

import LinkToGraph from './link_to_graph.vue';

// @vue/component
const PackageDetailsPanel = {
  components: {
    LinkToGraph,
  },
  props: {
    selectedPackage: Object,
  },
  computed: {
    PagePathName: () => PagePathName,
    packageClassNames: function() {
      return this.selectedPackage.classNames;
    },
    packageClassObjects: function() {
      return this.selectedPackage.classNames.map(className => {
        return {
          name: className,
          shortName: shortenClassName(className),
        };
      });
    },
  },
};

export default PackageDetailsPanel;
</script>

<style scoped>
#class-list {
  max-height: 300px;
  overflow-y: scroll;
}

#class-list >>> .md-list-item-content {
  min-height: 0;
  padding: 0;
}

.class-list-entry {
  width: 100%;
}

.details-list {
  padding: 0;
}

.list-item-entry {
  display: flex;
  flex-direction: column;
  width: 100%;
}
</style>
