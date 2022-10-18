// tslint:disable:variable-name Describing an API that's defined elsewhere.

import {Class} from './class.js';


/**
 * Legacy class factory and registration helper for defining Polymer
 * elements.
 *
 * This method is equivalent to
 *
 *     import {Class} from '@polymer/polymer/lib/legacy/class.js';
 *     customElements.define(info.is, Class(info));
 *
 * See `Class` for details on valid legacy metadata format for `info`.
 *
 * @returns Generated class
 */
export const Polymer: {
  (info: PolymerInit): {new(): HTMLElement};
  Class: typeof Class;
}

import {PolymerInit} from '../../interfaces';
