// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {string}  */
const DO_NOT_DIFF = 'Don\'t diff';

/** @type {string} Storage bucket hosting the size diffs. */
const SIZE_FILEHOST = 'https://storage.googleapis.com/chrome-supersize';

/**
 * @type {string} GCS JSON API endpoint (supports CORS from all origins for
 *     public objects).
 */
const STORAGE_API_ENDPOINT =
    'https://storage.googleapis.com/storage/v1/b/chrome-supersize/o';

/** Number of major versions to consider "recent" for APK filtering. */
const RECENT_MAJOR_VERSIONS_COUNT = 4;

/**
 * @typedef {{cpu: string, apk: string, version: string}} ReportEntry
 */

/**
 * @param {Array<string>} options
 * @return {DocumentFragment}
 */
function buildOptions(options) {
  const fragment = document.createDocumentFragment();
  for (const option of options) {
    const optionEl = document.createElement('option');
    optionEl.value = option;
    optionEl.textContent = option;
    fragment.appendChild(optionEl);
  }
  return fragment;
}

/**
 * @param {NodeListOf<HTMLOptionElement>} optList
 * @param {number} index
 */
function selectOption(optList, index) {
  const n = optList.length;
  if (n > 0) {
    optList[((index % n) + n) % n].selected = true;
  }
}

/**
 * @param {HTMLFormElement} form
 * @param {function(): (string|null)} fetchDataUrl
 */
function setSubmitListener(form, fetchDataUrl) {
  form.addEventListener('submit', event => {
    event.preventDefault();
    const dataUrl = fetchDataUrl();
    if (dataUrl) {
      window.open(`viewer.html?load_url=${dataUrl}`);
    }
  });
}

/**
 * @param {string} v
 * @return {Array<number>}
 */
function parseVersion(v) {
  return v.replace(/[^\d.]/g, '').split('.').map(x => parseInt(x, 10) || 0);
}

/**
 * @param {string} v1
 * @param {string} v2
 * @return {number}
 */
function compareVersions(v1, v2) {
  const p1 = parseVersion(v1);
  const p2 = parseVersion(v2);
  const len = Math.max(p1.length, p2.length);
  for (let i = 0; i < len; i++) {
    const n1 = p1[i] || 0;
    const n2 = p2[i] || 0;
    if (n1 !== n2) {
      return n1 - n2;
    }
  }
  return v1.localeCompare(v2);
}

/**
 * @param {string} v
 * @return {number}
 */
function getMajorVersion(v) {
  const m = parseInt(v, 10);
  return Number.isNaN(m) ? 0 : m;
}

/**
 * @param {string} cpu
 * @param {string} apk
 * @return {string}
 */
function fmtCpuApk(cpu, apk) {
  return cpu + '/' + apk;
}

/**
 * Normalizes 'pushed' data from JSON into an array of ReportEntry objects.
 * Handles both new array format and legacy {cpu, apk, version} object format.
 * @param {*} pushed
 * @return {Array<ReportEntry>}
 */
function normalizePushedReports(pushed) {
  if (Array.isArray(pushed)) {
    return pushed;
  }
  if (pushed && pushed.cpu && pushed.apk && pushed.version) {
    const reports = [];
    for (const cpu of pushed.cpu) {
      for (const apk of pushed.apk) {
        if (cpu === 'arm_64' && apk === 'Chrome.apk') {
          continue;
        }
        for (const version of pushed.version) {
          if (apk === 'AndroidWebview.apk' &&
              compareVersions(version, '71.0.0.0') < 0) {
            continue;
          }
          reports.push({cpu, apk, version});
        }
      }
    }
    return reports;
  }
  return [];
}

// Milestones and Official Builds.
(async () => {
  // Milestones.
  const milestoneResponse = await fetch(
      `${STORAGE_API_ENDPOINT}/milestones%2Fmilestones.json?alt=media`);
  const milestonesPushed = (await milestoneResponse.json())['pushed'];
  /** @type {Array<ReportEntry>} */
  const milestoneReports = normalizePushedReports(milestonesPushed);

  // Official Builds
  const officialBuildsResponse = await fetch(`${
      STORAGE_API_ENDPOINT}/official_builds%2Fcanary_reports.json?alt=media`);
  const officialBuildsPushed = (await officialBuildsResponse.json())['pushed'];
  /** @type {Array<ReportEntry>} */
  const canaryReports = normalizePushedReports(officialBuildsPushed);

  if (document.readyState === 'loading') {
    await new Promise(resolve => {
      document.onreadystatechange = () => {
        if (document.readyState !== 'loading') {
          resolve();
          document.onreadystatechange = null;
        }
      };
    });
  }

  /** @type {HTMLButtonElement} */
  const submitButton = /** @type {HTMLButtonElement} */ (
      document.getElementById('submit-button'));

  /** @type {HTMLFormElement} */
  const form = /** @type {HTMLFormElement} */ (
      document.getElementById('select-form'));

  /** @type {HTMLSelectElement} */
  const selApk = /** @type {HTMLSelectElement} */ (
      form.elements.namedItem('apk'));

  /** @type {HTMLSelectElement} */
  const selVersion1 = /** @type {HTMLSelectElement} */ (
      form.elements.namedItem('version1'));

  /** @type {HTMLSelectElement} */
  const selVersion2 = /** @type {HTMLSelectElement} */ (
      form.elements.namedItem('version2'));

  /** @type {HTMLInputElement} */
  const showAllApks = /** @type {HTMLInputElement} */ (
      document.getElementsByName('showall_apks')[0]);

  /** @type {HTMLInputElement} */
  const showAllCanary = /** @type {HTMLInputElement} */ (
      document.getElementsByName('showall')[0]);

  // Compute maximum major version across all reports (milestone and canary).
  let maxMajor = 0;
  for (const r of milestoneReports) {
    const m = getMajorVersion(r.version);
    if (m > maxMajor) {
      maxMajor = m;
    }
  }
  for (const r of canaryReports) {
    const m = getMajorVersion(r.version);
    if (m > maxMajor) {
      maxMajor = m;
    }
  }
  const recentMajorThreshold = maxMajor - RECENT_MAJOR_VERSIONS_COUNT;

  // Identify all distinct APKs and recent APKs.
  const allApkSet = new Set();
  const recentApkSet = new Set();
  for (const r of milestoneReports) {
    const key = fmtCpuApk(r.cpu, r.apk);
    allApkSet.add(key);
    if (getMajorVersion(r.version) >= recentMajorThreshold) {
      recentApkSet.add(key);
    }
  }
  for (const r of canaryReports) {
    const key = fmtCpuApk(r.cpu, r.apk);
    allApkSet.add(key);
    if (getMajorVersion(r.version) >= recentMajorThreshold) {
      recentApkSet.add(key);
    }
  }

  const allApks = Array.from(allApkSet).sort();
  const recentApks = Array.from(recentApkSet).sort();

  /** @type {Array<string>} */
  let activeVersions = [];

  function updateApk() {
    const prev = selApk.value;
    const apksToShow =
        showAllApks && showAllApks.checked ? allApks : recentApks;
    selApk.innerHTML = '';
    selApk.appendChild(buildOptions(apksToShow));
    const index = apksToShow.indexOf(prev);
    selectOption(
        /** @type {NodeListOf<HTMLOptionElement>} */ (
            selApk.querySelectorAll('option')),
        index >= 0 ? index : 0);
  }

  function updateVersions() {
    const prev = selVersion1.value;
    const selectedApk = selApk.value;

    const mainVersions =
        milestoneReports.filter(r => fmtCpuApk(r.cpu, r.apk) === selectedApk)
            .map(r => r.version);
    const canaryVersions =
        canaryReports.filter(r => fmtCpuApk(r.cpu, r.apk) === selectedApk)
            .map(r => r.version + ' (canary)');

    if (showAllCanary && showAllCanary.checked) {
      activeVersions = [...mainVersions, ...canaryVersions];
      activeVersions.sort(compareVersions);
    } else {
      canaryVersions.sort(compareVersions);
      activeVersions = [...mainVersions];
      if (canaryVersions.length > 0) {
        activeVersions.push(canaryVersions[canaryVersions.length - 1]);
      }
      activeVersions.sort(compareVersions);
    }

    selVersion1.innerHTML = '';
    selVersion1.appendChild(buildOptions(activeVersions));
    // Selects latest version (index -1) if previous option not still in list.
    selectOption(
        /** @type {NodeListOf<HTMLOptionElement>} */ (
            selVersion1.querySelectorAll('option')),
        activeVersions.indexOf(prev));

    if (submitButton) {
      submitButton.disabled = activeVersions.length === 0;
    }
  }

  function updateDiffVersions() {
    // Filter diff-against versions that are older than version1
    // Preserve current options if possible
    const prev = selVersion2.value;
    selVersion2.innerHTML = '';
    const v1 = selVersion1.value;
    if (v1) {
      const diffVersions =
          activeVersions.filter(v2 => compareVersions(v2, v1) < 0);
      diffVersions.push(DO_NOT_DIFF);
      selVersion2.appendChild(buildOptions(diffVersions));
      selectOption(
          /** @type {NodeListOf<HTMLOptionElement>} */ (
              selVersion2.querySelectorAll('option')),
          diffVersions.indexOf(prev));
    }
  }

  updateApk();
  updateVersions();
  updateDiffVersions();

  selApk.addEventListener('change', () => {
    updateVersions();
    updateDiffVersions();
  });

  selVersion1.addEventListener('change', () => {
    updateDiffVersions();
  });

  if (showAllApks) {
    showAllApks.addEventListener('change', () => {
      updateApk();
      updateVersions();
      updateDiffVersions();
    });
  }

  if (showAllCanary) {
    showAllCanary.addEventListener('change', () => {
      updateVersions();
      updateDiffVersions();
    });
  }

  /** @return {string|null} */
  function getDataUrl() {
    if (!selVersion1.value || !selApk.value) {
      return null;
    }
    function sizeUrlFor(value) {
      if (value.indexOf('canary') !== -1) {
        const strippedVersion = value.replace(/[^\d.]/g, '');
        return `${SIZE_FILEHOST}/official_builds/reports/${strippedVersion}/${
            selApk.value}.size`;
      }
      return `${SIZE_FILEHOST}/milestones/${value}/${selApk.value}.size`;
    }
    let ret = sizeUrlFor(selVersion1.value);
    if (selVersion2.value && selVersion2.value !== DO_NOT_DIFF) {
      ret += '&before_url=' + sizeUrlFor(selVersion2.value);
    }
    return ret;
  }

  setSubmitListener(form, getDataUrl);
})();
