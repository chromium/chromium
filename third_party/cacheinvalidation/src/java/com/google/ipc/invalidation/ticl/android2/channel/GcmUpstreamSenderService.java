/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.ipc.invalidation.ticl.android2.channel;

import com.google.ipc.invalidation.common.GcmSharedConstants;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.android2.ProtocolIntents;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelPreferences.GcmChannelType;
import com.google.ipc.invalidation.ticl.proto.AndroidService.AndroidNetworkSendRequest;
import com.google.ipc.invalidation.ticl.proto.ChannelCommon.NetworkEndpointId;
import com.google.ipc.invalidation.ticl.proto.CommonProtos;
import com.google.ipc.invalidation.util.ProtoWrapper.ValidationException;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Base64;


/**
 * Base class to send upstream messages using the GCM channel. Creates the bundle to send using
 * GoogleCloudMessaging#send. The client should add an OAuth2 token to bundle before sending
 * the message.
 *
 * <p>A sample implementation of an {@link GcmUpstreamSenderService} is shown below:
 *
 * <p><code>
 * class ExampleSenderService extends GcmUpstreamSenderService {
 *   @Override
 *   public void deliverMessage(String to, Bundle data) {
 *     String messageId = ...;
 *     String oAuth2Token = ...;
 *     data.putString("Authorization", "Bearer " + oAuth2Token);
 *     GoogleCloudMessaging.getInstance(this).send(to, messageId, data);
 *   }
 * }
 * </code>
 */
public abstract class GcmUpstreamSenderService extends IntentService {

  private static final Logger logger = AndroidLogger.forTag("GcmMsgSenderSvc");

  public GcmUpstreamSenderService() {
    super("GcmUpstreamService");
    setIntentRedelivery(true);
  }

  @Override
  protected void onHandleIntent(Intent intent) {
      if (AndroidChannelPreferences.getGcmChannelType() != GcmChannelType.GCM_UPSTREAM) {
          logger.warning("Incorrect channel type for using GCM Upstream");
          return;
      }
    if (intent == null) {
      return;
    }

    if (intent.hasExtra(ProtocolIntents.OUTBOUND_MESSAGE_KEY)) {
      // Request from the Ticl service to send a message.
      handleOutboundMessage(intent.getByteArrayExtra(ProtocolIntents.OUTBOUND_MESSAGE_KEY));
    } else if (intent.hasExtra(AndroidChannelConstants.MESSAGE_SENDER_SVC_GCM_REGID_CHANGE)) {
      handleGcmRegIdChange();
    } else {
      logger.warning("Ignoring intent: %s", intent);
    }
  }

  /**
   * Handles a request to send a message to the data center. Validates the message and creates the
   * Bundle to be sent in the upstream message.
   */
  private void handleOutboundMessage(byte[] sendRequestBytes) {
    // Parse and validate the send request.
    final AndroidNetworkSendRequest sendRequest;
    try {
       sendRequest = AndroidNetworkSendRequest.parseFrom(sendRequestBytes);
    } catch (ValidationException exception) {
      logger.warning("Invalid AndroidNetworkSendRequest from %s: %s",
          sendRequestBytes, exception);
      return;
    }

    byte[] message = sendRequest.getMessage().getByteArray();
    sendUpstreamMessage(message);
  }
  
  /**
   * Handles a change in the GCM registration token by sending the buffered client message (if any)
   * to the data center.
   */
  private void handleGcmRegIdChange() {
      byte[] bufferedMessage = AndroidChannelPreferences.takeBufferedMessage();
      if (bufferedMessage != null) {
          sendUpstreamMessage(bufferedMessage);
      }
  }
  
  /**
   * Creates the Bundle for sending the {@code message}. Encodes the message and the network
   * endpoint id and adds it to the bundle.
   */
  private void sendUpstreamMessage(byte[] message) {
    NetworkEndpointId endpointId = getNetworkEndpointId(this);
    if (endpointId == null) {
      logger.info("Buffering message to the data center: no GCM registration id");
      AndroidChannelPreferences.bufferMessage(message);
      return;
    }
    Bundle dataBundle = new Bundle();

    // Add the encoded android endpoint id to the bundle
    dataBundle.putString(GcmSharedConstants.NETWORK_ENDPOINT_ID_KEY,
        base64Encode(endpointId.toByteArray()));

    // Add the encoded message to the bundle
    dataBundle.putString(GcmSharedConstants.CLIENT_TO_SERVER_MESSAGE_KEY,
        base64Encode(message));
    logger.info("Encoded message: %s", base64Encode(message));

    // Currently we do not check for message size limits since this will be run as an experiment.
    // Feedback from the experiment will be used to decide whether handling of message size
    // limit is required.
    deliverMessage(GcmSharedConstants.GCM_UPDATED_SENDER_ID + "@google.com", dataBundle);
  }

  /**
   * Implemented by the client to deliver the message using GCM.
   */
  protected abstract void deliverMessage(String to, Bundle data);

  /** Returns the endpoint id for this channel, or {@code null} if one cannot be determined. */
  
  
  static NetworkEndpointId getNetworkEndpointId(Context context) {
      String registrationToken = AndroidChannelPreferences.getRegistrationToken();
      if (registrationToken == null || registrationToken.isEmpty()) {
          logger.warning("No GCM registration token; cannot determine our network endpoint id: %s",
                  registrationToken);
          return null;
      }
    return CommonProtos.newAndroidEndpointId(registrationToken,
        GcmSharedConstants.ANDROID_ENDPOINT_ID_CLIENT_KEY,
        context.getPackageName(), AndroidChannelConstants.CHANNEL_VERSION);
  }

  /** Returns a base-64 encoded version of {@code bytes}. */
  private static String base64Encode(byte[] bytes) {
    logger.info("Encoding message: %s", bytes);
    return Base64.encodeToString(bytes, Base64.NO_WRAP);
  }
}

