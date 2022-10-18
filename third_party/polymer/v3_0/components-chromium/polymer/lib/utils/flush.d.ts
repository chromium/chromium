// tslint:disable:variable-name Describing an API that's defined elsewhere.

import {enqueueDebouncer, flushDebouncers} from '../utils/debounce.js';

export {enqueueDebouncer};

export {flush};


/**
 * Forces several classes of asynchronously queued tasks to flush:
 * - Debouncers added via `enqueueDebouncer`
 * - ShadyDOM distribution
 */
declare function flush(): void;
