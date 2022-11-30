<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="filter-items-container">
    <div id="controls">
      <MdButton
          class="md-primary md-raised md-dense"
          @click="checkAll">
        Check All
      </MdButton>
      <MdButton
          class="md-primary md-raised md-dense"
          @click="uncheckAll">
        Uncheck All
      </MdButton>
      <MdButton
          class="md-primary md-raised md-dense"
          @click="delistUnchecked">
        Delist Unchecked
      </MdButton>
    </div>
    <MdList
        id="filter-list"
        class="md-scrollbar md-double-line">
      <MdListItem
          v-for="(node, index) in filterList"
          :key="node.name">
        <MdButton
            class="numeric-input-button md-icon-button md-dense"
            @click="delistFromFilter(node.name)">
          <MdIcon>clear</MdIcon>
        </MdButton>
        <MdCheckbox
            v-model="node.checked"
            class="md-primary"/>
        <div class="filter-items-text md-list-item-text">
          <div>{{ filterListDisplayData[index].firstLine }}</div>
          <div v-if="filterListDisplayData[index].secondLine !== ''">
            {{ filterListDisplayData[index].secondLine }}
          </div>
        </div>
      </MdListItem>
    </MdList>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

// @vue/component
const GraphFilterItems = {
  props: {
    nodeFilterData: Object,
    getDisplayData: Function,
  },
  data: function() {
    return this.nodeFilterData;
  },
  computed: {
    /**
     * Computes an array of display values such that filterListDisplayData[i]
     * corresponds to filterList[i].
     *
     * This is needed instead of computing a new array to iterate over in the
     * template since checkboxes are v-model bound to `node.checked` in the
     * template. If a new array were computed, the binding would be on the
     * elements of the new array instead of `filterList`.
     *
     * @return {!Array<{firstLine: string, secondLine: string}>} Text lines to
     *     display in UI.
     */
    filterListDisplayData: function() {
      return this.filterList.map(node => this.getDisplayData(node.name));
    },
  },
  methods: {
    delistFromFilter: function(nodeName) {
      this.$emit(CUSTOM_EVENTS.FILTER_DELIST, nodeName);
    },
    checkAll: function() {
      this.$emit(CUSTOM_EVENTS.FILTER_CHECK_ALL);
    },
    uncheckAll: function() {
      this.$emit(CUSTOM_EVENTS.FILTER_UNCHECK_ALL);
    },
    delistUnchecked: function() {
      this.$emit(CUSTOM_EVENTS.FILTER_DELIST_UNCHECKED);
    },
  },
};

export default GraphFilterItems;
</script>

<style scoped>
ul {
  list-style-type: none;
}

#filter-items-container {
  display: flex;
  flex-direction: column;
  margin-right: 20px;
  min-width: 100px;
}

.filter-items-text{
  display: inline-block;
  margin-left: 15px;
  white-space: normal;
  width: 100%;
  word-wrap: break-word;
}

#filter-list {
  max-height: 30vh;
  overflow-y: scroll;
}

#filter-list >>> .md-list-item-content {
  min-height: 0;
  padding: 0;
}

#controls {
  display: flex;
  flex-direction: row;
}
</style>
