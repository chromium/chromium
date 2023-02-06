// Run sharedStorage.worklet.addModule once.
// @param {string} module - The URL to the module.
async function addModuleOnce(module) {
  try {
    await sharedStorage.worklet.addModule(module);
  } catch (e) {
    // Shared Storage needs to have a module added before we can operate on it.
    // It is generated on the fly with this call, and since there's no way to
    // tell through the API if a module already exists, wrap the addModule call
    // in a try/catch so that if it runs a second time in a test, it will
    // gracefully fail rather than bring the whole test down.
  }
}

// Validate the type of the result of sharedStorage.worklet.selectURL.
// @param result - The result of sharedStorage.worklet.selectURL.
// @param {boolean} - Whether sharedStorage.worklet.selectURL is resolved to
//                    a fenced frame config (true) or an urn:uuid (false).
// @return {boolean} Whether sharedStorage.worklet.selectURL returns an expected
//                   result type or not.
function validateSelectURLResult(result, resolve_to_config) {
  if (resolve_to_config) {
    return result instanceof FencedFrameConfig;
  }

  return result.startsWith('urn:uuid:');
}