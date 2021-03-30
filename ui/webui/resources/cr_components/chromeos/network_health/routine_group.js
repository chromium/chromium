// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for a group of diagnostic routines.
 */

Polymer({
  is: 'routine-group',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * List of routines to display in the group.
     * @private {!Array<!Routine>}
     */
    routines: {
      type: Array,
      value: [],
    },

    /**
     * Localized name for the group of routines.
     * @private {String}
     */
    name: {
      type: String,
      value: '',
    },

    /**
     * Boolean flag if any routines in the group are running.
     * @private {Boolean}
     */
    running: {
      type: Boolean,
      computed: 'routinesRunning_(routines.*)',
    },

    /**
     * Boolean flag if the container is expanded.
     * @private {Boolean}
     */
    expanded: {
      type: Boolean,
      value: false,
    },

    /**
     * Boolean flag if icon representing the group result should be shown.
     * @private {Boolean}
     */
    showGroupIcon: {
      type: Boolean,
      computed: 'showGroupIcon_(running, expanded)',
    },
  },

  /**
   * Helper function to get the icon for a group of routines based on all of
   * their results.
   * @param {!PolymerDeepPropertyChange} routines
   * @return {string}
   * @private
   */
  getGroupIcon_(routines) {
    // Assume that all tests are complete and passing until proven otherwise.
    let complete = true;
    let failed = false;

    for (const routine of /** @type {!Array<!Routine>} */ (routines.base)) {
      if (!routine.result) {
        complete = false;
        continue;
      }

      switch (routine.result.verdict) {
        case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem:
          continue;
        case chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem:
          failed = true;
          break;
        case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun:
          complete = false;
          break;
      }
    }

    if (failed) {
      return Icons.TEST_FAILED;
    }
    if (!complete) {
      return Icons.TEST_NOT_RUN;
    }

    return Icons.TEST_PASSED;
  },

  /**
   * Determine if the group routine icon should be showing.
   * @param {boolean} running
   * @param {boolean} expanded
   * @return {boolean}
   * @private
   */
  showGroupIcon_(running, expanded) {
    return !running && !expanded;
  },

  /**
   * Helper function to get the icon for a routine based on the result.
   * @param {!RoutineResponse} result
   * @return {string}
   * @private
   */
  getRoutineIcon_(result) {
    if (!result) {
      return Icons.TEST_NOT_RUN;
    }

    switch (result.verdict) {
      case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem:
        return Icons.TEST_PASSED;
      case chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem:
        return Icons.TEST_FAILED;
      case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun:
        return Icons.TEST_NOT_RUN;
    }

    return Icons.TEST_NOT_RUN;
  },

  /**
   * Determine if any routines in the group are running.
   * @param {!PolymerDeepPropertyChange} routines
   * @return {boolean}
   * @private
   */
  routinesRunning_(routines) {
    for (const routine of /** @type {!Array<!Routine>} */ (routines.base)) {
      if (routine.running) {
        return true;
      }
    }
    return false;
  },
});
