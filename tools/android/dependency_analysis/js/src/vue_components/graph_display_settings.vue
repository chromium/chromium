<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->
<!-- eslint-disable vue/no-mutating-props -->

<template>
  <div id="display-settings">
    <MdField class="display-settings-option">
      <label for="graph-edge-color">Graph edge color scheme:</label>
      <MdSelect
          id="graph-edge-color"
          v-model="displaySettingsData.graphEdgeColor"
          @md-selected="displayOptionChanged">
        <MdOption
            v-for="edgeColor in GraphEdgeColor"
            :key="edgeColor"
            :value="edgeColor">
          {{ edgeColor }}
        </MdOption>
      </MdSelect>
    </MdField>
    <MdCheckbox
        id="curve-edges"
        v-model="displaySettingsData.curveEdges"
        class="md-primary display-settings-option"
        type="checkbox"
        @change="displayOptionChanged">
      Curve graph edges
    </MdCheckbox>
    <MdCheckbox
        id="color-on-hover"
        v-model="displaySettingsData.colorOnlyOnHover"
        class="md-primary display-settings-option"
        type="checkbox"
        @change="displayOptionChanged">
      Color graph edges only on node hover
    </MdCheckbox>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {GraphEdgeColor} from '../display_settings_data.js';

// @vue/component
const GraphDisplaySettings = {
  props: {
    displaySettingsData: Object,
  },
  computed: {
    GraphEdgeColor: () => GraphEdgeColor,
  },
  methods: {
    displayOptionChanged: function() {
      this.$emit(CUSTOM_EVENTS.DISPLAY_OPTION_CHANGED);
    },
  },
};

export default GraphDisplaySettings;
</script>

<style scoped>
#display-settings {
  display: flex;
  flex-direction: column;
  margin-bottom: 10px;
}

.display-settings-option {
  margin: 5px 0;
}
</style>
