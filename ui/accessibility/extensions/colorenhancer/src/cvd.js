// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Global exports.  Used by popup to show effect of filter during setup.
 */
(function(exports) {
  let curDelta = 0;
  let curSeverity = 0;
  let curType = 'PROTANOMALY';
  let curSimulate = false;
  let curEnable = false;
  let curFilter = 0;
  const cssContent = `
html[cvd="0"] {
  -webkit-filter: url('#cvd_extension_0');
}
html[cvd="1"] {
  -webkit-filter: url('#cvd_extension_1');
}
`;

  /** @const {string} */
  const SVG_DEFAULT_MATRIX =
    '1 0 0 0 0 ' +
    '0 1 0 0 0 ' +
    '0 0 1 0 0 ' +
    '0 0 0 1 0';

  const svgContent = `
<svg xmlns="http://www.w3.org/2000/svg" version="1.1">
  <defs>
    <filter x="0" y="0" width="99999" height="99999" id="cvd_extension_0">
      <feColorMatrix id="cvd_matrix_0" type="matrix" values="
          ${SVG_DEFAULT_MATRIX}"/>
    </filter>
    <filter x="0" y="0" width="99999" height="99999" id="cvd_extension_1">
      <feColorMatrix id="cvd_matrix_1" type="matrix" values="
          ${SVG_DEFAULT_MATRIX}"/>
    </filter>
  </defs>
</svg>
`;

  // ======= CVD parameters =======
  /**
   * Parameters for simulating color vision deficiency.
   * Source:
   *     http://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/CVD_Simulation.html
   * Original Research Paper:
   *     http://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/Machado_Oliveira_Fernandes_CVD_Vis2009_final.pdf
   *
   * @enum {string}
   */
  const cvdSimulationParams = {
    PROTANOMALY: [
      [0.4720, -1.2946, 0.9857],
      [-0.6128, 1.6326, 0.0187],
      [0.1407, -0.3380, -0.0044],
      [-0.1420, 0.2488, 0.0044],
      [0.1872, -0.3908, 0.9942],
      [-0.0451, 0.1420, 0.0013],
      [0.0222, -0.0253, -0.0004],
      [-0.0290, -0.0201, 0.0006],
      [0.0068, 0.0454, 0.9990]
    ],
    DEUTERANOMALY: [
      [0.5442, -1.1454, 0.9818],
      [-0.7091, 1.5287, 0.0238],
      [0.1650, -0.3833, -0.0055],
      [-0.1664, 0.4368, 0.0056],
      [0.2178, -0.5327, 0.9927],
      [-0.0514, 0.0958, 0.0017],
      [0.0180, -0.0288, -0.0006],
      [-0.0232, -0.0649, 0.0007],
      [0.0052, 0.0360, 0.9998]
    ],
    TRITANOMALY: [
      [0.4275, -0.0181, 0.9307],
      [-0.2454, 0.0013, 0.0827],
      [-0.1821, 0.0168, -0.0134],
      [-0.1280, 0.0047, 0.0202],
      [0.0233, -0.0398, 0.9728],
      [0.1048, 0.0352, 0.0070],
      [-0.0156, 0.0061, 0.0071],
      [0.3841, 0.2947, 0.0151],
      [-0.3685, -0.3008, 0.9778]
    ]
  };


  // TODO(mustaq): This should be nuked, see getCvdCorrectionMatrix().
  const cvdCorrectionParams = {
    PROTANOMALY: {
      addendum: Matrix3x3.fromData(
          [[0.0, 0.0, 0.0], [0.7, 1.0, 0.0], [0.7, 0.0, 1.0]]),
      delta_factor: Matrix3x3.fromData(
          [[0.0, 0.0, 0.0], [0.3, 0.0, 0.0], [-0.3, 0.0, 0.0]])
    },
    DEUTERANOMALY: {
      addendum: Matrix3x3.fromData(
          [[0.0, 0.0, 0.0], [0.7, 1.0, 0.0], [0.7, 0.0, 1.0]]),
      delta_factor: Matrix3x3.fromData(
          [[0.0, 0.0, 0.0], [0.3, 0.0, 0.0], [-0.3, 0.0, 0.0]])
    },
    TRITANOMALY: {
      addendum: Matrix3x3.fromData(
          [[1.0, 0.0, 0.7], [0.0, 1.0, 0.7], [0.0, 0.0, 0.0]]),
      delta_factor: Matrix3x3.fromData(
          [[0.0, 0.0, 0.3], [0.0, 0.0, -0.3], [0.0, 0.0, 0.0]])
    }
  };


  // =======  CVD matrix builders =======

  /**
   * Returns a 3x3 matrix for simulating the given type of CVD with the given
   * severity.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @param {number} severity A real number in [0,1] denoting severity.
   * @return {!Matrix3x3}
   */
  function getCvdSimulationMatrix(cvdType, severity) {
    const cvdSimulationParam = cvdSimulationParams[cvdType];
    const severity_squared = severity * severity;

    const calculateElementValue = (i, j) => {
      const paramRow = i*3+j;
      return cvdSimulationParam[paramRow][0] * severity_squared
           + cvdSimulationParam[paramRow][1] * severity
           + cvdSimulationParam[paramRow][2];
    };
    return Matrix3x3.fromElementwiseConstruction(calculateElementValue);
  }


  /**
   * Returns a 3x3 matrix for correcting the given type of CVD using the given
   * color adjustment.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @param {number} delta A real number in [0,1] denoting color adjustment.
   * @return {!Matrix3x3}
   */
  function getCvdCorrectionMatrix(cvdType, delta) {
    cvdCorrectionParam = cvdCorrectionParams[cvdType];
    // TODO(mustaq): Perhaps nuke full-matrix operations after experiment.
    return cvdCorrectionParam['addendum'].add(
        cvdCorrectionParam['delta_factor'].scale(delta));
  }


  /**
   * Returns the 3x3 matrix to be used for the given settings.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @param {number} severity A real number in [0,1] denoting severity.
   * @param {number} delta A real number in [0,1] denoting color adjustment.
   * @param {boolean} simulate Whether to simulate the CVD type.
   * @param {boolean} enable Whether to enable color filtering.
   * @return {!Matrix3x3}
   */
  function getEffectiveCvdMatrix(
      cvdType, severity, delta, simulate, enable) {
    if (!enable) {
      return Matrix3x3.IDENTITY;
    }

    let effectiveMatrix = getCvdSimulationMatrix(cvdType, severity);

    if (!simulate) {
      const cvdCorrectionMatrix = getCvdCorrectionMatrix(cvdType, delta);
      const tmpProduct = cvdCorrectionMatrix.multiply(effectiveMatrix);

      effectiveMatrix =
          Matrix3x3.IDENTITY.add(cvdCorrectionMatrix).subtract(tmpProduct);
    }

    return effectiveMatrix;
  }


  // ======= Page linker =======

  const STYLE_ID = 'cvd_style';
  const WRAP_ID = 'cvd_extension_svg_filter';

  /**
   * Checks for required elements, adding if missing.
   */
  function addElements() {
    let style = document.getElementById(STYLE_ID);
    if (!style) {
      style = document.createElement('style');
      style.id = STYLE_ID;
      style.setAttribute('type', 'text/css');
      style.innerHTML = cssContent;
      document.head.appendChild(style);
    }

    let wrap = document.getElementById(WRAP_ID);
    if (!wrap) {
      wrap = document.createElement('span');
      wrap.id = WRAP_ID;
      wrap.setAttribute('hidden', '');
      wrap.innerHTML = svgContent;
      document.body.appendChild(wrap);
    }
  }

  /**
   * Updates the SVG filter based on the RGB correction/simulation matrix.
   * @param {!Matrix3x3} matrix  3x3 RGB transformation matrix.
   */
  function setFilter(matrix) {
    addElements();
    const next = 1 - curFilter;

    debugPrint('update: matrix#' + next + '=' + matrix.toString());

    const matrixElem = document.getElementById('cvd_matrix_' + next);
    matrixElem.setAttribute('values', matrix.toSvgString());

    document.documentElement.setAttribute('cvd', next);

    curFilter = next;
  }

  /**
   * Updates the SVG matrix using the current settings.
   */
  function update() {
    if (curEnable) {
      if (!document.body) {
        document.addEventListener('DOMContentLoaded', update);
        return;
      }

      const effectiveMatrix = getEffectiveCvdMatrix(
          curType, curSeverity, curDelta * 2 - 1, curSimulate, curEnable);

      setFilter(effectiveMatrix);

      if (window == window.top) {
        window.scrollBy(0, 1);
        window.scrollBy(0, -1);
      }
    } else {
      clearFilter();
    }
  }


  /**
   * Process a message from background page.
   * @param {!object} message An object containing color filter parameters.
   */
  function onExtensionMessage(message) {
    debugPrint('onExtensionMessage: ' + JSON.stringify(message));
    let changed = false;

    if (!message) {
      return;
    }

    if (message['type'] !== undefined) {
      const type = message.type;
      if (curType != type) {
        curType = type;
        changed = true;
      }
    }

    if (message['severity'] !== undefined) {
      const severity = message.severity;
      if (curSeverity != severity) {
        curSeverity = severity;
        changed = true;
      }
    }

    if (message['delta'] !== undefined) {
      const delta = message.delta;
      if (curDelta != delta) {
        curDelta = delta;
        changed = true;
      }
    }

    if (message['simulate'] !== undefined) {
      const simulate = message.simulate;
      if (curSimulate != simulate) {
        curSimulate = simulate;
        changed = true;
      }
    }

    if (message['enable'] !== undefined) {
      const enable = message.enable;
      if (curEnable != enable) {
        curEnable = enable;
        changed = true;
      }
    }

    if (changed) {
      update();
    }
  }


  /**
   * Remove the filter from the page.
   */
  function clearFilter() {
    document.documentElement.removeAttribute('cvd');
  }


  /**
   * Prepare to process background messages and let it know to send initial
   * values.
   */
  exports.initializeExtension = function () {
    chrome.runtime.onMessage.addListener(onExtensionMessage);
    chrome.runtime.sendMessage("init", onExtensionMessage);
  };

  /**
   * Generate SVG filter for color enhancement based on type and severity using
   * default color adjustment.
   * @param {string} type Type type of color vision defficiency (CVD).
   * @param {number} severity The degree of CVD ranging from 0 for normal
   *     vision to 1 for dichromats.
   */
  exports.getDefaultCvdCorrectionFilter = function(type, severity) {
    return getEffectiveCvdMatrix(type, severity, 0, false, true);
  };

  /**
   * Adds support for a color enhancement filter.
   * @param {!Matrix3x3} matrix 3x3 RGB transformation matrix.
   */
  exports.injectColorEnhancementFilter = function(matrix) {
    setFilter(matrix);
  };

  /**
   * Clears color correction filter.
   */
  exports.clearColorEnhancementFilter = function() {
    clearFilter();
  };
})(this);

this.initializeExtension();
