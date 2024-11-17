// Builds valid digital identity request for navigator.identity.get() API.
export function buildValidNavigatorIdentityRequest() {
  return {
      digital: {
        providers: [{
          protocol: "openid4vp",
          request: JSON.stringify({
            // Based on https://github.com/openid/OpenID4VP/issues/125
            client_id: "client.example.org",
            client_id_scheme: "web-origin",
            nonce: "n-0S6_WzA2Mj",
            presentation_definition: {
              // Presentation Exchange request, omitted for brevity
            }
          }),
        }],
      },
  };
}

// Builds a valid navigator.identity.get() request where
// IdentityRequestProvider#request is an object.
export function buildValidNavigatorIdentityRequestWithRequestObject() {
  return {
      digital: {
        providers: [{
          protocol: "openid4vp",
          request: {
            // Based on https://github.com/openid/OpenID4VP/issues/125
            client_id: "client.example.org",
            client_id_scheme: "web-origin",
            nonce: "n-0S6_WzA2Mj",
            presentation_definition: {
              // Presentation Exchange request, omitted for brevity
            }
          },
        }],
      },
  };
}

// Requests digital identity with user activation.
export function requestIdentityWithActivation(test_driver, request) {
  return test_driver.bless("request identity with activation", async function() {
    return await navigator.identity.get(request);
  });
}

/**
 * Checks digital credential API availability.
 *
 * Different than requestIdentityWithActivation() in that this function:
 * - does not do full digital credentials request
 * - does not acquire transient user activation and thus is unaffected by test
 *   driver bugs related to transient user activation.
 *   https://github.com/web-platform-tests/wpt/issues/46989
 *   http://crbug.com/40124744
 */
export async function check_digital_credential_api_availability() {
  try {
    const request = buildValidNavigatorIdentityRequest();
    request.digital.providers = [];
    await navigator.identity.get(request);
    return false;
  } catch (error) {
    // If digital credentials API is disabled, an error due to the API being
    // disabled should be thrown prior to checking whether the request has
    // transient user activation and non-empty providers.
    if (error instanceof DOMException && error.name == "NotAllowedError" &&
        error.message != null &&
        error.message.includes("transient activation")) {
      return true;
    }
    return (error instanceof TypeError)
  }
}

export function digital_credential_test_feature_availability_in_iframe(test, url, expect_feature_available, allow) {
  return test_feature_availability(
      'digital-credentials-get', test, url, expect_feature_available, /*feature_name=*/allow, /*allowfullscreen=*/false, /*is_promise_test=*/true);
}
