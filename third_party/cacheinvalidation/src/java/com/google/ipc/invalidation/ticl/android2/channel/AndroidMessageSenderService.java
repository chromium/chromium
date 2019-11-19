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

import com.google.android.gcm.GCMRegistrar;
import com.google.ipc.invalidation.common.GcmSharedConstants;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.android2.AndroidTiclManifest;
import com.google.ipc.invalidation.ticl.android2.ProtocolIntents;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelConstants.AuthTokenConstants;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelConstants.HttpConstants;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelPreferences.GcmChannelType;
import com.google.ipc.invalidation.ticl.proto.AndroidService.AndroidNetworkSendRequest;
import com.google.ipc.invalidation.ticl.proto.ChannelCommon.NetworkEndpointId;
import com.google.ipc.invalidation.ticl.proto.CommonProtos;
import com.google.ipc.invalidation.util.Preconditions;
import com.google.ipc.invalidation.util.ProtoWrapper.ValidationException;

import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Base64;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.URL;
import java.util.Arrays;


/**
 * Service that sends messages to the data center using HTTP POSTs authenticated as a Google
 * account.
 * <p>
 * Messages are sent as byte-serialized {@code ClientToServerMessage} protocol buffers.
 * Additionally, the POST requests echo the latest value of the echo token received on C2DM
 * messages from the data center.
 *
 */
public class AndroidMessageSenderService extends IntentService {
  /* This class is public so that it can be instantiated by the Android runtime. */

  /**
   * A prefix on the "auth token type" that indicates we're using an OAuth2 token to authenticate.
   */
  private static final String OAUTH2_TOKEN_TYPE_PREFIX = "oauth2:";

  /** An override of the URL, for testing. */
  private static String channelUrlForTest = null;

  private final Logger logger = AndroidLogger.forTag("MsgSenderSvc");

  /** The last message sent, for tests. */
  public static byte[] lastTiclMessageForTest = null;

  public AndroidMessageSenderService() {
    super("AndroidNetworkService");
    setIntentRedelivery(true);
  }

  @Override
  public void onCreate() {
    super.onCreate();

    // HTTP connection reuse was buggy pre-Froyo, so disable it on those platforms.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.FROYO) {
      System.setProperty("http.keepAlive", "false");
    }
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    if (intent == null) {
      return;
    }

    if (intent.hasExtra(ProtocolIntents.OUTBOUND_MESSAGE_KEY)) {
      // Request from the Ticl service to send a message.
      handleOutboundMessage(intent.getByteArrayExtra(ProtocolIntents.OUTBOUND_MESSAGE_KEY));
    } else if (intent.hasExtra(AndroidChannelConstants.AuthTokenConstants.EXTRA_AUTH_TOKEN)) {
      // Reply from the app with an auth token and a message to send.
      handleAuthTokenResponse(intent);
    } else if (intent.hasExtra(AndroidChannelConstants.MESSAGE_SENDER_SVC_GCM_REGID_CHANGE)) {
      handleGcmRegIdChange();
    } else {
      logger.warning("Ignoring intent: %s", intent);
    }
  }

  /**
   * Handles a request to send a message to the data center. Validates the message and sends
   * an intent to the application to obtain an auth token to use on the HTTP request to the
   * data center.
   */
  private void handleOutboundMessage(byte[] sendRequestBytes) {
    // Parse and validate the send request.
    final AndroidNetworkSendRequest sendRequest;
    try {
      sendRequest = AndroidNetworkSendRequest.parseFrom(sendRequestBytes);
    } catch (ValidationException exception) {
      logger.warning("Invalid AndroidNetworkSendRequest from %s: %s", sendRequestBytes, exception);
      return;
    }

    // Request an auth token from the application to use when sending the message.
    byte[] message = sendRequest.getMessage().getByteArray();
    requestAuthTokenForMessage(message, null);
  }

  /**
   * Requests an auth token from the application to use to send {@code message} to the data
   * center.
   * <p>
   * If not {@code null}, {@code invalidAuthToken} is an auth token that was previously
   * found to be invalid. The intent sent to the application to request the new token will include
   * the invalid token so that the application can invalidate it in the {@code AccountManager}.
   */
  private void requestAuthTokenForMessage(byte[] message, String invalidAuthToken) {
    /*
     * Send an intent requesting an auth token. This intent will contain a pending intent
     * that the recipient can use to send back the token (by attaching the token as a string
     * extra). That pending intent will also contain the message that we were just asked to send,
     * so that it will be echoed back to us with the token. This avoids our having to persist
     * the message while waiting for the token.
     */

    // This is the intent that the application will send back to us (the pending intent allows
    // it to send the intent). It contains the stored message. We require that it be delivered to
    // this class only, as a security check.
    Intent tokenResponseIntent = new Intent(this, getClass());
    tokenResponseIntent.putExtra(AuthTokenConstants.EXTRA_STORED_MESSAGE, message);

    // If we have an invalid auth token, set a bit in the intent that the application will send
    // back to us. This will let us know that it is a retry; if sending subsequently fails again,
    // we will not do any further retries.
    tokenResponseIntent.putExtra(AuthTokenConstants.EXTRA_IS_RETRY, invalidAuthToken != null);

    // The pending intent allows the application to send us the tokenResponseIntent.
    PendingIntent pendingIntent = PendingIntent.getService(
        this, Arrays.hashCode(message), tokenResponseIntent, PendingIntent.FLAG_ONE_SHOT);

    // We send the pending intent as an extra in a normal intent to the application. The
    // invalidation listener service must handle AUTH_TOKEN_REQUEST intents.
    Intent requestTokenIntent = new Intent(AuthTokenConstants.ACTION_REQUEST_AUTH_TOKEN);
    requestTokenIntent.putExtra(AuthTokenConstants.EXTRA_PENDING_INTENT, pendingIntent);
    if (invalidAuthToken != null) {
      requestTokenIntent.putExtra(AuthTokenConstants.EXTRA_INVALIDATE_AUTH_TOKEN, invalidAuthToken);
    }
    String simpleListenerClass =
        new AndroidTiclManifest(getApplicationContext()).getListenerServiceClass();
    requestTokenIntent.setClassName(getApplicationContext(), simpleListenerClass);
    try {
      startService(requestTokenIntent);
    } catch (SecurityException | IllegalStateException exception) {
      logger.warning("unable to request auth token: %s", exception);
    }
  }

  /**
   * Handles an intent received from the application that contains both a message to send and
   * an auth token and type to use when sending it. This is called when the reply to the intent
   * sent in {@link #requestAuthTokenForMessage(byte[], String)} is received.
   */
  private void handleAuthTokenResponse(Intent intent) {
    if (!(intent.hasExtra(AuthTokenConstants.EXTRA_STORED_MESSAGE)
            && intent.hasExtra(AuthTokenConstants.EXTRA_AUTH_TOKEN)
            && intent.hasExtra(AuthTokenConstants.EXTRA_AUTH_TOKEN_TYPE)
            && intent.hasExtra(AuthTokenConstants.EXTRA_IS_RETRY))) {
      logger.warning(
          "auth-token-response intent missing fields: %s, %s", intent, intent.getExtras());
      return;
    }
    boolean isRetryForInvalidAuthToken =
        intent.getBooleanExtra(AuthTokenConstants.EXTRA_IS_RETRY, false);
    deliverOutboundMessage(
        intent.getByteArrayExtra(AuthTokenConstants.EXTRA_STORED_MESSAGE),
        intent.getStringExtra(AuthTokenConstants.EXTRA_AUTH_TOKEN),
        intent.getStringExtra(AuthTokenConstants.EXTRA_AUTH_TOKEN_TYPE),
        isRetryForInvalidAuthToken);
  }

  /**
   * Sends {@code outgoingMessage} to the data center as a serialized ClientToServerMessage using an
   * HTTP POST.
   * <p>
   * If the HTTP POST fails due to an authentication failure and this is not a retry for an invalid
   * auth token ({@code isRetryForInvalidAuthToken} is {@code false}), then it will call
   * {@link #requestAuthTokenForMessage(byte[], String)} with {@code authToken} to invalidate the
   * token and retry.
   *
   * @param authToken the auth token to use in the HTTP POST
   * @param authTokenType the type of the auth token
   */
  private void deliverOutboundMessage(byte[] outgoingMessage, String authToken,
      String authTokenType, boolean isRetryForInvalidAuthToken) {
    
    NetworkEndpointId networkEndpointId = getNetworkEndpointId(this, logger);
    if (networkEndpointId == null) {
      // No GCM registration; buffer the message to send when we become registered.
      logger.info("Buffering message to the data center: no GCM registration id");
      AndroidChannelPreferences.bufferMessage(outgoingMessage);
      return;
    }
    logger.fine("Delivering outbound message: %s bytes", outgoingMessage.length);
    lastTiclMessageForTest = outgoingMessage;
    URL url = null;
    HttpURLConnection urlConnection = null;
    try {
      // Open the connection.
      boolean isOAuth2Token = authTokenType.startsWith(OAUTH2_TOKEN_TYPE_PREFIX);
      url = buildUrl(isOAuth2Token ? null : authTokenType, networkEndpointId);
      urlConnection = createUrlConnectionForPost(this, url, authToken, isOAuth2Token);

      // We are seeing EOFException errors when reusing connections. Request that the connection is
      // closed on response to work around this issue. Client-to-server messages are batched and
      // infrequent so there isn't much benefit in connection reuse here.
      urlConnection.setRequestProperty("Connection", "close");
      urlConnection.setFixedLengthStreamingMode(outgoingMessage.length);
      urlConnection.connect();

      // Write the outgoing message.
      urlConnection.getOutputStream().write(outgoingMessage);

      // Consume all of the response. We do not do anything with the response (except log it for
      // non-200 response codes), and do not expect any, but certain versions of the Apache HTTP
      // library have a bug that causes connections to leak when the response is not fully consumed;
      // out of sheer paranoia, we do the same thing here.
      String response = readCompleteStream(urlConnection.getInputStream());

      // Retry authorization failures and log other non-200 response codes.
      final int responseCode = urlConnection.getResponseCode();
      switch (responseCode) {
        case HttpURLConnection.HTTP_OK:
        case HttpURLConnection.HTTP_NO_CONTENT:
          break;
        case HttpURLConnection.HTTP_UNAUTHORIZED:
          if (!isRetryForInvalidAuthToken) {
            // If we had an auth failure and this is not a retry of an auth failure, then ask the
            // application to invalidate authToken and give us a new one with which to retry. We
            // check that this attempt was not a retry to avoid infinite loops if authorization
            // always fails.
            requestAuthTokenForMessage(outgoingMessage, authToken);
          }
          break;
        default:
          logger.warning("Unexpected response code %s for HTTP POST to %s; response = %s",
              responseCode, url, response);
      }
    } catch (MalformedURLException exception) {
      logger.warning("Malformed URL: %s", exception);
    } catch (IOException exception) {
      logger.warning("IOException sending message (%s): %s", url, exception);
    } catch (RuntimeException exception) {
      // URL#openConnection occasionally throws a NullPointerException due to an inability to get
      // the class loader for the current thread. There may be other unknown bugs in the network
      // libraries so we just eat runtime exception here.
      logger.warning(
          "RuntimeException creating HTTP connection or sending message (%s): %s", url, exception);
    } finally {
      if (urlConnection != null) {
        urlConnection.disconnect();
      }
    }
  }

  /**
   * Handles a change in the GCM registration id by sending the buffered client message (if any)
   * to the data center.
   */
  private void handleGcmRegIdChange() {
      byte[] bufferedMessage = AndroidChannelPreferences.takeBufferedMessage();
      if (bufferedMessage != null) {
          // Rejoin the start of the code path that handles sending outbound messages.
          requestAuthTokenForMessage(bufferedMessage, null);
      }
  }

  /**
   * Returns a URL to use to send a message to the data center.
   *
   * @param gaiaServiceId Gaia service for which the request will be authenticated (when using a
   *      GoogleLogin token), or {@code null} when using an OAuth2 token.
   * @param networkEndpointId network id of the client
   */
  private static URL buildUrl(String gaiaServiceId, NetworkEndpointId networkEndpointId)
      throws MalformedURLException {
    StringBuilder urlBuilder = new StringBuilder();

    // Build base URL that targets the inbound request service with the encoded network endpoint
    // id.
    urlBuilder.append((channelUrlForTest != null) ? channelUrlForTest : HttpConstants.CHANNEL_URL);
    urlBuilder.append(HttpConstants.REQUEST_URL);

    // TODO: We should be sending a ClientGatewayMessage in the request body
    // instead of appending the client's network endpoint id to the request URL. Once we do that, we
    // should use a UriBuilder to build up a structured Uri object instead of the brittle string
    // concatenation we're doing below.
    urlBuilder.append(base64Encode(networkEndpointId.toByteArray()));

    // Add query parameter indicating the service to authenticate against
    if (gaiaServiceId != null) {
      urlBuilder.append('?');
      urlBuilder.append(HttpConstants.SERVICE_PARAMETER);
      urlBuilder.append('=');
      urlBuilder.append(gaiaServiceId);
    }
    return new URL(urlBuilder.toString());
  }

  /**
   * Returns an {@link HttpURLConnection} to use to POST a message to the data center. Sets
   * the content-type and user-agent headers; also sets the echo token header if we have an
   * echo token.
   *
   * @param context Android context
   * @param url URL to which to post
   * @param authToken auth token to provide in the request header
   * @param isOAuth2Token whether the token is an OAuth2 token (vs. a GoogleLogin token)
   */
  
  public static HttpURLConnection createUrlConnectionForPost(
      Context context, URL url, String authToken, boolean isOAuth2Token) throws IOException {
    HttpURLConnection connection = (HttpURLConnection) url.openConnection();
    try {
      connection.setRequestMethod("POST");
    } catch (ProtocolException exception) {
      throw new RuntimeException("Cannot set request method to POST", exception);
    }
    connection.setDoOutput(true);
    if (isOAuth2Token) {
      connection.setRequestProperty("Authorization", "Bearer " + authToken);
    } else {
      connection.setRequestProperty("Authorization", "GoogleLogin auth=" + authToken);
    }
    connection.setRequestProperty("Content-Type", HttpConstants.PROTO_CONTENT_TYPE);
    connection.setRequestProperty(
        "User-Agent", context.getApplicationInfo().className + "(" + Build.VERSION.RELEASE + ")");

    String echoToken = AndroidChannelPreferences.getEchoToken();
    if (echoToken != null) {
      // If we have a token to echo to the server, echo it.
      connection.setRequestProperty(HttpConstants.ECHO_HEADER, echoToken);
    }
    return connection;
  }

  /** Reads and all data from {@code in}. */
  private static String readCompleteStream(InputStream in) throws IOException {
    StringBuffer buffer = new StringBuffer();
    BufferedReader reader = new BufferedReader(new InputStreamReader(in));
    String line;
    while ((line = reader.readLine()) != null) {
      buffer.append(line);
    }
    return buffer.toString();
  }

  /** Returns a base-64 encoded version of {@code bytes}. */
  private static String base64Encode(byte[] bytes) {
    return Base64.encodeToString(bytes, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING);
  }

  /** Returns the network id for this channel, or {@code null} if one cannot be determined. */
  
  
  public static NetworkEndpointId getNetworkEndpointId(Context context, Logger logger) {
    String registrationId;
    String clientKey;

    // Select the registration token to use.
    if (AndroidChannelPreferences.getGcmChannelType() == GcmChannelType.UPDATED) {
        registrationId = AndroidChannelPreferences.getRegistrationToken();
        clientKey = GcmSharedConstants.ANDROID_ENDPOINT_ID_CLIENT_KEY;
    } else {
        // No client key when using old style registration id.
        clientKey = "";
        try {
            registrationId = GCMRegistrar.getRegistrationId(context);
        } catch (RuntimeException exception) {
            // GCMRegistrar#getRegistrationId occasionally throws a runtime exception. Catching the
            // exception rather than crashing.
            logger.warning("Unable to get GCM registration id: %s", exception);
            registrationId = null;
        }
    }
    if ((registrationId == null) || registrationId.isEmpty()) {
      // No registration with GCM; we cannot compute a network id. The GCM documentation says the
      // string is never null, but we'll be paranoid.
      logger.warning(
          "No GCM registration id; cannot determine our network endpoint id: %s", registrationId);
      return null;
    }
    return CommonProtos.newAndroidEndpointId(registrationId, clientKey,
        context.getPackageName(), AndroidChannelConstants.CHANNEL_VERSION);
  }

  /** Sets the channel url to {@code url}, for tests. */
  public static void setChannelUrlForTest(String url) {
    channelUrlForTest = Preconditions.checkNotNull(url);
  }
}
