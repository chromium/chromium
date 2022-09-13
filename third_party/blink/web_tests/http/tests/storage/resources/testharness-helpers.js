function timePasses(delay) {
  return new Promise((resolve, reject) => step_timeout(() => resolve(), delay));
}

var ORIGINAL_HOST  = "example.test";
var TEST_ROOT = "not-example.test";
var TEST_HOST = "storage." + TEST_ROOT;
var TEST_SUB  = "subdomain." + TEST_HOST;
