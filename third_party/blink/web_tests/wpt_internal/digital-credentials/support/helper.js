// Builds valid digital identity request for navigator.identity.get() API.
export function buildValidNavigatorIdentityRequest() {
  return {
      digital: {
        providers: [{
          protocol: "urn:openid.net:oid4vp",
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
          protocol: "urn:openid.net:oid4vp",
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
 * @type {SendMessage}
 **/
export function sendMessage(iframe) {
  return new Promise((resolve, reject) => {
    window.addEventListener("message", function messageListener(event) {
      if (event.source === iframe.contentWindow) {
        window.removeEventListener("message", messageListener);
        resolve(event.data);
      }
    });
    if (!iframe.contentWindow) {
      reject(
        new Error("iframe.contentWindow is undefined, cannot send message.")
      );
      return;
    }
    iframe.contentWindow.postMessage({}, "*");
  });
}

/**
 * @param {HTMLIFrameElement} iframe
 * @param {string|URL} url
 * @returns {Promise<void>}
 */
export function loadIframe(iframe, url) {
  return new Promise((resolve, reject) => {
    iframe.addEventListener("load", resolve, { once: true });
    iframe.addEventListener("error", reject, { once: true });
    if (!iframe.isConnected) {
      document.body.appendChild(iframe);
    }
    iframe.src = url.toString();
  });
}
