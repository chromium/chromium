/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
const r=(r,t)=>r===t||r.length===t.length&&r.every(((r,e)=>i(r,t[e]))),t=Object.prototype.valueOf,e=Object.prototype.toString,{keys:n}=Object,{isArray:f}=Array,i=(r,o)=>{if(Object.is(r,o))return!0;if(null!==r&&null!==o&&"object"==typeof r&&"object"==typeof o){if(r.constructor!==o.constructor)return!1;if(f(r))return r.length===o.length&&r.every(((r,t)=>i(r,o[t])));if(r.valueOf!==t)return r.valueOf()===o.valueOf();if(r.toString!==e)return r.toString()===o.toString();if(r instanceof Map&&o instanceof Map){if(r.size!==o.size)return!1;for(const[t,e]of r.entries())if(!1===i(e,o.get(t))||void 0===e&&!1===o.has(t))return!1;return!0}if(r instanceof Set&&o instanceof Set){if(r.size!==o.size)return!1;for(const t of r.keys())if(!1===o.has(t))return!1;return!0}if(r instanceof RegExp)return r.source===o.source&&r.flags===o.flags;const u=n(r);if(u.length!==n(o).length)return!1;for(const t of u)if(!o.hasOwnProperty(t)||!i(r[t],o[t]))return!1;return!0}return!1};export{r as deepArrayEquals,i as deepEquals};
//# sourceMappingURL=deep-equals.js.map
