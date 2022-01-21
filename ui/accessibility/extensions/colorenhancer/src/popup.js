// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Global exports, used locally to separate initialization from declaration.
 */
(function(exports) {
  var site;

  /**
   * Toggle between filters 0 and 1 in order to force a repaint.
   * TODO(kevers): Consolidate with filter in CVD.
   * @type {!number}
   */
  var activeFilterIndex = 0;

  /**
   * Save previous state of setup parameters for use in the event of a canceled
   * setup.
   * @type {{type: string, severity: number} | undefined}
   */
  var restoreSettings = undefined;

  /**
   * The strings for CVD Types.
   * TODO(mustaq): Define an enum in cvd.js instead.
   * @const {array{string}}
   */
  var CVD_TYPES = [
    'PROTANOMALY',
    'DEUTERANOMALY',
    'TRITANOMALY'
  ];

  /**
   * Vertical offset for displaying the row highlight.
   * @const {number}
   */
  var HIGHLIGHT_OFFSET = 7;

  // ======= Swatch generator =======

  /**
   * Set of colors for test swatches.
   * Each row of swatches corresponds to a different type of color blindness.
   * Tests for the 3 different types of dichromatic color vison.
   * Colors selected based on color confusion lines for dichromats using our
   * swatch generator tool. See:
   * http://www.color-blindness.com/2007/01/23/confusion-lines-of-the-cie-1931-color-space/
   */
  var SWATCH_COLORS = [
    {
      BACKGROUND: [194,66,96],
      PROTANOMALY: [123,73,103],
      DEUTERANOMALY: [131,91,97],
      TRITANOMALY: [182,57,199]
    },
    {
      BACKGROUND: [156,90,94],
      PROTANOMALY: [100,96,97],
      DEUTERANOMALY: [106,110,95],
      TRITANOMALY: [165,100,0]
    },
    {
      BACKGROUND: [201,110,50],
      PROTANOMALY: [125,120,52],
      DEUTERANOMALY: [135,136,51],
      TRITANOMALY: [189,99,163]
    },
    {
      BACKGROUND: [90,180,60],
      PROTANOMALY: [161,171,57],
      DEUTERANOMALY: [156,154,59],
      TRITANOMALY: [84,151,247]
    },
    {
      BACKGROUND: [30,172,150],
      PROTANOMALY: [114,163,144],
      DEUTERANOMALY: [97,146,148],
      TRITANOMALY: [31,154,246]
    },
    {
      BACKGROUND: [50,99,144],
      PROTANOMALY: [145,90,135],
      DEUTERANOMALY: [97,81,142],
      TRITANOMALY: [52,112,59]
    },
    {
      BACKGROUND: [91,72,147],
      PROTANOMALY: [62,74,151],
      DEUTERANOMALY: [63,83,148],
      TRITANOMALY: [102,88,12]
    },
  ];

  /**
   * Creates a radio button for selecting the given type of CVD and a series of
   * color swatches for testing color vision.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   *  @return {!Element} Row of color swatches with a leading radio button.
   */
  function createTestRow(type) {
    var toCssColor = function(rgb) {
      return 'rgb(' + rgb.join(',') + ')';
    };
    var row = document.createElement('label');
    row.classList.add('row');

    var button = document.createElement('input');
    button.id = 'select-' + type;
    button.name = 'cvdType';
    button.setAttribute('type', 'radio');
    button.value = type;
    button.checked = false;
    row.appendChild(button);
    button.addEventListener('change', function() {
      onTypeChange(this.value);
    });
    button.setAttribute('aria-label', type);

    SWATCH_COLORS.forEach(function(data) {
      var swatch = document.querySelector('.swatch.template').cloneNode(true);
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
   * @return {?string}
   */
  function getCvdTypeSelection() {
    var active = undefined;
    CVD_TYPES.forEach(function(str) {
      if ($('select-' + str).checked) {
        active = str;
        return;
      }
    });
    return active;
  }


  /**
   * Sets the radio buttons selection to the given CVD type.
   * @param {string} cvdType Type of CVD, either "PROTANOMALY" or
   *     "DEUTERANOMALY" or "TRITANOMALY".
   * @return {?string}
   */
  function setCvdTypeSelection(cvdType) {
    var highlight = $('row-highlight');
    highlight.hidden = true;
    CVD_TYPES.forEach(function(str) {
      var checkbox = $('select-' + str);
      if (cvdType == str) {
        checkbox.checked = true;
        var top = checkbox.parentElement.offsetTop - HIGHLIGHT_OFFSET;
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
  function updateControls() {
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

      if (!getCvdTypeSelection()) {
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
        onSeverityChange(parseFloat($('severity').value));
      }
    }
  }

  /**
   * Update the popup controls based on settings for this site or the default.
   * @return {boolean} True if settings are valid and update performed.
   */
  async function update() {
    var type = await getDefaultType();
    var validType = false;
    CVD_TYPES.forEach(function(cvdType) {
      if (cvdType == type) {
        validType = true;
        return;
      }
    });

    if (!validType)
      return false;

    if (site) {
      $('delta').value = await getSiteDelta(site);
    } else {
      $('delta').value = await getDefaultDelta();
    }

    $('severity').value = await getDefaultSeverity();

    if (!$('setup-panel').classList.contains('collapsed'))
      setCvdTypeSelection(await getDefaultType());
    $('enable').checked = await getDefaultEnable();

    debugPrint('update: ' +
        ' del=' + $('delta').value +
        ' sev=' + $('severity').value +
        ' typ=' + await getDefaultType() +
        ' enb=' + $('enable').checked +
        ' for ' + site
    );
    chrome.runtime.sendMessage('updateTabs');
    return true;
  }

  /**
   * Callback for color rotation slider.
   *
   * @param {number} value Parsed value of slider element.
   */
  function onDeltaChange(value) {
    debugPrint('onDeltaChange: ' + value + ' for ' + site);
    if (site) {
      setSiteDelta(site, value);
    }
    setDefaultDelta(value);
    update();
  }

  /**
   * Callback for severity slider.
   *
   * @param {number} value Parsed value of slider element.
   */
  function onSeverityChange(value) {
    debugPrint('onSeverityChange: ' + value + ' for ' + site);
    setDefaultSeverity(value);
    update();
    // Apply filter to popup swatches.
    var filter = window.getDefaultCvdCorrectionFilter(
        getCvdTypeSelection(), value);
    injectColorEnhancementFilter(filter);
    // Force a refresh.
    window.getComputedStyle(document.documentElement, null);
  }

  /**
   * Callback for changing color deficiency type.
   *
   * @param {string} value Value of dropdown element.
   */
  function onTypeChange(value) {
    debugPrint('onTypeChange: ' + value + ' for ' + site);
    setDefaultType(value);
    update();
    // TODO(kevers): reset severity to effectively disable filter.
    activeFilterType = value;
    $('severity').value = 0;
    updateControls();
  }

  /**
   * Callback for enable/disable setting.
   *
   * @param {boolean} value Value of checkbox element.
  */
  function onEnableChange(value) {
    debugPrint('onEnableChange: ' + value + ' for ' + site);
    setDefaultEnable(value);
    if (!update()) {
      // Settings are not valid for a reconfiguration.
      $('setup').onclick();
    }
  }

  /**
   * Callback for resetting stored per-site values.
   */
  function onReset() {
    debugPrint('onReset');
    resetSiteDeltas();
    update();
  }

  /**
   * Attach event handlers to controls and update the filter config values for
   * the currently visible tab.
   */
  function initialize() {
    var i18nElements = document.querySelectorAll('*[i18n-content]');
    for (var i = 0; i < i18nElements.length; i++) {
      var elem = i18nElements[i];
      var msg = elem.getAttribute('i18n-content');
      elem.textContent = chrome.i18n.getMessage(msg);
    }

    $('setup').onclick = async function() {
      $('setup-panel').classList.remove('collapsed');
      // Store current settings in the event of a canceled setup.
      restoreSettings = {
        type: await getDefaultType(),
        severity: await getDefaultSeverity()
      };
      // Initalize controls based on current settings.
      setCvdTypeSelection(restoreSettings.type);
      $('severity').value = restoreSettings.severity;
      updateControls();
    };

    $('delta').addEventListener('input', function() {
      onDeltaChange(parseFloat(this.value));
    });
    $('severity').addEventListener('input', function() {
      onSeverityChange(parseFloat(this.value));
    });
    $('enable').addEventListener('change', function() {
      onEnableChange(this.checked);
    });

    $('reset').onclick = function() {
      setDefaultSeverity(0);
      setDefaultType('');
      setDefaultEnable(false);
      $('severity').value = 0;
      $('enable').checked = false;
      setCvdTypeSelection('');
      updateControls();
      clearColorEnhancementFilter();
    };
    $('reset').hidden = !IS_DEV_MODE;

    var closeSetup = function() {
      $('setup-panel').classList.add('collapsed');
      updateControls();
    };

    $('ok').onclick = function() {
      closeSetup();
    };

    $('cancel').onclick = function() {
      closeSetup();
      if (restoreSettings) {
        debugPrint(
          'restore previous settings: ' +
          'type = ' + restoreSettings.type +
           ', severity = ' + restoreSettings.severity);
        setDefaultType(restoreSettings.type);
        setDefaultSeverity(restoreSettings.severity);
      }
    };

    var swatches = $('swatches');
    CVD_TYPES.forEach(function(cvdType) {
      swatches.appendChild(createTestRow(cvdType));
    });

    chrome.windows.getLastFocused({'populate': true}, function(window) {
      for (var i = 0; i < window.tabs.length; i++) {
        var tab = window.tabs[i];
        if (tab.active) {
          site = siteFromUrl(tab.url);
          debugPrint('init: active tab update for ' + site);
          update();
          return;
        }
      }
      site = 'unknown site';
      update();
    });
  }

  /**
   * Runs initialize once popup loading is complete.
   */
  exports.initializeOnLoad = function() {
    var ready = new Promise(function readyPromise(resolve) {
      if (document.readyState === 'complete') {
        resolve();
      }
      document.addEventListener('DOMContentLoaded', resolve);
    });
    ready.then(initialize);
  };
})(this);

this.initializeOnLoad();
