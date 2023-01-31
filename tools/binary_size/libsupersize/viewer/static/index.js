// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {string}  */
const DO_NOT_DIFF = 'Don\'t diff';

/** @type {string} Domain hosting the viewer.html */
const FIREBASE_HOST = 'https://chrome-supersize.firebaseapp.com'

/** @type {string} Storage bucket hosting the size diffs. */
const SIZE_FILEHOST = 'https://storage.googleapis.com/chrome-supersize'

/**
 * @param {Array<*>} options
 * @return {DocumentFragment}
 */
function buildOptions(options) {
  const fragment = document.createDocumentFragment();
  for (let option of options) {
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
  if (n > 0)
    optList[((index % n) + n) % n].selected = true;
}

/**
 * @param {HTMLFormElement} form
 * @param {function(): string} fetchDataUrl
 */
function setSubmitListener(form, fetchDataUrl) {
  form.addEventListener('submit', event => {
    event.preventDefault();
    const dataUrl = fetchDataUrl();
    window.open(`${FIREBASE_HOST}/viewer.html?load_url=${dataUrl}`);
  });
}

// Milestones.
(async () => {
  // Milestones.
  const milestoneResponse = await fetch(
      `${SIZE_FILEHOST}/milestones/milestones.json`);
  const milestonesPushed = (await milestoneResponse.json())['pushed'];

  // Official Builds
  const officialBuildsResponse =
      await fetch(`${SIZE_FILEHOST}/official_builds/canary_reports.json`);
  const officialBuildsPushed = (await officialBuildsResponse.json())['pushed'];

  if (document.readyState === 'loading') {
    await new Promise(resolve => {
      document.onreadystatechange =
          () => {
            if (document.readyState !== 'loading') {
              resolve();
              document.onreadystatechange = null;
            }
          }
    });
  }

  /** @type {HTMLButtonElement} */
  const submitButton =   /** @type {HTMLButtonElement} */ (
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
  const showAll = /** @type {HTMLInputElement} */ (
      document.getElementsByName('showall')[0]);

  /** @type {HTMLButtonElement} */
  const btnOpen = /** @type {HTMLButtonElement} */ (
      form.querySelector('button[type="submit"]'));

  /** @type {Array<string>} */
  let activeVersions = [];

  /**
   * @param {string} cpu
   * @param {string} apk
   * @return {string}
   */
  function fmtCpuApk(cpu, apk) {
    return cpu + '/' + apk;
  }

  /**
   * @param {Array<string>} cpus
   * @param {Array<string>} apks
   * @return {Array<string>}
   */
  function cpuApkPairs(cpus, apks) {
    let out = [];
    for (let cpu of cpus) {
      for (let apk of apks) {
        // Chrome.apk not available for arm_64
        if (!(cpu === 'arm_64' && apk === 'Chrome.apk')) {
          out.push(fmtCpuApk(cpu, apk));
        }
      }
    }
    return out;
  }

  function updateApk() {
    // Overwrites the apk selector with entries of format {cpu}/{apk}
    let mainApks = cpuApkPairs(milestonesPushed.cpu, milestonesPushed.apk);
    let canaryApks = officialBuildsPushed.map(a => fmtCpuApk(a.cpu, a.apk));
    selApk.innerHTML = '';
    selApk.appendChild(
        buildOptions([...new Set([...mainApks, ...canaryApks])]));
  }

  /**
   * @param {string} v1
   * @param {string} v2
   * @return {number}
   */
  function compareVersions(v1, v2) {
    function toNumber(s) {
      return (
          s.split('.').map(x => parseInt(x)).reduce((x, y) => x * 1000 + y));
    }
    return toNumber(v1) - toNumber(v2);
  }

  function updateVersions() {
    const prev = selVersion1.value;
    // For the selected APK
    let mainVersions = milestonesPushed.version;
    let canaryVersions =
        (officialBuildsPushed
             .filter(a => fmtCpuApk(a.cpu, a.apk) === selApk.value)
             .map(a => a.version + ' (canary)'));

    if (selApk.value.indexOf('AndroidWebview.apk') !== -1) {
      // AndroidWebview.apk size information exists only for M71 and above.
      mainVersions =
          mainVersions.filter(v2 => compareVersions(v2, '71.0.0.0') > 0);
    }

    if (showAll.checked) {
      activeVersions = [...mainVersions, ...canaryVersions];
      activeVersions.sort(compareVersions);
    } else {
      canaryVersions.sort(compareVersions);
      activeVersions = [...mainVersions];
      if (canaryVersions.length) {
        activeVersions.push(canaryVersions[canaryVersions.length - 1]);
      }
    }
    selVersion1.innerHTML = '';
    selVersion1.appendChild(buildOptions(activeVersions));
    // Selects latest version (index -1) if previous option not still in list.
    selectOption(
        /** @type {NodeListOf<HTMLOptionElement>} */ (
            selVersion1.querySelectorAll('option')),
        activeVersions.indexOf(prev));
  }

  function updateDiffVersions() {
    // Filter diff-against versions that are newer
    // Preserve current options if possible
    const prev = selVersion2.value;
    selVersion2.innerHTML = '';
    let v1 = selVersion1.value;
    if (v1) {
      let diffVersions =
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
  });

  selVersion1.addEventListener('change', () => {
    updateDiffVersions();
  });

  showAll.addEventListener('click', () => {
    updateApk();
    updateVersions();
    updateDiffVersions();
  });

  /** @return {string} */
  function getDataUrl() {
    function sizeUrlFor(value) {
      if (value.indexOf('canary') !== -1) {
        const strippedVersion = value.replace(/[^\d.]/g, '');
        return `${SIZE_FILEHOST}/official_builds/reports/${strippedVersion}/${
            selApk.value}.size`;
      }
      return `${SIZE_FILEHOST}/milestones/${value}/${selApk.value}.size`;
    }
    let ret = sizeUrlFor(selVersion1.value);
    if (selVersion2.value !== DO_NOT_DIFF) {
      ret += '&before_url=' + sizeUrlFor(selVersion2.value);
    }
    return ret;
  }

  setSubmitListener(form, getDataUrl);
})();
