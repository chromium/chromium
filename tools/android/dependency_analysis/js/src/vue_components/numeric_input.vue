<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="numeric-input-container">
    <MdField
        class="input-field">
      <label :for="inputId">{{ description }}</label>
      <MdInput
          :id="inputId"
          v-model="internalInputValue"
          class="numeric-input-value"
          type="number"/>
    </MdField>
    <div class="button-group">
      <MdButton
          class="numeric-input-button md-icon-button md-dense"
          @click="internalInputValue++">
        <MdIcon>expand_less</MdIcon>
      </MdButton>
      <MdButton
          class="numeric-input-button md-icon-button md-dense"
          @click="internalInputValue--">
        <MdIcon>expand_more</MdIcon>
      </MdButton>
    </div>
  </div>
</template>

<script>
// @vue/component
const NumericInput = {
  props: {
    description: String,
    inputId: String,
    inputValue: Number,
    minValue: Number,
  },
  computed: {
    internalInputValue: {
      get: function() {
        return this.inputValue;
      },
      set: function(newValue, oldValue) {
        const validNewValue = Math.max(this.minValue, newValue);
        this.$emit('update:inputValue', validNewValue);
        // If `inputValue` is currently `minValue` and the user submits input <
        // `minValue`, `inputValue` will be updated to `minValue` (which, since
        // it already was `minValue`, does not trigger a rerender from Vue's
        // reactivity system). In these cases, we need to force an update to
        // sync the UI with the data.
        if (validNewValue === oldValue) {
          this.$forceUpdate();
        }
      },
    },
  },
};

export default NumericInput;
</script>

<style scoped>
.numeric-input-container {
  display: flex;
  flex-direction: row;
}

.button-group {
  align-items: center;
  display: flex;
  flex-direction: column;
}

.numeric-input-button {
  margin: 0;
}

.numeric-input-value {
  width: 100%;
}

.input-field {
  width: 50%;
}

input[type=number]::-webkit-inner-spin-button,
input[type=number]::-webkit-outer-spin-button {
  -webkit-appearance: none;
  margin: 0;
}
</style>
