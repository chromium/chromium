// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {htmlEscape} from '../../../common/js/dom_utils.js';
import {FileType} from '../../../common/js/file_type.js';
import {strf} from '../../../common/js/util.js';

import {AutocompleteList} from './autocomplete_list.js';
import {ListItem} from './list_item.js';


/**
 * @fileoverview
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

/**
 * Type of the item stored in the autocomplete list.
 */
export class SearchItem {
  isHeaderItem: boolean = false;
  searchQuery: string = '';
}

export type SearchOrMetaItem =
    SearchItem|chrome.fileManagerPrivate.DriveMetadataSearchResult;

/**
 * Customizes list items shown in the autocomplete list. If the item is a header
 * item, it gets extra text attached to it. For drive items it gets icon
 * decorations.
 */
export class SearchAutocompleteListItem extends ListItem {
  itemInfo: SearchOrMetaItem;

  constructor(document: Document, item: SearchOrMetaItem) {
    super();
    this.itemInfo = item;

    const icon = document.createElement('div');
    icon.className = 'detail-icon';

    const text = document.createElement('div');
    text.className = 'detail-text';

    if ('isHeaderItem' in item) {
      const searchItem = item as SearchItem;
      icon.setAttribute('search-icon', '');
      text.innerHTML =
          strf('SEARCH_DRIVE_HTML', htmlEscape(searchItem.searchQuery));
    } else {
      const driveItem =
          item as chrome.fileManagerPrivate.DriveMetadataSearchResult;
      const iconType = FileType.getIcon(driveItem.entry);
      icon.setAttribute('file-type-icon', iconType);
      // highlightedBaseName is a piece of HTML with meta characters properly
      // escaped. See the comment at fileManagerPrivate.searchDriveMetadata().
      text.innerHTML = driveItem.highlightedBaseName;
    }
    this.appendChild(icon);
    this.appendChild(text);
  }
}

/**
 * Autocomplete list displaying suggested matches for the Drive search.
 */
export class SearchAutocompleteList extends AutocompleteList {
  autoExpands: boolean;
  selectedItem: any;

  constructor(document: Document) {
    super();
    (this as any).__proto__ = SearchAutocompleteList.prototype;
    this.id = 'autocomplete-list';
    this.autoExpands = true;
    this.itemConstructor = SearchAutocompleteListItem.bind(null, document);
    this.addEventListener('mouseover', this.onMouseOver_.bind(this));
  }

  /**
   * Do nothing when a suggestion is selected.
   */
  override handleSelectedSuggestion(_suggestions: any) {}

  /**
   * Change the selection by a mouse over instead of just changing the
   * color of moused over element with :hover in CSS. Here's why:
   *
   * 1) The user selects an item A with up/down keys (item A is highlighted)
   * 2) Then the user moves the cursor to another item B
   *
   * If we just change the color of moused over element (item B), both
   * the item A and B are highlighted. This is bad. We should change the
   * selection so only the item B is highlighted.
   */
  private onMouseOver_(event: Event) {
    const target: any = event.target;
    if (target && target.itemInfo) {
      this.selectedItem = target.itemInfo;
    }
  }
}
