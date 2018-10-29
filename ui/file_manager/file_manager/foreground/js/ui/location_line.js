// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Location line.
 *
 * @extends {cr.EventTarget}
 * @param {!Element} breadcrumbs Container element for breadcrumbs.
 * @param {!VolumeManager} volumeManager Volume manager.
 * @constructor
 */
function LocationLine(breadcrumbs, volumeManager) {
  this.breadcrumbs_ = breadcrumbs;
  this.volumeManager_ = volumeManager;
  this.entry_ = null;
  this.components_ = [];
}

/**
 * Extends cr.EventTarget.
 */
LocationLine.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Shows breadcrumbs. This operation is done without IO.
 *
 * @param {!Entry|!FakeEntry} entry Target entry or fake entry.
 */
LocationLine.prototype.show = function(entry) {
  if (entry === this.entry_)
    return;

  this.update_(this.getComponents_(entry));
};

/**
 * Returns current path components built by the current directory entry.
 * @return {!Array<!LocationLine.PathComponent>} Current path components.
 */
LocationLine.prototype.getCurrentPathComponents = function() {
  return this.components_;
};

/**
 * Replace the root directory name at the end of a url.
 * The input, |url| is a displayRoot URL of a Drive volume like
 * filesystem:chrome-extension://....foo.com-hash/root
 * The output is like:
 * filesystem:chrome-extension://....foo.com-hash/other
 *
 * @param {string} url which points to a volume display root
 * @param {string} newRoot new root directory name
 * @return {string} new URL with the new root directory name
 * @private
 */
LocationLine.prototype.replaceRootName_ = function(url, newRoot) {
  return url.slice(0, url.length - '/root'.length) + newRoot;
};

/**
 * Get components for the path of entry.
 * @param {!Entry|!FilesAppEntry} entry An entry.
 * @return {!Array<!LocationLine.PathComponent>} Components.
 * @private
 */
LocationLine.prototype.getComponents_ = function(entry) {
  var components = [];
  var locationInfo = this.volumeManager_.getLocationInfo(entry);

  if (!locationInfo)
    return components;

  if (util.isFakeEntry(entry)) {
    components.push(new LocationLine.PathComponent(
        util.getRootTypeLabel(locationInfo), entry.toURL(),
        /** @type {!FakeEntry} */ (entry)));
    return components;
  }

  // Add volume component.
  var displayRootUrl = locationInfo.volumeInfo.displayRoot.toURL();
  var displayRootFullPath = locationInfo.volumeInfo.displayRoot.fullPath;

  var prefixEntry = locationInfo.volumeInfo.prefixEntry;
  if (prefixEntry) {
    components.push(new LocationLine.PathComponent(
        prefixEntry.name, prefixEntry.toURL(), prefixEntry));
  }
  if (locationInfo.rootType === VolumeManagerCommon.RootType.DRIVE_OTHER) {
    // When target path is a shared directory, volume should be shared with me.
    const match = entry.fullPath.match(/\/\.files-by-id\/\d+\//);
    if (match) {
      displayRootFullPath = match[0];
    } else {
      displayRootFullPath = '/other';
    }
    displayRootUrl = this.replaceRootName_(displayRootUrl, displayRootFullPath);
    var sharedWithMeFakeEntry = locationInfo.volumeInfo.fakeEntries[
        VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
    components.push(new LocationLine.PathComponent(
        str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
        sharedWithMeFakeEntry.toURL(),
        sharedWithMeFakeEntry));
  } else if (
      locationInfo.rootType === VolumeManagerCommon.RootType.TEAM_DRIVE) {
    displayRootUrl = this.replaceRootName_(
        displayRootUrl, VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH);
    components.push(new LocationLine.PathComponent(
        util.getRootTypeLabel(locationInfo), displayRootUrl));
  } else {
    components.push(new LocationLine.PathComponent(
        util.getRootTypeLabel(locationInfo), displayRootUrl));
  }

  // Get relative path to display root (e.g. /root/foo/bar -> foo/bar).
  var relativePath = entry.fullPath.slice(displayRootFullPath.length);
  if (entry.fullPath.startsWith(
          VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH)) {
    relativePath = entry.fullPath.slice(
        VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH.length);
  }
  if (relativePath.indexOf('/') === 0) {
    relativePath = relativePath.slice(1);
  }
  if (relativePath.length === 0)
    return components;

  // currentUrl should be without trailing slash.
  var currentUrl = /^.+\/$/.test(displayRootUrl) ?
      displayRootUrl.slice(0, displayRootUrl.length - 1) : displayRootUrl;

  // Add directory components to the target path.
  var paths = relativePath.split('/');
  for (var i = 0; i < paths.length; i++) {
    currentUrl += '/' + encodeURIComponent(paths[i]);
    components.push(new LocationLine.PathComponent(paths[i], currentUrl));
  }

  return components;
};

/**
 * Updates the breadcrumb display.
 * @param {!Array<!LocationLine.PathComponent>} components Components to the
 *     target path.
 * @private
 */
LocationLine.prototype.update_ = function(components) {
  this.components_ = components;

  // Make the new breadcrumbs temporarily.
  var newBreadcrumbs = document.createElement('div');
  for (var i = 0; i < components.length; i++) {
    // Add a component.
    var component = components[i];
    var button = document.createElement('button');
    button.id = 'breadcrumb-path-' + i;
    button.classList.add(
        'breadcrumb-path', 'entry-name', 'imitate-paper-button');
    var nameElement = document.createElement('div');
    nameElement.classList.add('name');
    nameElement.textContent = component.name;
    button.appendChild(nameElement);
    button.addEventListener('click', this.onClick_.bind(this, i));
    newBreadcrumbs.appendChild(button);

    var ripple = document.createElement('paper-ripple');
    ripple.classList.add('recenteringTouch');
    ripple.setAttribute('fit', '');
    button.appendChild(ripple);

    // If this is the last component, break here.
    if (i === components.length - 1)
      break;

    // Add a separator.
    var separator = document.createElement('span');
    separator.classList.add('separator');
    newBreadcrumbs.appendChild(separator);
  }

  // Replace the shown breadcrumbs with the new one, keeping the DOMs for common
  // prefix of the path.
  // 1. Forward the references to the path element while in the common prefix.
  var childOriginal = this.breadcrumbs_.firstChild;
  var childNew = newBreadcrumbs.firstChild;
  var cnt = 0;
  while (childOriginal && childNew &&
         childOriginal.textContent === childNew.textContent) {
    childOriginal = childOriginal.nextSibling;
    childNew = childNew.nextSibling;
    cnt++;
  }
  // 2. Remove all elements in original breadcrumbs which are not in the common
  // prefix.
  while (childOriginal) {
    var childToRemove = childOriginal;
    childOriginal = childOriginal.nextSibling;
    this.breadcrumbs_.removeChild(childToRemove);
  }
  // 3. Append new elements after the common prefix.
  while (childNew) {
    var childToAppend = childNew;
    childNew = childNew.nextSibling;
    this.breadcrumbs_.appendChild(childToAppend);
  }
  // 4. Reset the tab index and class 'breadcrumb-last'.
  for (var el = this.breadcrumbs_.firstChild; el; el = el.nextSibling) {
    if (el.classList.contains('breadcrumb-path')) {
      var isLast = !el.nextSibling;
      el.tabIndex = isLast ? -1 : 9;
      el.classList.toggle('breadcrumb-last', isLast);
    }
  }

  this.breadcrumbs_.hidden = false;
  this.truncate();
};

/**
 * Updates breadcrumbs widths in order to truncate it properly.
 */
LocationLine.prototype.truncate = function() {
  if (!this.breadcrumbs_.firstChild)
    return;

  // Assume style.width == clientWidth (items have no margins).

  for (var item = this.breadcrumbs_.firstChild; item; item = item.nextSibling) {
    item.removeAttribute('style');
    item.removeAttribute('collapsed');
    item.removeAttribute('hidden');
  }

  var containerWidth = this.breadcrumbs_.getBoundingClientRect().width;

  var pathWidth = 0;
  var currentWidth = 0;
  var lastSeparator;
  for (var item = this.breadcrumbs_.firstChild; item; item = item.nextSibling) {
    if (item.className == 'separator') {
      pathWidth += currentWidth;
      currentWidth = item.getBoundingClientRect().width;
      lastSeparator = item;
    } else {
      currentWidth += item.getBoundingClientRect().width;
    }
  }
  if (pathWidth + currentWidth <= containerWidth)
    return;
  if (!lastSeparator) {
    this.breadcrumbs_.lastChild.style.width =
        Math.min(currentWidth, containerWidth) + 'px';
    return;
  }
  var lastCrumbSeparatorWidth = lastSeparator.getBoundingClientRect().width;
  // Current directory name may occupy up to 70% of space or even more if the
  // path is short.
  var maxPathWidth = Math.max(Math.round(containerWidth * 0.3),
                              containerWidth - currentWidth);
  maxPathWidth = Math.min(pathWidth, maxPathWidth);

  var parentCrumb = lastSeparator.previousSibling;

  // Pre-calculate the minimum width for crumbs.
  parentCrumb.setAttribute('collapsed', '');
  var minCrumbWidth = parentCrumb.getBoundingClientRect().width;
  parentCrumb.removeAttribute('collapsed');

  var collapsedWidth = 0;
  if (parentCrumb &&
      pathWidth - parentCrumb.getBoundingClientRect().width + minCrumbWidth >
          maxPathWidth) {
    // At least one crumb is hidden completely (or almost completely).
    // Show sign of hidden crumbs like this:
    // root > some di... > ... > current directory.
    parentCrumb.setAttribute('collapsed', '');
    collapsedWidth = Math.min(maxPathWidth,
                              parentCrumb.getBoundingClientRect().width);
    maxPathWidth -= collapsedWidth;
    if (parentCrumb.getBoundingClientRect().width != collapsedWidth)
      parentCrumb.style.width = collapsedWidth + 'px';

    lastSeparator = parentCrumb.previousSibling;
    if (!lastSeparator)
      return;
    collapsedWidth += lastSeparator.clientWidth;
    maxPathWidth = Math.max(0, maxPathWidth - lastSeparator.clientWidth);
  }

  pathWidth = 0;
  for (var item = this.breadcrumbs_.firstChild; item != lastSeparator;
       item = item.nextSibling) {
    // TODO(serya): Mixing access item.clientWidth and modifying style and
    // attributes could cause multiple layout reflows.
    if (pathWidth === maxPathWidth) {
      item.setAttribute('hidden', '');
    } else {
      if (item.classList.contains('separator')) {
        // If the current separator and the following crumb don't fit in the
        // breadcrumbs area, hide remaining separators and crumbs.
        if (pathWidth + item.getBoundingClientRect().width + minCrumbWidth >
                maxPathWidth) {
          item.setAttribute('hidden', '');
          maxPathWidth = pathWidth;
        } else {
          pathWidth += item.getBoundingClientRect().width;
        }
      } else {
        // If the current crumb doesn't fully fit in the breadcrumbs area,
        // shorten the crumb and hide remaining separators and crums.
        if (pathWidth + item.getBoundingClientRect().width > maxPathWidth) {
          item.style.width = (maxPathWidth - pathWidth) + 'px';
          pathWidth = maxPathWidth;
        } else {
          pathWidth += item.getBoundingClientRect().width;
        }
      }
    }
  }

  currentWidth = Math.min(currentWidth,
                          containerWidth - pathWidth - collapsedWidth);
  this.breadcrumbs_.lastChild.style.width =
      (currentWidth - lastCrumbSeparatorWidth) + 'px';
};

/**
 * Hide breadcrumbs div.
 */
LocationLine.prototype.hide = function() {
  this.breadcrumbs_.hidden = true;
};

/**
 * Execute an element.
 * @param {number} index The index of clicked path component.
 * @param {!Event} event The MouseEvent object.
 * @private
 */
LocationLine.prototype.onClick_ = function(index, event) {
  if (index >= this.components_.length - 1)
    return;

  // Remove 'focused' state from the clicked button.
  var button = event.target;
  while (button && !button.classList.contains('breadcrumb-path'))
    button = button.parentElement;
  if (button)
    button.blur();

  var pathComponent = this.components_[index];
  pathComponent.resolveEntry().then(function(entry) {
    var pathClickEvent = new Event('pathclick');
    pathClickEvent.entry = entry;
    this.dispatchEvent(pathClickEvent);
  }.bind(this));
  metrics.recordUserAction('ClickBreadcrumbs');
};

/**
 * Path component.
 * @param {string} name Name.
 * @param {string} url Url.
 * @param {FilesAppEntry=} opt_fakeEntry Fake entry should be set when
 *     this component represents fake entry.
 * @constructor
 * @struct
 */
LocationLine.PathComponent = function(name, url, opt_fakeEntry) {
  this.name = name;
  this.url_ = url;
  this.fakeEntry_ = opt_fakeEntry || null;
};

/**
 * Resolve an entry of the component.
 * @return {!Promise<!Entry|!FilesAppEntry>} A promise which is
 *     resolved with an entry.
 */
LocationLine.PathComponent.prototype.resolveEntry = function() {
  if (this.fakeEntry_)
    return /** @type {!Promise<!Entry|!FilesAppEntry>} */ (
        Promise.resolve(this.fakeEntry_));
  else
    return new Promise(
        window.webkitResolveLocalFileSystemURL.bind(null, this.url_));
};
