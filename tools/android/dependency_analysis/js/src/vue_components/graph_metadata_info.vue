<!-- Copyright 2020 The Chromium Authors
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <MdButton
      id="info-button"
      class="md-icon-button md-dense"
      @click="openMetadataModal">
    <MdIcon>info</MdIcon>
  </MdButton>
</template>

<script>

// @vue/component
const GraphMetadataInfo = {
  props: {
    graphMetadata: Object,
  },
  methods: {
    openMetadataModal: function() {
      this.$modal.show('dialog', {
        title: 'Graph Metadata',
        text: `<p>Commit Hash: ${this.graphMetadata.commit_hash}</p>` +
            `<p>Commit Time: ${this.graphMetadata.commit_time}</p>` +
            `<p>Cr-Commit-Position: ${this.graphMetadata.commit_cr_position}` +
            `</p>`,
        buttons: [
          {
            title: 'Close',
            handler: () => {
              this.$modal.hide('dialog');
            },
          },
        ],
      });
    },
  },
};

export default GraphMetadataInfo;
</script>

<style scoped>
</style>
