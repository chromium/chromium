<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="hull-settings">
    <label>Group nodes by:</label>
    <div
        v-for="hullDisplay in HullDisplay"
        :key="hullDisplay"
        @change="displayOptionChanged">
      <MdRadio
          :id="hullDisplay"
          v-model="internalSelectedHullDisplay"
          class="md-primary hull-settings-option"
          type="radio"
          name="hullDisplayRadioButtons"
          :value="hullDisplay">
        {{ hullDisplay }}
      </MdRadio>
    </div>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';
import {HullDisplay} from '../class_view_consts.js';

// @vue/component
const ClassGraphHullSettings = {
  props: {
    selectedHullDisplay: String,
  },
  computed: {
    HullDisplay: () => HullDisplay,
    internalSelectedHullDisplay: {
      get: function() {
        return this.selectedHullDisplay;
      },
      set: function(newValue) {
        this.$emit('update:selectedHullDisplay', newValue);
      },
    },
  },
  methods: {
    displayOptionChanged: function() {
      this.$emit(CUSTOM_EVENTS.DISPLAY_OPTION_CHANGED);
    },
  },
};

export default ClassGraphHullSettings;
</script>

<style scoped>
#hull-settings {
  display: flex;
  flex-direction: column;
  margin-bottom: 10px;
  padding-top: 10px;
}

.hull-settings-option {
  margin: 0;
}
</style>
