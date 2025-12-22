// tslint:disable:variable-name Describing an API that's defined elsewhere.

import {PolymerElementConstructor} from '../../interfaces.js';

export {PolymerElementConstructor} from '../../interfaces.js';

/** Array of Polymer element classes that have been finalized. */
export const registrations: PolymerElementConstructor[];

export {incrementInstanceCount};

declare function incrementInstanceCount(): void;

export {register};


/**
 * Registers a class prototype for telemetry purposes.
 */
declare function register(prototype: PolymerElementConstructor): void;

export {dumpRegistrations};


/**
 * Logs all elements registered with an `is` to the console.
 */
declare function dumpRegistrations(): void;
