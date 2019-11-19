// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Global exports.  Used by popup to show effect of filter during setup.
 */
(function(exports) {
  // TODO(wnwen): Replace var with let.
  var curDelta = 0;
  var curSeverity = 0;
  var curType = 'PROTANOMALY';
  var curSimulate = false;
  var curEnable = false;
  var curFilter = 0;
  var cssContent = `
html[cvd="0"] {
  -webkit-filter: url('#cvd_extension_0');
}
html[cvd="1"] {
  -webkit-filter: url('#cvd_extension_1');
}
`;

  /** @const {string} */
  var SVG_DEFAULT_MATRIX =
    '1 0 0 0 0 ' +
    '0 1 0 0 0 ' +
    '0 0 1 0 0 ' +
    '0 0 0 1 0';

  var svgContent = `
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

  // ======= 3x3 matrix ops =======

  /**
   * The 3x3 identity matrix.
   * @const {object}
   */
  var IDENTITY_MATRIX_3x3 = [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1]
  ];


  /**
   * Adds two matrices.
   * @param {!object} m1 A 3x3 matrix.
   * @param {!object} m2 A 3x3 matrix.
   * @return {!object} The 3x3 matrix m1 + m2.
   */
  function add3x3(m1, m2) {
    var result = [];
    for (var i = 0; i < 3; i++) {
      result[i] = [];
      for (var j = 0; j < 3; j++) {
        result[i].push(m1[i][j] + m2[i][j]);
      }
    }
    return result;
  }


  /**
   * Subtracts one matrix from another.
   * @param {!object} m1 A 3x3 matrix.
   * @param {!object} m2 A 3x3 matrix.
   * @return {!object} The 3x3 matrix m1 - m2.
   */
  function sub3x3(m1, m2) {
    var result = [];
    for (var i = 0; i < 3; i++) {
      result[i] = [];
      for (var j = 0; j < 3; j++) {
        result[i].push(m1[i][j] - m2[i][j]);
      }
    }
    return result;
  }


  /**
   * Multiplies one matrix with another.
   * @param {!object} m1 A 3x3 matrix.
   * @param {!object} m2 A 3x3 matrix.
   * @return {!object} The 3x3 matrix m1 * m2.
   */
  function mul3x3(m1, m2) {
    var result = [];
    for (var i = 0; i < 3; i++) {
      result[i] = [];
      for (var j = 0; j < 3; j++) {
        var sum = 0;
        for (var k = 0; k < 3; k++) {
          sum += m1[i][k] * m2[k][j];
        }
        result[i].push(sum);
      }
    }
    return result;
  }


  /**
   * Multiplies a matrix with a number.
   * @param {!object} m A 3x3 matrix.
   * @param {!number} k A scalar multiplier.
   * @return {!object} The 3x3 matrix m * k.
   */
  function mul3x3Scalar(m, k) {
    var result = [];
    for (var i = 0; i < 3; i++) {
      result[i] = [];
      for (var j = 0; j < 3; j++) {
        result[i].push(k * m[i][j]);
      }
    }
    return result;
  }


  // ======= 3x3 matrix utils =======

  /**
   * Makes the SVG matrix string (of 20 values) for a given matrix.
   * @param {!object} m A 3x3 matrix.
   * @return {!string} The SVG matrix string for m.
   */
  function svgMatrixStringFrom3x3(m) {
    var outputRows = [];
    for (var i = 0; i < 3; i++) {
      outputRows.push(m[i].join(' ') + ' 0 0');
    }
    // Add the alpha row
    outputRows.push('0 0 0 1 0');
    return outputRows.join(' ');
  }


  /**
   * Makes a human readable string for a given matrix.
   * @param {!object} m A 3x3 matrix.
   * @return {!string} A human-readable string for m.
   */
  function humanReadbleStringFrom3x3(m) {
      var result = '';
      for (var i = 0; i < 3; i++) {
          result += (i ? ', ' : '') + '[';
          for (var j = 0; j < 3; j++) {
              result += (j ? ', ' : '') + m[i][j].toFixed(2);
          }
          result += ']';
      }
      return result;
  }


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
  var cvdSimulationParams = {
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
  var cvdCorrectionParams = {
    PROTANOMALY: {
      addendum: [
        [0.0, 0.0, 0.0],
        [0.7, 1.0, 0.0],
        [0.7, 0.0, 1.0]
      ],
      delta_factor: [
        [0.0, 0.0, 0.0],
        [0.3, 0.0, 0.0],
        [-0.3, 0.0, 0.0]
      ]
    },
    DEUTERANOMALY: {
      addendum: [
        [0.0, 0.0, 0.0],
        [0.7, 1.0, 0.0],
        [0.7, 0.0, 1.0]
      ],
      delta_factor: [
        [0.0, 0.0, 0.0],
        [0.3, 0.0, 0.0],
        [-0.3, 0.0, 0.0]
      ]
    },
    TRITANOMALY: {
      addendum: [
        [1.0, 0.0, 0.7],
        [0.0, 1.0, 0.7],
        [0.0, 0.0, 0.0]
      ],
      delta_factor: [
        [0.0, 0.0, 0.3],
        [0.0, 0.0, -0.3],
        [0.0, 0.0, 0.0]
      ]
    }
  };


  // =======  CVD matrix builders =======

  /**
   * Returns a 3x3 matrix for simulating the given type of CVD with the given
   * severity.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @param {number} severity A real number in [0,1] denoting severity.
   */
  function getCvdSimulationMatrix(cvdType, severity) {
    var cvdSimulationParam = cvdSimulationParams[cvdType];
    var severity2 = severity * severity;
    var matrix = [];
    for (var i = 0; i < 3; i++) {
      var row = [];
      for (var j = 0; j < 3; j++) {
        var paramRow = i*3+j;
        var val = cvdSimulationParam[paramRow][0] * severity2
                + cvdSimulationParam[paramRow][1] * severity
                + cvdSimulationParam[paramRow][2];
        row.push(val);
      }
      matrix.push(row);
    }
    return matrix;
  }


  /**
   * Returns a 3x3 matrix for correcting the given type of CVD using the given
   * color adjustment.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @param {number} delta A real number in [0,1] denoting color adjustment.
   */
  function getCvdCorrectionMatrix(cvdType, delta) {
    cvdCorrectionParam = cvdCorrectionParams[cvdType];
    // TODO(mustaq): Perhaps nuke full-matrix operations after experiment.
    return add3x3(cvdCorrectionParam['addendum'],
                  mul3x3Scalar(cvdCorrectionParam['delta_factor'], delta));
  }


  /**
   * Returns the 3x3 matrix to be used for the given settings.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @param {number} severity A real number in [0,1] denoting severity.
   * @param {number} delta A real number in [0,1] denoting color adjustment.
   * @param {boolean} simulate Whether to simulate the CVD type.
   * @param {boolean} enable Whether to enable color filtering.
   */
  function getEffectiveCvdMatrix(cvdType, severity, delta, simulate, enable) {
    if (!enable) {
      return IDENTITY_MATRIX_3x3;
    }

    var effectiveMatrix = getCvdSimulationMatrix(cvdType, severity);

    if (!simulate) {
      var cvdCorrectionMatrix = getCvdCorrectionMatrix(cvdType, delta);
      var tmpProduct = mul3x3(cvdCorrectionMatrix, effectiveMatrix);

      effectiveMatrix = sub3x3(
          add3x3(IDENTITY_MATRIX_3x3, cvdCorrectionMatrix),
          tmpProduct);
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
    var style = document.getElementById(STYLE_ID);
    if (!style) {
      var baseUrl = window.location.href.replace(window.location.hash, '');
      style = document.createElement('style');
      style.id = STYLE_ID;
      style.setAttribute('type', 'text/css');
      style.innerHTML = cssContent;
      document.head.appendChild(style);
    }

    var wrap = document.getElementById(WRAP_ID);
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
   * @param {!Object} matrix  3x3 RGB transformation matrix.
   */
  function setFilter(matrix) {
    addElements();
    var next = 1 - curFilter;

    debugPrint('update: matrix#' + next + '=' +
        humanReadbleStringFrom3x3(matrix));

    var matrixElem = document.getElementById('cvd_matrix_' + next);
    matrixElem.setAttribute('values', svgMatrixStringFrom3x3(matrix));

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

      var effectiveMatrix = getEffectiveCvdMatrix(
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
   * Process request from background page.
   * @param {!object} request An object containing color filter parameters.
   */
  function onExtensionMessage(request) {
    debugPrint('onExtensionMessage: ' + JSON.stringify(request));
    var changed = false;

    if (request['type'] !== undefined) {
      var type = request.type;
      if (curType != type) {
        curType = type;
        changed = true;
      }
    }

    if (request['severity'] !== undefined) {
      var severity = request.severity;
      if (curSeverity != severity) {
        curSeverity = severity;
        changed = true;
      }
    }

    if (request['delta'] !== undefined) {
      var delta = request.delta;
      if (curDelta != delta) {
        curDelta = delta;
        changed = true;
      }
    }

    if (request['simulate'] !== undefined) {
      var simulate = request.simulate;
      if (curSimulate != simulate) {
        curSimulate = simulate;
        changed = true;
      }
    }

    if (request['enable'] !== undefined) {
      var enable = request.enable;
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
    chrome.extension.onRequest.addListener(onExtensionMessage);
    chrome.extension.sendRequest({'init': true}, onExtensionMessage);
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
   * @param {!Object} matrix 3x3 RGB transformation matrix.
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
