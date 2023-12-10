// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class CVD {
  constructor() {
    /** @private {number} */
    this.curDelta = 0;
    /** @private {number} */
    this.curSeverity = 0;
    /** @private {!CvdType} */
    this.curType = CvdType.PROTANOMALY;
    /** @private {boolean} */
    this.curSimulate = false;
    /** @private {boolean} */
    this.curEnable = false;
    /** @private {number} */
    this.curFilter = 0;
    /** @private {!CvdAxis} */
    this.curAxis = CvdAxis.DEFAULT;

    this.init_();
  }

  /**
   * @const {string}
   * @private
   */
  static cssContent_ = `
html[cvd="0"] {
  filter: url('#cvd_extension_0');
}
html[cvd="1"] {
  filter: url('#cvd_extension_1');
}
`;

  /**
   * @const {string}
   * @private
   */
  static SVG_DEFAULT_MATRIX_ = '1 0 0 0 0 ' +
      '0 1 0 0 0 ' +
      '0 0 1 0 0 ' +
      '0 0 0 1 0';

  /**
   * @const {string}
   * @private
   */
  static svgContent_ = `
<svg xmlns="http://www.w3.org/2000/svg" version="1.1">
  <defs>
    <filter x="0" y="0" width="99999" height="99999" id="cvd_extension_0">
      <feColorMatrix id="cvd_matrix_0" type="matrix" values="
          ${CVD.SVG_DEFAULT_MATRIX_}"/>
    </filter>
    <filter x="0" y="0" width="99999" height="99999" id="cvd_extension_1">
      <feColorMatrix id="cvd_matrix_1" type="matrix" values="
          ${CVD.SVG_DEFAULT_MATRIX_}"/>
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
   * @const {!Object<!CvdType, !Array<!Array<number>>}}
   * @private
   */
  static simulationParams_ = {
    [CvdType.PROTANOMALY]: [
      [0.4720, -1.2946, 0.9857], [-0.6128, 1.6326, 0.0187],
      [0.1407, -0.3380, -0.0044], [-0.1420, 0.2488, 0.0044],
      [0.1872, -0.3908, 0.9942], [-0.0451, 0.1420, 0.0013],
      [0.0222, -0.0253, -0.0004], [-0.0290, -0.0201, 0.0006],
      [0.0068, 0.0454, 0.9990]
    ],
    [CvdType.DEUTERANOMALY]: [
      [0.5442, -1.1454, 0.9818], [-0.7091, 1.5287, 0.0238],
      [0.1650, -0.3833, -0.0055], [-0.1664, 0.4368, 0.0056],
      [0.2178, -0.5327, 0.9927], [-0.0514, 0.0958, 0.0017],
      [0.0180, -0.0288, -0.0006], [-0.0232, -0.0649, 0.0007],
      [0.0052, 0.0360, 0.9998]
    ],
    [CvdType.TRITANOMALY]: [
      [0.4275, -0.0181, 0.9307], [-0.2454, 0.0013, 0.0827],
      [-0.1821, 0.0168, -0.0134], [-0.1280, 0.0047, 0.0202],
      [0.0233, -0.0398, 0.9728], [0.1048, 0.0352, 0.0070],
      [-0.0156, 0.0061, 0.0071], [0.3841, 0.2947, 0.0151],
      [-0.3685, -0.3008, 0.9778]
    ]
  };

  /**
   * TODO(mustaq): This should be nuked, see this.getCvdCorrectionMatrix_().
   * @const {Object<!CvdAxis, {addendum: !Matrix3x3, delta_factor: !Matrix3x3}>}
   * @private
   */
  static correctionParams_ = {
    [CvdAxis.RED]: {
      addendum: Matrix3x3.fromData(
          [[0.0, 0.0, 0.0], [0.7, 1.0, 0.0], [0.7, 0.0, 1.0]]),
      delta_factor: Matrix3x3.fromData(
          [[0.0, 0.0, 0.0], [0.3, 0.0, 0.0], [-0.3, 0.0, 0.0]])
    },
    [CvdAxis.GREEN]: {
      addendum: Matrix3x3.fromData(
          [[1.0, 0.7, 0.0], [0.0, 0.0, 0.0], [0.0, 0.7, 1.0]]),
      delta_factor: Matrix3x3.fromData(
          [[0.0, 0.3, 0.0], [0.3, 0.0, 0.0], [0.0, -0.3, 0.0]])
    },
    [CvdAxis.BLUE]: {
      addendum: Matrix3x3.fromData(
          [[1.0, 0.0, 0.7], [0.0, 1.0, 0.7], [0.0, 0.0, 0.0]]),
      delta_factor: Matrix3x3.fromData(
          [[0.0, 0.0, 0.3], [0.0, 0.0, -0.3], [0.0, 0.0, 0.0]])
    }
  };

  /**
   * @const {string}
   * @private
   */
  static STYLE_ID_ = 'cvd_style';

  /**
   * @const {string}
   * @private
   */
  static WRAP_ID_ = 'cvd_extension_svg_filter';

  // =======  CVD matrix builders =======

  /**
   * Returns a 3x3 matrix for simulating the given type of CVD with the given
   * severity.
   * @param {!CvdType} cvdType Type of CVD, either PROTANOMALY or
   *     DEUTERANOMALY or TRITANOMALY.
   * @param {number} severity A real number in [0,1] denoting severity.
   * @return {!Matrix3x3}
   * @private
   */
  getCvdSimulationMatrix_(cvdType, severity) {
    const calculateElementValue = (i, j) => {
      const cvdSimulationParam = CVD.simulationParams_[cvdType];
      const severity_squared = severity * severity;
      const paramRow = i*3+j;
      return cvdSimulationParam[paramRow][0] * severity_squared
           + cvdSimulationParam[paramRow][1] * severity
           + cvdSimulationParam[paramRow][2];
    }
    return Matrix3x3.fromElementwiseConstruction(calculateElementValue);
  }

  /**
   * Returns a 3x3 matrix for correcting the given type of CVD using the given
   * color adjustment.
   * @param {!CvdType} cvdType Type of CVD, either PROTANOMALY or
   *     DEUTERANOMALY or TRITANOMALY.
   * @param {!CvdAxis} cvdAxis Axis of correction: RED, GREEN, BLUE or DEFAULT.
   * @param {number} delta A real number in [0,1] denoting color adjustment.
   * @return {!Matrix3x3}
   * @private
   */
  getCvdCorrectionMatrix_(cvdType, cvdAxis, delta) {
    if (cvdAxis == CvdAxis.DEFAULT) {
      switch (cvdType) {
      case CvdType.PROTANOMALY:
        cvdAxis = CvdAxis.RED;
        break;
      case CvdType.DEUTERANOMALY:
        cvdAxis = CvdAxis.GREEN;
        break;
      case CvdType.TRITANOMALY:
        cvdAxis = CvdAxis.BLUE;
        break;
      default:
        Common.debugPrint('correction: invalid axis: ' + cvdAxis);
        throw new Error('Invalid Rotation Axis');
      }
    }

    const cvdCorrectionParam = CVD.correctionParams_[cvdAxis];
    // TODO(mustaq): Perhaps nuke full-matrix operations after experiment.
    return cvdCorrectionParam['addendum'].add(
        cvdCorrectionParam['delta_factor'].scale(delta));
  }

  /**
   * Returns the 3x3 matrix to be used for the given settings.
   * @param {!CvdType} cvdType Type of CVD, either PROTANOMALY or
   *     DEUTERANOMALY or TRITANOMALY.
   * @param {!CvdAxis} cvdAxis Axis of correction: RED, GREEN, BLUE or DEFAULT.
   * @param {number} severity A real number in [0,1] denoting severity.
   * @param {number} delta A real number in [0,1] denoting color adjustment.
   * @param {boolean} simulate Whether to simulate the CVD type.
   * @param {boolean} enable Whether to enable color filtering.
   * @return {!Matrix3x3}
   * @private
   */
  getEffectiveCvdMatrix_(cvdType, cvdAxis, severity, delta, simulate, enable) {
    if (!enable) {
      return Matrix3x3.IDENTITY;
    }

    let effectiveMatrix = this.getCvdSimulationMatrix_(cvdType, severity);

    if (!simulate) {
      const cvdCorrectionMatrix =
        this.getCvdCorrectionMatrix_(cvdType, cvdAxis, delta);
      const tmpProduct = cvdCorrectionMatrix.multiply(effectiveMatrix);

      effectiveMatrix =
          Matrix3x3.IDENTITY.add(cvdCorrectionMatrix).subtract(tmpProduct);
    }

    return effectiveMatrix;
  }

  // ======= Page linker =======

  /**
   * Checks for required elements, adding if missing.
   * @private
   */
  addElements_() {
    let style = document.getElementById(CVD.STYLE_ID_);
    if (!style) {
      style = document.createElement('style');
      style.id = CVD.STYLE_ID_;
      style.setAttribute('type', 'text/css');
      style.innerHTML = CVD.cssContent_;
      document.head.appendChild(style);
    }

    let wrap = document.getElementById(CVD.WRAP_ID_);
    if (!wrap) {
      wrap = document.createElement('span');
      wrap.id = CVD.WRAP_ID_;
      wrap.setAttribute('hidden', '');
      wrap.innerHTML = CVD.svgContent_;
      document.body.appendChild(wrap);
    }
  }

  /**
   * Updates the SVG filter based on the RGB correction/simulation matrix.
   * @param {!Matrix3x3} matrix  3x3 RGB transformation matrix.
   * @private
   */
  setFilter_(matrix) {
    this.addElements_();
    const next = 1 - this.curFilter;

    Common.debugPrint('update: matrix#' + next + '=' + matrix.toString());

    const matrixElem = document.getElementById('cvd_matrix_' + next);
    matrixElem.setAttribute('values', matrix.toSvgString());

    document.documentElement.setAttribute('cvd', next);

    this.curFilter = next;
  }

  /**
   * Updates the SVG matrix using the current settings.
   * @private
   */
  update_() {
    if (this.curEnable) {
      if (!document.body) {
        document.addEventListener('DOMContentLoaded', this.update_.bind(this));
        return;
      }

      const effectiveMatrix = this.getEffectiveCvdMatrix_(
          this.curType, this.curAxis, this.curSeverity, this.curDelta * 2 - 1,
          this.curSimulate, this.curEnable);

      this.setFilter_(effectiveMatrix);

      if (window == window.top) {
        window.scrollBy(0, 1);
        window.scrollBy(0, -1);
      }
    } else {
      this.clearFilter_();
    }
  }

  /**
   * Process a message from background page.
   * @param {!object} message An object containing color filter parameters.
   * @private
   */
  onExtensionMessage_(message) {
    Common.debugPrint('onExtensionMessage: ' + JSON.stringify(message));
    let changed = false;

    if (!message) {
      return;
    }

    if (message['type'] !== undefined) {
      const type = message.type;
      if (this.curType != type) {
        this.curType = type;
        changed = true;
      }
    }

    if (message['severity'] !== undefined) {
      const severity = message.severity;
      if (this.curSeverity != severity) {
        this.curSeverity = severity;
        changed = true;
      }
    }

    if (message['delta'] !== undefined) {
      const delta = message.delta;
      if (this.curDelta != delta) {
        this.curDelta = delta;
        changed = true;
      }
    }

    if (message['simulate'] !== undefined) {
      const simulate = message.simulate;
      if (this.curSimulate != simulate) {
        this.curSimulate = simulate;
        changed = true;
      }
    }

    if (message['enable'] !== undefined) {
      const enable = message.enable;
      if (this.curEnable != enable) {
        this.curEnable = enable;
        changed = true;
      }
    }

    if (message['axis'] !== undefined) {
      const axis = message.axis;
      if (this.curAxis !== axis) {
        this.curAxis = axis;
        changed = true;
      }
    }

    if (changed) {
      this.update_();
    }
  }

  /**
   * Remove the filter from the page.
   * @private
   */
  clearFilter_() {
    document.documentElement.removeAttribute('cvd');
  }

  /**
   * Prepare to process background messages and let it know to send initial
   * values.
   * @private
   */
  init_() {
    chrome.runtime.onMessage.addListener(this.onExtensionMessage_.bind(this));
    chrome.runtime.sendMessage('init', this.onExtensionMessage_.bind(this));
  }

  // ============ Public Methods ==============

  /**
   * Generate SVG filter for color enhancement based on type and severity using
   * default color adjustment.
   * @param {!CvdType} type Type type of color vision defficiency (CVD).
   * @param {!CvdAxis} axis Axis of color correction.
   * @param {number} severity The degree of CVD ranging from 0 for normal
   *     vision to 1 for dichromats.
   */
  getDefaultCvdCorrectionFilter(type, axis, severity) {
    return this.getEffectiveCvdMatrix_(type, axis, severity,
        /* set color shift to default correction (zero shift, simulate = false
         * and enable = true) */ 0, false, true);
  }

  /**
   * Adds support for a color enhancement filter.
   * @param {!Matrix3x3} matrix 3x3 RGB transformation matrix.
   */
  injectColorEnhancementFilter(matrix) {
    this.setFilter_(matrix);
  }

  /**
   * Clears color correction filter.
   */
  clearColorEnhancementFilter() {
    this.clearFilter_();
  }
}

window.cvd = new CVD();
