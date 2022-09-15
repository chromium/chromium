<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="display-panel">
    <div id="preset-container">
      <MdField id="preset-select-container">
        <label for="preset-select">Display Preset</label>
        <MdSelect
            id="preset-select"
            v-model="internalDisplaySettingsPreset"
            @md-selected="applySelectedPreset">
          <MdOption
              v-for="presetName in DisplaySettingsPreset"
              :key="presetName"
              :value="presetName">
            {{ presetName }}
          </MdOption>
        </MdSelect>
      </MdField>
      <MdButton
          class="md-primary md-raised md-dense"
          @click="settingsExpanded = !settingsExpanded">
        {{ settingsExpanded ? 'Hide' : 'Show' }} Advanced
      </MdButton>
    </div>
    <div
        v-if="settingsExpanded"
        id="advanced-panel">
      <slot/>
    </div>
  </div>
</template>

<script>
import {DisplaySettingsPreset} from '../display_settings_data.js';

// @vue/component
const GraphDisplaySettings = {
  props: {
    displaySettingsData: Object,
    displaySettingsPreset: String,
  },
  data: function() {
    return {
      settingsExpanded: false,
    };
  },
  computed: {
    DisplaySettingsPreset: () => DisplaySettingsPreset,
    internalDisplaySettingsPreset: {
      get: function() {
        return this.displaySettingsPreset;
      },
      set: function(newValue) {
        this.$emit('update:displaySettingsPreset', newValue);
      },
    },
  },
  methods: {
    applySelectedPreset() {
      // nextTick is needed here since we need to wait for the parent/child data
      // sync on `displaySettingsPreset` to finish.
      this.$nextTick(() => this.displaySettingsData.applyPreset(
          this.internalDisplaySettingsPreset));
    },
  },
};

export default GraphDisplaySettings;
</script>

<style scoped>
#preset-container {
  align-items: baseline;
  display: flex;
  flex-direction: row;
  justify-content: space-between;
}

#preset-select-container {
  margin-bottom: 0;
  width: 60%;
}

#advanced-panel {
  margin: 0 20px;
}

#display-panel {
  display: flex;
  flex-direction: column;
  margin-bottom: 10px;
}
</style>
