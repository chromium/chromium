export const SAME_SITE_HELPER_URL = '/wpt_internal/digital-goods/resources/iframe-helper.html';

// A different origin serving the same data.
export const CROSS_SITE_HELPER_URL =
  'https://{{hosts[alt][]}}:{{ports[https][0]}}'
  + SAME_SITE_HELPER_URL;

// A subdomain serving the same data.
export const SUBDOMAIN_HELPER_URL =
  'https://www1.{{hosts[][]}}:{{ports[https][0]}}'
  + SAME_SITE_HELPER_URL;
