// * |platformSpecific| determines the platform-filtering of features. Only
//   platform-specific features will be tested if set to true, and only
//   all-platform features will be used if set to false.
// * |outputFunc| is called back with each line of output.

function featurePolicyFeaturesListing(platformSpecific, outputFunc) {
  // List of all policy-controlled platform-specific features. Please update
  // this list when adding a new policy-controlled platform-specific interface.
  // This enables us to keep the churn on the platform-specific expectations
  // files to a bare minimum to make updates in the common (platform-neutral)
  // case as simple as possible.
  var platformSpecificFeatures = new Set([
    'bluetooth',
  ]);

  function filterPlatformSpecificFeature(featureName) {
    return platformSpecificFeatures.has(featureName) == platformSpecific;
  }

  for (const feature of document.featurePolicy.features()
           .filter(filterPlatformSpecificFeature)
           .sort()) {
    outputFunc(feature);
  }
}