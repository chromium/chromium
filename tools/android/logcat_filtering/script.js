// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Display a logcat file based on the options selected by the user.
 */

/**
 * @typedef {Object} ParsedLine
 * @property {boolean} isLogcat
 * @property {string|undefined} originalLine - defined when isLogcat = false
 * @property {string|undefined} date - defined when isLogcat = true
 * @property {string|undefined} time - defined when isLogcat = true
 * @property {number|undefined} pid - defined when isLogcat = true
 * @property {number|undefined} tid - defined when isLogcat = true
 * @property {string|undefined} priority - defined when isLogcat = true
 * @property {string|undefined} tag - defined when isLogcat = true
 * @property {string|undefined} message - defined when isLogcat = true
 */

/**
 * @typedef {Object} FilterOption
 * @property {HTMLElement} li
 * @property {HTMLElement} checkbox
 */

// HTML elements:

const controlsDiv = document.getElementById('controls');
const fileUploadButton = document.getElementById('file-upload-button');
const nextExceptionButton = document.getElementById('next-exception-button');
const nextTestButton = document.getElementById('next-test-button');
const dropdownHeaderProcess = document.getElementById(
  'dropdown-header-process');
const dropdownListProcess = document.getElementById('dropdown-list-process');
const dropdownHeaderTag = document.getElementById('dropdown-header-tag');
const dropdownListTag = document.getElementById('dropdown-list-tag');
const dropdownHeaderPriority = document.getElementById(
  'dropdown-header-priority');
const dropdownListPriority = document.getElementById('dropdown-list-priority');
const hideDateTimeCheckbox = document.getElementById('hide-date-time-checkbox');
const wrapTextCheckbox = document.getElementById('wrap-text-checkbox');
const displayNonLogcatCheckbox = document.getElementById(
  'display-non-logcat-checkbox');
const toggleDarkModeCheckbox = document.getElementById(
  'toggle-dark-mode-checkbox');
const textDisplayArea = document.getElementById('text-display-area');

// Event listeners:

fileUploadButton.addEventListener('change', handleFileUpload);

nextExceptionButton.addEventListener('click', jumpToNextException);

nextTestButton.addEventListener('click', jumpToNextTest);

dropdownHeaderProcess.addEventListener('click', () => {
  if (dropdownListProcess.style.display === 'block') {
    dropdownListProcess.style.display = 'none';
  } else {
    dropdownListProcess.style.display = 'block';
  }
});

dropdownListProcess.addEventListener('click', handleProcessFilterOptionClick);

dropdownHeaderTag.addEventListener('click', () => {
  if (dropdownListTag.style.display === 'block') {
    dropdownListTag.style.display = 'none';
  } else {
    dropdownListTag.style.display = 'block';
  }
});

dropdownListTag.addEventListener('click', handleTagFilterOptionClick);

dropdownHeaderPriority.addEventListener('click', () => {
  if (dropdownListPriority.style.display === 'block') {
    dropdownListPriority.style.display = 'none';
  } else {
    dropdownListPriority.style.display = 'block';
  }
});

dropdownListPriority.addEventListener('click', handlePriorityFilterOptionClick);

hideDateTimeCheckbox.addEventListener('change', updateTextDisplayArea);

wrapTextCheckbox.addEventListener('change', () => {
  const shouldWrapText = wrapTextCheckbox.checked;
  if (shouldWrapText) {
    textDisplayArea.classList.add('wrap-text');
    textDisplayArea.classList.remove('not-wrap-text');
  } else {
    textDisplayArea.classList.add('not-wrap-text');
    textDisplayArea.classList.remove('wrap-text');
  }
});

displayNonLogcatCheckbox.addEventListener('change', updateTextDisplayArea);

toggleDarkModeCheckbox.addEventListener('change', () => {
  const isDarkMode = toggleDarkModeCheckbox.checked;
  if (isDarkMode) {
    document.body.classList.add('dark-mode');
    document.body.classList.remove('light-mode');
  } else {
    document.body.classList.add('light-mode');
    document.body.classList.remove('dark-mode');
  }
});

// Global variables:

/** @type {Array<ParsedLine>} */
let currentFileParsedLines = [];

/** @type {FilterOption} */
const displayAllProcesses = {
  li: document.getElementById('display-all-process-li'),
  checkbox: document.getElementById('display-all-process-checkbox'),
};

/** @type {FilterOption} */
const displayNamedProcesses = {
  li: document.getElementById('display-named-process-li'),
  checkbox: document.getElementById('display-named-process-checkbox'),
};

/** @type {Array<FilterOption>} */
let displaySingleNamedProcess = [];

/** @type {Array<FilterOption>} */
let displaySingleUnnamedProcess = [];

/** @type {Set<number>} */
let allPids = new Set();

/** @type {Map<number, string>} */
let pidToProcessName = new Map();

/** @type {FilterOption} */
const displayAllTags = {
  li: document.getElementById('display-all-tag-li'),
  checkbox: document.getElementById('display-all-tag-checkbox'),
};

/** @type {Array<FilterOption>} */
let displaySingleTag = [];

/** @type {Set<string>} */
let allTags = new Set();

/** @type {FilterOption} */
const displayAllPriorities = {
  li: document.getElementById('display-all-priority-li'),
  checkbox: document.getElementById('display-all-priority-checkbox'),
};

const displaySinglePriorityLi = Array.from(
  dropdownListPriority.querySelectorAll('li[data-type="priority"]'));

/** @type {Array<FilterOption>} */
const displaySinglePriority = displaySinglePriorityLi.map(li => {
  const checkbox = li.querySelector('input[type="checkbox"]');
  return {
    li: li,
    checkbox: checkbox,
  };
});

/** @type {RegExp} */
const pidRegexPattern = new RegExp('Start proc (\\d+):(.+?)\\/');

// Set the offset of the text display area so that it stays below the controls.

function setTextDisplayAreaOffset() {
  const controlsHeight = controls.offsetHeight;
  textDisplayArea.style.marginTop = `${controlsHeight}px`;
}

setTextDisplayAreaOffset();

// Re-calculate the offset of the text display area whenever the size of the
// controls change.

const resizeObserver = new ResizeObserver(entries => {
  for (let entry of entries) {
    if (entry.target === controls) {
      setTextDisplayAreaOffset();
    }
  }
});

resizeObserver.observe(controls);

/**
 * This function is called when the user clicks on the file upload button.
 * @param {Event} event
 */
function handleFileUpload(event) {
  const file = event.target.files[0];
  if (file) {
    const reader = new FileReader();
    reader.onload = function(e) {
      setUpElements(e.target.result.split('\n'));
      updateTextDisplayArea(false);
    };
    reader.onerror = function(e) {
      setUpElements([]);
      textDisplayArea.innerHTML = 'Encountered an error when reading the file.';
    };
    reader.readAsText(file);
  } else {
    setUpElements([]);
    textDisplayArea.innerHTML = 'Please select a file.';
  }
}

/**
 * Given an array containing the lines in the current logcat file,
 * populate the global variables and set up the dropdowns.
 * @param {Array<string>} currentFileLines
 */
function setUpElements(currentFileLines) {
  processCurrentFileLines(currentFileLines);
  setUpProcessDropdownList();
  setUpTagDropdownList();
  setUpPriorityDropdownList();
}

/**
 * Process an array containing the lines in the current logcat file,
 * and populate the global variables currentFileParsedLines, allPids, allTags,
 * pidToProcessName based on the contents of the logcat file.
 * @param {Array<string>} currentFileLines
 */
function processCurrentFileLines(currentFileLines) {
  currentFileParsedLines = [];
  allPids = new Set();
  allTags = new Set();
  pidToProcessName = new Map();

  for (const line of currentFileLines) {
    const parsedLine = parseOneLine(line);
    currentFileParsedLines.push(parsedLine);
    if (!parsedLine.isLogcat) {
      continue;
    }

    allPids.add(parsedLine.pid);
    allTags.add(parsedLine.tag);

    const match = pidRegexPattern.exec(parsedLine.message);
    if (match) {
      const processId = parseInt(match[1], 10);
      if (!isNaN(processId)) {
        const processName = match[2];
        allPids.add(processId);
        pidToProcessName.set(processId, processName);
      }
    }
  }
}

/**
 * Parse a single line in the logcat file, and return an object of type
 * ParsedLine that contains the individual components of the logcat line.
 * @param {string} line
 * @return {ParsedLine}
 */
function parseOneLine(line) {
  const tokens = splitString(line, 7);
  if (tokens.length < 7) {
    return { isLogcat: false, originalLine: line };
  }

  const date = tokens[0];
  const time = tokens[1];
  const pid = parseInt(tokens[2], 10);
  if (isNaN(pid)) {
    return { isLogcat: false, originalLine: line };
  }
  const tid = parseInt(tokens[3], 10);
  if (isNaN(tid)) {
    return { isLogcat: false, originalLine: line };
  }
  const priority = tokens[4];
  let tag = tokens[5];
  if (tag.endsWith(':')) {
    tag = tag.slice(0, -1);
  }
  let message = tokens[6];
  if (message.startsWith(': ')) {
    message = message.slice(2);
  }

  return { isLogcat: true, date, time, pid, tid, priority, tag, message };
}

/**
 * Split the input string into at most |maxParts| parts by whitespace.
 * If there are more than |maxParts| parts, the last part will contain all the
 * rest of the string so that the return value has exactly |maxParts| parts.
 * @param {string} inputString
 * @param {number} maxParts
 * @return {Array<string>}
 */
function splitString(inputString, maxParts) {
  const parts = [];
  const whitespaceRegex = /\s+/g;
  let previousIndex = 0;

  while (parts.length < maxParts - 1) {
    const match = whitespaceRegex.exec(inputString);
    if (!match) {
      break;
    }
    parts.push(inputString.substring(previousIndex, match.index));
    previousIndex = whitespaceRegex.lastIndex;
  }

  parts.push(inputString.substring(previousIndex));
  return parts;
}

/**
 * Set up the dropdown list that filters the logcat based on process.
 */
function setUpProcessDropdownList() {
  // Remove existing single process filter options from DOM.
  displaySingleNamedProcess.forEach(filterOption => filterOption.li.remove());
  displaySingleNamedProcess = [];
  displaySingleUnnamedProcess.forEach(filterOption => filterOption.li.remove());
  displaySingleUnnamedProcess = [];

  // Automatically select 'Display All Processes' and consequently all other
  // options in the dropdown when the user uploads a new file.
  displayAllProcesses.li.classList.add('selected');
  displayAllProcesses.checkbox.checked = true;
  displayNamedProcesses.li.classList.add('selected');
  displayNamedProcesses.checkbox.checked = true;

  const sortedPid = Array.from(allPids).sort((a, b) => a - b);

  // Named processes appear before the processes with no name.
  for (const pid of sortedPid) {
    if (pidToProcessName.has(pid)) {
      createProcessListItem(pid, pidToProcessName.get(pid));
    }
  }

  for (const pid of sortedPid) {
    if (!pidToProcessName.has(pid)) {
      createProcessListItem(pid);
    }
  }
}

/**
 * Create an HTML element of type li based on the given process id and name.
 * @param {number} pid
 * @param {string} processName
 */
function createProcessListItem(pid, processName) {
  const listItem = document.createElement('li');
  listItem.dataset.value = pid;
  if (processName === undefined) {
    listItem.dataset.type = 'unnamed-process';
  } else {
    listItem.dataset.type = 'named-process';
  }
  listItem.classList.add('dropdown-item');
  listItem.classList.add('selected');

  const checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.checked = true;

  const label = document.createElement('label');
  if (processName === undefined) {
    label.textContent = `${pid} (process name unknown)`;
  } else {
    label.textContent = `${pid} (${processName})`;
  }

  listItem.appendChild(checkbox);
  listItem.appendChild(label);
  dropdownListProcess.appendChild(listItem);

  if (processName === undefined) {
    displaySingleUnnamedProcess.push({
      li: listItem,
      checkbox: checkbox,
    });
  } else {
    displaySingleNamedProcess.push({
      li: listItem,
      checkbox: checkbox,
    });
  }
}

/**
 * This function is called when the user clicks on a process filter option.
 * @param {Event} event
 */
function handleProcessFilterOptionClick(event) {
  const listItem = event.target.closest('li');
  if (!listItem) return;

  const checkbox = listItem.querySelector('input[type="checkbox"]');
  const value = listItem.dataset.value;
  const type = listItem.dataset.type;

  listItem.classList.toggle('selected');
  checkbox.checked = !checkbox.checked;
  const isSelected = checkbox.checked;

  if (type === 'main-process' && value === 'all') {
    displayNamedProcesses.li.classList.toggle('selected', isSelected);
    displayNamedProcesses.checkbox.checked = isSelected;
    for (const filterOption of displaySingleNamedProcess) {
      filterOption.li.classList.toggle('selected', isSelected);
      filterOption.checkbox.checked = isSelected;
    }
    for (const filterOption of displaySingleUnnamedProcess) {
      filterOption.li.classList.toggle('selected', isSelected);
      filterOption.checkbox.checked = isSelected;
    }
  } else if (type === 'main-process' && value === 'named') {
    for (const filterOption of displaySingleNamedProcess) {
      filterOption.li.classList.toggle('selected', isSelected);
      filterOption.checkbox.checked = isSelected;
    }
    updateDisplayAllProcessSelection();
  } else if (type === 'named-process') {
    updateDisplayNamedProcessSelection();
    updateDisplayAllProcessSelection();
  } else if (type === 'unnamed-process') {
    updateDisplayAllProcessSelection();
  }

  updateTextDisplayArea();
}

/**
 * Update the selected state of the 'Display All Processes' filter option.
 */
function updateDisplayAllProcessSelection() {
  const allSelected = displayNamedProcesses.checkbox.checked &&
    displaySingleUnnamedProcess.every(
      filterOption => filterOption.checkbox.checked);
  displayAllProcesses.li.classList.toggle('selected', allSelected);
  displayAllProcesses.checkbox.checked = allSelected;
}

/**
 * Update the selected state of the 'Display All Named Processes' filter option.
 */
function updateDisplayNamedProcessSelection() {
  const allSelected = displaySingleNamedProcess.every(
    filterOption => filterOption.checkbox.checked);
  displayNamedProcesses.li.classList.toggle('selected', allSelected);
  displayNamedProcesses.checkbox.checked = allSelected;
}

/**
 * Set up the dropdown list that filters the logcat based on tag.
 */
function setUpTagDropdownList() {
  // Remove existing single tag filter options from DOM.
  displaySingleTag.forEach(filterOption => filterOption.li.remove());
  displaySingleTag = [];

  // Automatically select 'Display All Tags' and consequently all other
  // options in the dropdown when the user uploads a new file.
  displayAllTags.li.classList.add('selected');
  displayAllTags.checkbox.checked = true;

  const sortedTags = Array.from(allTags).sort();
  for (const tag of sortedTags) {
    createTagListItem(tag);
  }
}

/**
 * Create an HTML element of type li based on the given tag.
 * @param {string} tag
 */
function createTagListItem(tag) {
  const listItem = document.createElement('li');
  listItem.dataset.value = tag;
  listItem.dataset.type = 'tag';
  listItem.classList.add('dropdown-item');
  listItem.classList.add('selected');

  const checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.checked = true;

  const label = document.createElement('label');
  label.textContent = tag;

  listItem.appendChild(checkbox);
  listItem.appendChild(label);
  dropdownListTag.appendChild(listItem);

  displaySingleTag.push({
    li: listItem,
    checkbox: checkbox,
  });
}

/**
 * This function is called when the user clicks on a tag filter option.
 * @param {Event} event
 */
function handleTagFilterOptionClick(event) {
  const listItem = event.target.closest('li');
  if (!listItem) return;

  const checkbox = listItem.querySelector('input[type="checkbox"]');
  const value = listItem.dataset.value;
  const type = listItem.dataset.type;

  listItem.classList.toggle('selected');
  checkbox.checked = !checkbox.checked;
  const isSelected = checkbox.checked;

  if (type === 'main-tag' && value === 'all') {
    for (const filterOption of displaySingleTag) {
      filterOption.li.classList.toggle('selected', isSelected);
      filterOption.checkbox.checked = isSelected;
    }
  } else if (type === 'tag') {
    updateDisplayAllTagSelection();
  }

  updateTextDisplayArea();
}

/**
 * Update the selected state of the 'Display All Tags' filter option.
 */
function updateDisplayAllTagSelection() {
  const allSelected = displaySingleTag.every(
    filterOption => filterOption.checkbox.checked);
  displayAllTags.li.classList.toggle('selected', allSelected);
  displayAllTags.checkbox.checked = allSelected;
}

/**
 * Set up the dropdown list that filters the logcat based on priority.
 */
function setUpPriorityDropdownList() {
  // Automatically select 'Display All Priorities' and consequently all other
  // options in the dropdown when the user uploads a new file.
  displayAllPriorities.li.classList.add('selected');
  displayAllPriorities.checkbox.checked = true;

  for (const filterOption of displaySinglePriority) {
    filterOption.li.classList.add('selected');
    filterOption.checkbox.checked = true;
  }
}

/**
 * This function is called when the user clicks on a priority filter option.
 * @param {Event} event
 */
function handlePriorityFilterOptionClick(event) {
  const listItem = event.target.closest('li');
  if (!listItem) return;

  const checkbox = listItem.querySelector('input[type="checkbox"]');
  const value = listItem.dataset.value;
  const type = listItem.dataset.type;

  listItem.classList.toggle('selected');
  checkbox.checked = !checkbox.checked;
  const isSelected = checkbox.checked;

  if (type === 'main-priority' && value === 'all') {
    for (const filterOption of displaySinglePriority) {
      filterOption.li.classList.toggle('selected', isSelected);
      filterOption.checkbox.checked = isSelected;
    }
  } else if (type === 'priority') {
    updateDisplayAllPrioritySelection();
  }

  updateTextDisplayArea();
}

/**
 * Update the selected state of the 'Display All Priorities' filter option.
 */
function updateDisplayAllPrioritySelection() {
  const allSelected = displaySinglePriority.every(
    filterOption => filterOption.checkbox.checked);
  displayAllPriorities.li.classList.toggle('selected', allSelected);
  displayAllPriorities.checkbox.checked = allSelected;
}

/**
 * Update the text display area with the contents of the current logcat file.
 */
function updateTextDisplayArea(restoreScrollPosition = true) {
  // Find out which process ids are selected by the user.
  const selectedProcessOptions = Array.from(
    dropdownListProcess.querySelectorAll('li.selected'));
  const selectedPids = new Set();

  for (const option of selectedProcessOptions) {
    const type = option.dataset.type;
    const value = option.dataset.value;
    if (type === 'named-process' || type === 'unnamed-process') {
      const selectedPid = parseInt(value, 10);
      if (!isNaN(selectedPid)) {
        selectedPids.add(selectedPid);
      }
    }
  }

  // Find out which tags are selected by the user.
  const selectedTagOptions = Array.from(
    dropdownListTag.querySelectorAll('li.selected'));
  const selectedTags = new Set();

  for (const option of selectedTagOptions) {
    const type = option.dataset.type;
    const value = option.dataset.value;
    if (type === 'tag') {
      selectedTags.add(value);
    }
  }

  // Find out which priorities are selected by the user.
  const selectedPriorityOptions = Array.from(
    dropdownListPriority.querySelectorAll('li.selected'));
  const selectedPriorities = new Set();

  for (const option of selectedPriorityOptions) {
    const type = option.dataset.type;
    const value = option.dataset.value;
    if (type === 'priority') {
      selectedPriorities.add(value);
    }
  }

  // Find out the line numbers that will be displayed based on the options
  // selected by the user.
  const displayedLineNumbers = [];
  const displayNonLogcatLines = displayNonLogcatCheckbox.checked;

  for (const [i, parsedLine] of currentFileParsedLines.entries()) {
    if (!parsedLine.isLogcat) {
      if (displayNonLogcatLines) {
        displayedLineNumbers.push(i);
      }
      continue;
    }

    if (selectedPids.has(parsedLine.pid) && selectedTags.has(parsedLine.tag) &&
      selectedPriorities.has(parsedLine.priority)) {
      displayedLineNumbers.push(i);
    }
  }

  // Map each line number to an HTML element with appropriate styling.
  const hideDateTime = hideDateTimeCheckbox.checked;
  const displayedHTMLElements = document.createDocumentFragment();

  for (const lineNumber of displayedLineNumbers) {
    const parsedLine = currentFileParsedLines[lineNumber];
    if (parsedLine.isLogcat) {
      const divElement = formatParsedLine(parsedLine, lineNumber, hideDateTime);
      displayedHTMLElements.appendChild(divElement);
    } else {
      const divElement = document.createElement('div');
      divElement.dataset.lineNumber = lineNumber;
      divElement.appendChild(document.createTextNode(parsedLine.originalLine));
      divElement.appendChild(document.createElement('br'));
      displayedHTMLElements.appendChild(divElement);
    }
  }

  if (!restoreScrollPosition) {
    textDisplayArea.innerHTML = '';
    textDisplayArea.appendChild(displayedHTMLElements);
    return;
  }

  // Find the first logcat line that is fully visible to the user
  // before the text display area is updated.
  const [firstVisibleLineNumber, firstVisibleLineOffset] =
    findFirstVisibleLine();

  // Update the text display area.
  textDisplayArea.innerHTML = '';
  textDisplayArea.appendChild(displayedHTMLElements);
  if (firstVisibleLineNumber === -1) {
    return;
  }

  // Scroll the window so that the firstVisibleLine becomes visible again.
  // If the firstVisibleLine no longer exists, make the next line visible.
  const logcatLines = Array.from(textDisplayArea.children);
  for (const logcatLine of logcatLines) {
    const lineNumber = parseInt(logcatLine.dataset.lineNumber, 10);
    if (!isNaN(lineNumber) && lineNumber >= firstVisibleLineNumber) {
      window.scrollTo(window.scrollX,
        logcatLine.offsetTop - firstVisibleLineOffset);
      break;
    }
  }
}

/**
 * Format a parsed logcat line into an HTML element with appropriate styling.
 * @param {ParsedLine} parsedLine
 * @param {number} lineNumber
 * @param {boolean} hideDateTime
 * @return {HTMLElement}
 */
function formatParsedLine(parsedLine, lineNumber, hideDateTime) {
  const dim = !pidToProcessName.has(parsedLine.pid);
  const pidCssClasses = getPidStyling(parsedLine.pid, parsedLine.tag, dim);
  const tidCssClasses = getTidStyling(parsedLine.tid, parsedLine.pid, dim);
  const priorityCssClasses = getPriorityStyling(parsedLine.priority);
  const tagCssClasses = pidCssClasses.slice();
  tagCssClasses.push('log-bright');
  const messageCssClasses = pidCssClasses.slice();

  const pidElement = style(
    String(parsedLine.pid).padStart(5, ' '), pidCssClasses);
  const tidElement = style(
    String(parsedLine.tid).padStart(5, ' '), tidCssClasses);
  const priorityElement = style(parsedLine.priority, priorityCssClasses);
  const tagElement = style(parsedLine.tag.padEnd(8, ' '), tagCssClasses);
  const messageElement = style(parsedLine.message, messageCssClasses);

  const divElement = document.createElement('div');
  divElement.dataset.lineNumber = lineNumber;
  if (!hideDateTime) {
    divElement.appendChild(
      document.createTextNode(`${parsedLine.date} ${parsedLine.time} `));
  }
  divElement.appendChild(pidElement);
  divElement.appendChild(document.createTextNode(' '));
  divElement.appendChild(tidElement);
  divElement.appendChild(document.createTextNode(' '));
  divElement.appendChild(priorityElement);
  divElement.appendChild(document.createTextNode(' '));
  divElement.appendChild(tagElement);
  divElement.appendChild(document.createTextNode(': '));
  divElement.appendChild(messageElement);
  divElement.appendChild(document.createElement('br'));
  return divElement;
}

/**
 * Determine the CSS classes for styling a process id.
 * @param {number} pid
 * @param {string} tag
 * @param {boolean} dim
 * @return {Array<string>}
 */
function getPidStyling(pid, tag, dim = false) {
  if (tag === 'ActivityManager' || tag === 'ActivityTaskManager') {
    return ['log-fore-black-pid'];
  }
  if (pidToProcessName.has(pid)) {
    return ['log-fore-yellow'];
  }
  if (dim) {
    return ['log-dim'];
  }
  return [];
}

/**
 * Determine the CSS classes for styling a thread id.
 * @param {number} tid
 * @param {number} pid
 * @param {boolean} dim
 * @return {Array<string>}
 */
function getTidStyling(tid, pid, dim = false) {
  if (!dim && tid === pid) {
    return ['log-bright'];
  } else {
    return ['log-normal'];
  }
}

/**
 * Determine the CSS classes for styling a log priority.
 * @param {string} priority
 * @return {Array<string>}
 */
function getPriorityStyling(priority) {
  let cssClasses = ['log-fore-black-priority'];
  if (priority === 'E' || priority === 'F') {
    cssClasses.push('log-back-red');
  } else if (priority === 'W') {
    cssClasses.push('log-back-yellow');
  } else if (priority === 'I') {
    cssClasses.push('log-back-green');
  } else if (priority === 'D') {
    cssClasses.push('log-back-blue');
  }
  return cssClasses;
}

/**
 * Apply CSS classes to the given text and wrap it in a span.
 * @param {string} text
 * @param {Array<string>} cssClasses
 * @return {HTMLElement}
 */
function style(text, cssClasses) {
  const span = document.createElement('span');
  span.textContent = text;

  if (cssClasses !== undefined && cssClasses.length !== 0) {
    span.classList.add(...cssClasses);
  }

  return span;
}

/**
 * Find the first logcat line that is fully visible to the user.
 * Return its line number and its offset from the top edge of the screen.
 * Return [-1, -1] if no such logcat line can be found.
 * @return {Array<number>}
 */
function findFirstVisibleLine() {
  const logcatLines = Array.from(textDisplayArea.children);
  const controlsHeight = controlsDiv.offsetHeight;
  const scrollPosition = window.scrollY;
  let firstVisibleLineNumber = -1;
  let firstVisibleLineOffset = -1;

  for (const logcatLine of logcatLines) {
    if (logcatLine.offsetTop - controlsHeight >= scrollPosition) {
      const lineNumber = parseInt(logcatLine.dataset.lineNumber, 10);
      if (!isNaN(lineNumber)) {
        firstVisibleLineNumber = lineNumber;
        firstVisibleLineOffset = logcatLine.offsetTop - scrollPosition;
        break;
      }
    }
  }

  return [firstVisibleLineNumber, firstVisibleLineOffset];
}

/**
 * Scroll to the next logcat line that represents a program exception.
 */
function jumpToNextException() {
  // Find the first logcat line that is fully visible to the user.
  const [firstVisibleLineNumber, _] = findFirstVisibleLine();
  if (firstVisibleLineNumber === -1) {
    return;
  }

  // Find the first line after the firstVisibleLine that represents
  // a program exception and scroll to that line.
  const logcatLines = Array.from(textDisplayArea.children);
  for (const logcatLine of logcatLines) {
    const lineNumber = parseInt(logcatLine.dataset.lineNumber, 10);
    if (!isNaN(lineNumber) && lineNumber > firstVisibleLineNumber) {
      const parsedLine = currentFileParsedLines[lineNumber];
      if (isStartOfStackTrace(parsedLine)) {
        scrollToLine(logcatLine);
        return;
      }
    }
  }
}

/**
 * Given a ParsedLine, return whether it represents the start of a stack trace.
 * @param {ParsedLine} parsedLine
 * @return {boolean}
 */
function isStartOfStackTrace(parsedLine) {
  return (parsedLine.isLogcat && parsedLine.tag === 'TestRunner' &&
    parsedLine.message === '----- begin exception -----') ||
    (!parsedLine.isLogcat && parsedLine.originalLine === 'Stack Trace:') ||
    (!parsedLine.isLogcat &&
      parsedLine.originalLine === '--------- beginning of crash');
}

/**
 * Scroll to the next logcat line that represents the start of a test.
 */
function jumpToNextTest() {
  // Find the first logcat line that is fully visible to the user.
  const [firstVisibleLineNumber, _] = findFirstVisibleLine();
  if (firstVisibleLineNumber === -1) {
    return;
  }

  // Find the first line after the firstVisibleLine that represents
  // the start of a test and scroll to that line.
  const logcatLines = Array.from(textDisplayArea.children);
  for (const logcatLine of logcatLines) {
    const lineNumber = parseInt(logcatLine.dataset.lineNumber, 10);
    if (!isNaN(lineNumber) && lineNumber > firstVisibleLineNumber) {
      const parsedLine = currentFileParsedLines[lineNumber];
      if (isStartOfTest(parsedLine)) {
        scrollToLine(logcatLine);
        return;
      }
    }
  }
}

/**
 * Given a ParsedLine, return whether it represents the start of a test.
 * @param {ParsedLine} parsedLine
 * @return {boolean}
 */
function isStartOfTest(parsedLine) {
  return parsedLine.isLogcat && parsedLine.tag === 'TestRunner' &&
    parsedLine.message.startsWith('started: ');
}

/**
 * Given an HTMLElement representing one logcat line, scroll to that line.
 * @param {HTMLElement} logcatLine
 */
function scrollToLine(logcatLine) {
  const controlsHeight = controlsDiv.offsetHeight;
  window.scrollTo(window.scrollX, logcatLine.offsetTop - controlsHeight);
}
