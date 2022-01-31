// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Popup {
  constructor() {
    this.site;

    /**
     * Save previous state of setup parameters for use in the event of a
     * canceled setup.
     * @type {{type: string, severity: number} | undefined}
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
   * @return {?CvdType}
   */
  getCvdTypeSelection() {
    let active = undefined;
    Object.values(CvdType).forEach((str) => {
      if ($('select-' + str).checked) {
        active = str;
        return;
      }
    });
    return /** @type {?CvdType} */ (active);
  }

  /**
   * Sets the radio buttons selection to the given CVD type.
   * @param {!CvdType} cvdType Type of CVD, either PROTANOMALY or
   *     DEUTERANOMALY or TRITANOMALY.
   * @return {?string}
   */
  setCvdTypeSelection(cvdType) {
    const highlight = $('row-highlight');
    highlight.hidden = true;
    Object.values(CvdType).forEach((str) => {
      const checkbox = $('select-' + str);
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
   * Styles controls based on stage of setup.
   */
  updateControls() {
    if ($('setup-panel').classList.contains('collapsed')) {
      // Not performing setup.  Ensure main controls are enabled.
      $('enable').disabled = false;
      $('delta').disabled = false;
      $('setup').disabled = false;
    } else {
      // Disable main controls during setup phase.
      $('enable').disabled = true;
      $('delta').disabled = true;
      $('setup').disabled = true;

      if (!this.getCvdTypeSelection()) {
        // Have not selected a CVD type. Mark Step 1 as active.
        $('step-1').classList.add('active');
        $('step-2').classList.remove('active');
        // Disable "step 2" controls.
        $('severity').disabled = true;
        $('reset').disabled = true;
      } else {
        $('step-1').classList.remove('active');
        $('step-2').classList.add('active');
        // Enable "step 2" controls.
        $('severity').disabled = false;
        $('reset').disabled = false;
        // Force filter update.
        this.onSeverityChange(parseFloat($('severity').value));
      }
    }
  }

  /**
   * Update the popup controls based on settings for this site or the default.
   * @return {boolean} True if settings are valid and update performed.
   */
  async update() {
    const type = await storage.getDefaultType();
    let validType = false;
    Object.values(CvdType).forEach((cvdType) => {
      if (cvdType == type) {
        validType = true;
        return;
      }
    });

    if (!validType)
      return false;

    if (this.site) {
      $('delta').value = await storage.getSiteDelta(this.site);
    } else {
      $('delta').value = await storage.getDefaultDelta();
    }

    $('severity').value = await storage.getDefaultSeverity();

    if (!$('setup-panel').classList.contains('collapsed'))
      this.setCvdTypeSelection(await storage.getDefaultType());
    $('enable').checked = await storage.getDefaultEnable();

    debugPrint(
        'update: ' +
        ' del=' + $('delta').value + ' sev=' + $('severity').value +
        ' typ=' + await storage.getDefaultType() +
        ' enb=' + $('enable').checked + ' for ' + this.site);
    chrome.runtime.sendMessage('updateTabs');
    return true;
  }

  /**
   * Callback for color rotation slider.
   *
   * @param {number} value Parsed value of slider element.
   */
  onDeltaChange(value) {
    debugPrint('onDeltaChange: ' + value + ' for ' + this.site);
    if (this.site) {
      storage.setSiteDelta(this.site, value).then(this.update.bind(this));
    }
    storage.setDefaultDelta(value).then(this.update.bind(this));
  }

  /**
   * Callback for severity slider.
   *
   * @param {number} value Parsed value of slider element.
   */
  onSeverityChange(value) {
    debugPrint('onSeverityChange: ' + value + ' for ' + this.site);
    storage.setDefaultSeverity(value).then(() => {
      this.update();
      // Apply filter to popup swatches.
      const filter =
          cvd.getDefaultCvdCorrectionFilter(this.getCvdTypeSelection(), value);
      cvd.injectColorEnhancementFilter(filter);
      // Force a refresh.
      window.getComputedStyle(document.documentElement, null);
    });
  }

  /**
   * Callback for changing color deficiency type.
   *
   * @param {string} value Value of dropdown element.
   */
  onTypeChange(value) {
    debugPrint('onTypeChange: ' + value + ' for ' + this.site);
    storage.setDefaultType(value).then(() => {
      this.update();
      $('severity').value = 0;
      this.updateControls();
    });
  }

  /**
   * Callback for enable/disable setting.
   *
   * @param {boolean} value Value of checkbox element.
  */
  onEnableChange(value) {
    debugPrint('onEnableChange: ' + value + ' for ' + this.site);
    storage.setDefaultEnable(value).then(() => {
      if (!this.update()) {
        // Settings are not valid for a reconfiguration.
        $('setup').onclick();
      }
    });
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

    $('setup').onclick = async () => {
      $('setup-panel').classList.remove('collapsed');
      // Store current settings in the event of a canceled setup.
      this.restoreSettings = {
        type: await storage.getDefaultType(),
        severity: await storage.getDefaultSeverity()
      };
      // Initialize controls based on current settings.
      this.setCvdTypeSelection(this.restoreSettings.type);
      $('severity').value = this.restoreSettings.severity;
      this.updateControls();
    };

    $('delta').addEventListener('input', function() {
      window.popup.onDeltaChange(parseFloat(this.value));
    });
    $('severity').addEventListener('input', function() {
      window.popup.onSeverityChange(parseFloat(this.value));
    });
    $('enable').addEventListener('change', function() {
      window.popup.onEnableChange(this.checked);
    });

    $('reset').onclick = () => {
      storage.setDefaultSeverity(0);
      storage.setDefaultType('');
      storage.setDefaultEnable(false);
      $('severity').value = 0;
      $('enable').checked = false;
      this.setCvdTypeSelection('');
      this.updateControls();
      cvd.clearColorEnhancementFilter();
    };
    $('reset').hidden = !IS_DEV_MODE;

    const closeSetup = () => {
      $('setup-panel').classList.add('collapsed');
      this.updateControls();
    };

    $('ok').onclick = () => {
      closeSetup();
    };

    $('cancel').onclick = () => {
      closeSetup();
      if (this.restoreSettings) {
        debugPrint(
            'restore previous settings: ' +
            'type = ' + this.restoreSettings.type +
            ', severity = ' + this.restoreSettings.severity);
        storage.setDefaultType(this.restoreSettings.type);
        storage.setDefaultSeverity(this.restoreSettings.severity);
      }
    };

    const swatches = $('swatches');
    Object.values(CvdType).forEach((cvdType) => {
      swatches.appendChild(this.createTestRow(cvdType));
    });

    chrome.windows.getLastFocused({'populate': true}, (w) => {
      for (let i = 0; i < w.tabs.length; i++) {
        const tab = w.tabs[i];
        if (tab.active) {
          this.site = siteFromUrl(tab.url);
          debugPrint('init: active tab update for ' + this.site);
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
  window.storage = new Storage();
});
