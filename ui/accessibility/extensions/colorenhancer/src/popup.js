// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Popup {
  constructor() {
    this.site;

    /**
     * Save previous state of setup parameters for use in the event of a
     * canceled setup.
     * @type {{type: !CvdType, axis: !CvdAxis, severity: number} | undefined}
     */
    this.restoreSettings = undefined;

    this.initialize();
  }

  /**
   * Creates a radio button for selecting the given type of CVD and a series of
   * color swatches for testing color vision.
   * @param {!CvdType} cvdType Type of CVD, either PROTANOMALY or
   *     DEUTERANOMALY or TRITANOMALY.
   *  @return {!Element} Row of color swatches with a leading radio button.
   */
  createTestRow(type) {
    const toCssColor = function(rgb) {
      return 'rgb(' + rgb.join(',') + ')';
    };
    const row = document.createElement('label');
    row.classList.add('row');

    const button = document.createElement('input');
    button.id = 'select-' + type;
    button.name = 'cvdType';
    button.setAttribute('type', 'radio');
    button.value = type;
    button.checked = false;
    row.appendChild(button);
    button.addEventListener('change', function() {
      window.popup.onTypeChange(this.value);
    });
    button.setAttribute('aria-label', type);

    Popup.SWATCH_COLORS.forEach((data) => {
      const swatch = document.querySelector('.swatch.template').cloneNode(true);
      swatch.style.background = toCssColor(data.BACKGROUND);
      swatch.style.color = toCssColor(data[type]);
      swatch.classList.remove('template');
      row.appendChild(swatch);
    });
    return row;
  }

  // ======= UI hooks =======

  /**
   * Gets the CVD type selected through the radio buttons.
   * @return {CvdType}
   */
  getCvdTypeSelection() {
    for (const cvdType of Object.values(CvdType)) {
      if (Common.$('select-' + cvdType).checked) {
        return cvdType;
      }
    }
  }

  /**
   * Gets the CVD AXIS selected through the radio buttons.
   * @return {!CvdAxis}
   */
  getCvdAxisSelection() {
    const axisButtons = document.querySelectorAll('input[name="CvdAxis"]');
    for (const axis of axisButtons) {
      if (axis.checked)
        return axis.value;
    }
  }

  /**
   * Sets the radio buttons selection to the given CVD type.
   * @param {!CvdType} cvdType Type of CVD, either PROTANOMALY or
   *     DEUTERANOMALY or TRITANOMALY.
   */
  setCvdTypeSelection(cvdType) {
    const highlight = Common.$('row-highlight');
    highlight.hidden = true;
    Object.values(CvdType).forEach((str) => {
      const checkbox = Common.$('select-' + str);
      if (cvdType == str) {
        checkbox.checked = true;
        const top = checkbox.parentElement.offsetTop - Popup.HIGHLIGHT_OFFSET;
        highlight.style.top = top + 'px';
        highlight.hidden = false;
      } else {
        checkbox.checked = false;
      }
    });
  }

  /**
   * Sets the radio buttons selection to the given CVD axis.
   * @param {!CvdAxis} cvdAxis Type of Axis, either DEFAULT or
   *     RED or GREEN or BLUE.
   */
  setCvdAxisSelection(axis) {
    Common.$('axis-' + axis.toLowerCase()).checked = true;
  }

  /**
   * Enable/Disable all axis selectors.
   * @param {boolean} enable determines if the axis control is enabled.
   */

  updateAxisControls(disable) {
    Common.$('step-3').querySelectorAll('.axis').forEach(axis => {
      axis.disabled = disable;
    });
  }

  /**
   * Styles controls based on stage of setup.
   */
  updateControls() {
    if (Common.$('setup-panel').classList.contains('collapsed')) {
      // Not performing setup.  Ensure main controls are enabled.
      Common.$('enable').disabled = false;
      Common.$('delta').disabled = false;
      Common.$('setup').disabled = false;

      // Disable advanced
      Common.$('advanced-toggle').disabled = true;
    } else {
      // Disable main controls during setup phase.
      Common.$('enable').disabled = true;
      Common.$('delta').disabled = true;
      Common.$('setup').disabled = true;
      this.updateAxisControls(true);

      // Enable Advanced Toggle
      Common.$('advanced-toggle').disabled = false;

      if (!this.getCvdTypeSelection()) {
        // Have not selected a CVD type. Mark Step 1 as active.
        Common.$('step-1').classList.add('active');
        Common.$('step-2').classList.remove('active');
        Common.$('step-3').classList.remove('active');
        // Disable "step 2" controls.
        Common.$('severity').disabled = true;
        Common.$('reset').disabled = true;
        this.updateAxisControls(true);
      } else {
        Common.$('step-1').classList.remove('active');
        Common.$('step-2').classList.add('active');
        // Enable "step 2" controls.
        Common.$('severity').disabled = false;
        Common.$('reset').disabled = false;
        if (Common.$('step-3').classList.contains('advanced')) {
          Common.$('step-3').classList.remove('active');
          this.updateAxisControls(true);
        } else {
          Common.$('step-2').classList.add('active');
          this.updateAxisControls(false);
        }

        // Force filter update.
        this.onSeverityChange(parseFloat(Common.$('severity').value));
      }
    }
  }

  /**
   * Update the popup controls based on settings for this site or the default.
   * @return {boolean} True if settings are valid and update performed.
   */
  update() {
    if (!Object.values(CvdType).includes(Storage.type))
      return false;

    if (this.site) {
      Common.$('delta').value = Storage.getSiteDelta(this.site);
    } else {
      Common.$('delta').value = Storage.baseDelta;
    }

    Common.$('severity').value = Storage.severity;

    if (!Common.$('setup-panel').classList.contains('collapsed')) {
      this.setCvdTypeSelection(Storage.type);
      this.setCvdAxisSelection(Storage.axis);
    }
    Common.$('enable').checked = Storage.enable;

    Common.debugPrint(
        'update: ' +
        ' del=' + Common.$('delta').value + ' sev=' +
        Common.$('severity').value + ' typ=' + Storage.type +
        ' enb=' + Common.$('enable').checked + ' for ' + this.site);
    chrome.runtime.sendMessage('updateTabs');
    return true;
  }

  /**
   * Callback for color rotation slider.
   *
   * @param {number} value Parsed value of slider element.
   */
  onDeltaChange(value) {
    Common.debugPrint('onDeltaChange: ' + value + ' for ' + this.site);
    if (this.site) {
      Storage.setSiteDelta(this.site, value);
      this.update();
    }
    Storage.baseDelta = value;
    this.update();
  }

  /**
   * Callback for severity slider.
   * @param {number} value Parsed value of slider element.
   */
  onSeverityChange(value) {
    Common.debugPrint('onSeverityChange: ' + value + ' for ' + this.site);
    Storage.severity = value;
    this.update();
    // Apply filter to popup swatches.
    const filter =
        cvd.getDefaultCvdCorrectionFilter(this.getCvdTypeSelection(),
            this.getCvdAxisSelection(), value);
    cvd.injectColorEnhancementFilter(filter);
    // Force a refresh.
    window.getComputedStyle(document.documentElement, null);
  }

  /**
   * Callback for changing color deficiency type.
   * @param {string} value Value of dropdown element.
   */
  onTypeChange(value) {
    Common.debugPrint('onTypeChange: ' + value + ' for ' + this.site);
    Storage.type = value;
    Storage.axis = CvdAxis.DEFAULT;
    this.update();
    Common.$('severity').value = 0;
    this.updateControls();
  }

  /**
   * Callback for enable/disable setting.
   *
   * @param {boolean} value Value of checkbox element.
  */
  onEnableChange(value) {
    Common.debugPrint('onEnableChange: ' + value + ' for ' + this.site);
    Storage.enable = value;
    if (!this.update()) {
      // Settings are not valid for a reconfiguration.
      Common.$('setup').onclick();
    }
  }

  /**
   * Callback for changing color deficiency correction axis.
   * @param {string} value Value of checkbox element.
   */
  onAxisChange(value) {
    Common.debugPrint('onAxisChange: ' + value + ' for ' + this.site);
    Storage.axis = value;
    this.update();
    this.updateControls();
  }

  /**
   * Attach event handlers to controls and update the filter config values for
   * the currently visible tab.
   */
  initialize() {
    const i18nElements = document.querySelectorAll('*[i18n-content]');
    for (let i = 0; i < i18nElements.length; i++) {
      const elem = i18nElements[i];
      const msg = elem.getAttribute('i18n-content');
      elem.textContent = chrome.i18n.getMessage(msg);
    }

    Common.$('setup').onclick = () => {
      Common.$('setup-panel').classList.remove('collapsed');
      // Store current settings in the event of a canceled setup.
      this.restoreSettings = {
        type: Storage.type,
        severity: Storage.severity,
        axis: Storage.axis
      };
      // Initialize controls based on current settings.
      this.setCvdTypeSelection(this.restoreSettings.type);
      this.setCvdAxisSelection(this.restoreSettings.axis);
      Common.$('severity').value = this.restoreSettings.severity;
      this.updateControls();
    };

    Common.$('advanced-toggle').onclick = () => {
      if (Common.$('step-3').classList.contains('advanced'))
        Common.$('step-3').classList.remove('advanced');
      else
        Common.$('step-3').classList.add('advanced');
      this.updateControls();
    };

    Common.$('delta').addEventListener('input', function() {
      window.popup.onDeltaChange(parseFloat(this.value));
    });
    Common.$('severity').addEventListener('input', function() {
      window.popup.onSeverityChange(parseFloat(this.value));
    });
    Common.$('enable').addEventListener('change', function() {
      window.popup.onEnableChange(this.checked);
    });

    Common.$('step-3').querySelectorAll('.axis').forEach(axis => {
      axis.addEventListener('change', function(event) {
        window.popup.onAxisChange(event.target.value);
      });
    });

    Common.$('reset').onclick = () => {
      Storage.severity = 0;
      Storage.type = Storage.INVALID_TYPE_PLACEHOLDER;
      Storage.enable = false;
      Storage.axis = CvdAxis.DEFAULT;
      Common.$('severity').value = 0;
      Common.$('enable').checked = false;
      this.setCvdAxisSelection(CvdAxis.DEFAULT);
      this.setCvdTypeSelection('');
      this.updateControls();
      cvd.clearColorEnhancementFilter();
    };
    Common.$('reset').hidden = !Common.IS_DEV_MODE;

    const closeSetup = () => {
      Common.$('setup-panel').classList.add('collapsed');
      Common.$('advanced-toggle').disabled = true;
      this.updateControls();
    };

    Common.$('ok').onclick = () => {
      closeSetup();
    };

    Common.$('cancel').onclick = () => {
      closeSetup();
      if (this.restoreSettings) {
        Common.debugPrint(
            'restore previous settings: ' +
            'type = ' + this.restoreSettings.type +
            'axis = ' + this.restoreSettings.axis +
            ', severity = ' + this.restoreSettings.severity);
        Storage.type = this.restoreSettings.type;
        Storage.severity = this.restoreSettings.severity;
        Storage.axis = this.restoreSettings.axis;
      }
    };

    const swatches = Common.$('swatches');
    Object.values(CvdType).forEach((cvdType) => {
      swatches.appendChild(this.createTestRow(cvdType));
    });

    chrome.windows.getLastFocused({'populate': true}, (w) => {
      for (let i = 0; i < w.tabs.length; i++) {
        const tab = w.tabs[i];
        if (tab.active) {
          this.site = Common.siteFromUrl(tab.url);
          Common.debugPrint('init: active tab update for ' + this.site);
          this.update();
          return;
        }
      }
      this.site = 'unknown site';
      this.update();
    });
  }
}

/**
 * Vertical offset for displaying the row highlight.
 * @const {number}
 */
Popup.HIGHLIGHT_OFFSET = 7;

// ======= Swatch generator =======

/**
 * Set of colors for test swatches.
 * Each row of swatches corresponds to a different type of color blindness.
 * Tests for the 3 different types of dichromatic color vison.
 * Colors selected based on color confusion lines for dichromats using our
 * swatch generator tool. See:
 * http://www.color-blindness.com/2007/01/23/confusion-lines-of-the-cie-1931-color-space/
 */
Popup.SWATCH_COLORS = [
  {
    BACKGROUND: [194, 66, 96],
    PROTANOMALY: [123, 73, 103],
    DEUTERANOMALY: [131, 91, 97],
    TRITANOMALY: [182, 57, 199]
  },
  {
    BACKGROUND: [156, 90, 94],
    PROTANOMALY: [100, 96, 97],
    DEUTERANOMALY: [106, 110, 95],
    TRITANOMALY: [165, 100, 0]
  },
  {
    BACKGROUND: [201, 110, 50],
    PROTANOMALY: [125, 120, 52],
    DEUTERANOMALY: [135, 136, 51],
    TRITANOMALY: [189, 99, 163]
  },
  {
    BACKGROUND: [90, 180, 60],
    PROTANOMALY: [161, 171, 57],
    DEUTERANOMALY: [156, 154, 59],
    TRITANOMALY: [84, 151, 247]
  },
  {
    BACKGROUND: [30, 172, 150],
    PROTANOMALY: [114, 163, 144],
    DEUTERANOMALY: [97, 146, 148],
    TRITANOMALY: [31, 154, 246]
  },
  {
    BACKGROUND: [50, 99, 144],
    PROTANOMALY: [145, 90, 135],
    DEUTERANOMALY: [97, 81, 142],
    TRITANOMALY: [52, 112, 59]
  },
  {
    BACKGROUND: [91, 72, 147],
    PROTANOMALY: [62, 74, 151],
    DEUTERANOMALY: [63, 83, 148],
    TRITANOMALY: [102, 88, 12]
  },
];

window.addEventListener('DOMContentLoaded', () => {
  window.popup = new Popup();
});
Storage.initialize();
