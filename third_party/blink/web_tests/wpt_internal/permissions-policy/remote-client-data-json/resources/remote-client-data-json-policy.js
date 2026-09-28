'use strict';

const remoteClientDataJsonPolicyFeature =
    'publickey-credentials-remote-client-data-json';
const baseWebAuthnPolicyFeatures =
    'publickey-credentials-create; publickey-credentials-get';
const allRemoteClientDataJsonPolicyFeatures =
    `${baseWebAuthnPolicyFeatures}; ${remoteClientDataJsonPolicyFeature}`;

function getWithInvalidRemoteClientDataJson() {
  return navigator.credentials.get({
    publicKey: {
      rpId: 'example.test',
      challenge: new Uint8Array(16),
      extensions: {remoteClientDataJSON: '{}'},
    }
  });
}

async function getRejection(operation) {
  try {
    await operation();
  } catch (error) {
    return error;
  }
  throw new Error('WebAuthn request unexpectedly succeeded');
}

async function useRemoteClientDataJsonPolicyFeature() {
  const error = await getRejection(getWithInvalidRemoteClientDataJson);
  if (error.name === 'EncodingError') {
    return;
  }
  throw error;
}

function runRemoteClientDataJsonHeaderPolicyTests(crossOrigin, expected,
                                                  description) {
  if (page_loaded_in_iframe()) {
    test_feature_in_iframe(remoteClientDataJsonPolicyFeature,
                           useRemoteClientDataJsonPolicyFeature);
    return;
  }

  promise_test(t => expected.topLevel ?
                   useRemoteClientDataJsonPolicyFeature() :
                   promise_rejects_dom(t, 'NotAllowedError',
                                       useRemoteClientDataJsonPolicyFeature()),
               `${description} in the top-level document.`);

  const sameOriginSrc = same_origin_url(remoteClientDataJsonPolicyFeature);
  promise_test(
      t => test_feature_availability_with_post_message_result(
          t, sameOriginSrc, expected.sameOrigin ? '#OK' : 'NotAllowedError',
          allRemoteClientDataJsonPolicyFeatures),
      `${description} in a same-origin iframe.`);

  const crossOriginSrc =
      cross_origin_url(crossOrigin, remoteClientDataJsonPolicyFeature);
  promise_test(
      t => test_feature_availability_with_post_message_result(
          t, crossOriginSrc, expected.crossOrigin ? '#OK' : 'NotAllowedError',
          allRemoteClientDataJsonPolicyFeatures),
      `${description} in a cross-origin iframe.`);
}

function runRemoteClientDataJsonDefaultPolicyTests(crossOrigin) {
  if (page_loaded_in_iframe()) {
    test_feature_in_iframe(remoteClientDataJsonPolicyFeature,
                           useRemoteClientDataJsonPolicyFeature);
    return;
  }

  promise_test(
      () => useRemoteClientDataJsonPolicyFeature(),
      'The default self allowlist enables the feature in the top-level ' +
          'document.');

  const sameOriginSrc = same_origin_url(remoteClientDataJsonPolicyFeature);
  promise_test(
      t => test_feature_availability_with_post_message_result(
          t, sameOriginSrc, '#OK', baseWebAuthnPolicyFeatures),
      'The default self allowlist enables the feature in a same-origin ' +
          'iframe.');

  const crossOriginSrc =
      cross_origin_url(crossOrigin, remoteClientDataJsonPolicyFeature);
  promise_test(
      t => test_feature_availability_with_post_message_result(
          t, crossOriginSrc, 'NotAllowedError', baseWebAuthnPolicyFeatures),
      'The default self allowlist disables the feature in a cross-origin ' +
          'iframe.');

  promise_test(
      t => test_feature_availability_with_post_message_result(
          t, crossOriginSrc, '#OK', allRemoteClientDataJsonPolicyFeatures),
      'The allow attribute delegates the feature to a cross-origin iframe.');

  promise_test(
      t => test_feature_availability_with_post_message_result(
          t, sameOriginSrc, 'NotAllowedError',
          `${baseWebAuthnPolicyFeatures}; ` +
              `${remoteClientDataJsonPolicyFeature} 'none'`),
      'The allow attribute disables the feature in a same-origin iframe.');
}
