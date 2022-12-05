// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import TargetGraphPage from './vue_components/target_graph_page.vue';
import {loadGraph} from './load_graph.js';

import Vue from 'vue';
import {
  MdButton,
  MdCheckbox,
  MdDivider,
  MdField,
  MdIcon,
  MdList,
  // MdMenu is a dependency of MdField's MdSelect, see
  // https://github.com/vuematerial/vue-material/issues/1974
  MdMenu,
  MdRadio,
  MdSubheader,
} from 'vue-material/dist/components';

import 'vue-material/dist/vue-material.min.css';

import VModal from 'vue-js-modal';

document.addEventListener('DOMContentLoaded', () => {
  loadGraph().then(data => {
    Vue.use(MdButton);
    Vue.use(MdCheckbox);
    Vue.use(MdDivider);
    Vue.use(MdField);
    Vue.use(MdIcon);
    Vue.use(MdList);
    Vue.use(MdMenu);
    Vue.use(MdRadio);
    Vue.use(MdSubheader);

    Vue.use(VModal, {dialog: true});

    new Vue({
      el: '#target-graph-page',
      render: createElement => createElement(
          TargetGraphPage,
          {
            props: {
              graphJson: data.target_graph,
              graphMetadata: data.build_metadata,
            },
          },
      ),
    });
  }).catch(e => {
    document.write('Error loading graph.');
  });
});
