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

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;

import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelConstants.C2dmConstants;

import org.chromium.base.ContextUtils;

/** Accessor class for shared preference entries used by the channel. */
public class AndroidChannelPreferences {
  /** GCM Channel Configurations */
  public class GcmChannelType {
    /**
     * Uses {@link GcmRegistrar} for registration, {@link AndroidMessageReceiverService} for
     * receiving downstream messages and {@link AndroidMessageSenderService} for upstream messages.
     */
    public static final int DEFAULT = 0;

    /**
     * Uses InstanceID for registration, AndroidGcmController#onMessageReceived for receiving
     * downstream messages and AndroidMessageSenderService for upstream messages.
     */
    public static final int UPDATED = 1;

    /**
     * Uses InstanceID for registration and AndroidGcmController#onMessageReceived for receiving
     * downstream messages and GcmUpstreamSenderService for upstream messages.
     */
    public static final int GCM_UPSTREAM = 2;
  }

  /** Name of the preferences in which channel preferences are stored. */
  private static final String PREFERENCES_NAME = "com.google.ipc.invalidation.gcmchannel";

  /**
   * Preferences entry used to buffer the last message sent by the Ticl in the case where a GCM
   * registration id is not currently available.
   */
  private static final String BUFFERED_MSG_PREF = "buffered-msg";

  /**
   * Preferences entry used to store the sender id for registering with GCM.
   */
  private static final String GCM_CHANNEL_TYPE_PREF = "gcm_channel_type";

  /**
   * Preferences entry used to store the GCM registration token used with
   * {@code GcmChannelType#UPDATED} or {@code GcmChannelType#GCM_UPSTREAM}
   */
  private static final String GCM_REGISTRATION_TOKEN_PREF = "gcm_registration_token";

  /**
   * Preferences entry used to store the client app version for the {@code GCM_REGISTRATION_TOKEN}
   * since the current token is not guarenteed to work with an updated version.
   */
  private static final String GCM_APP_VERSION_PREF = "gcm_app_version";

  private static final Logger logger = AndroidLogger.forTag("ChannelPrefs");

  /** Sets the token echoed on subsequent HTTP requests. */
  static void setEchoToken(String token) {
      SharedPreferences.Editor editor = getPreferences().edit();

      // This might fail, but at worst it just means we lose an echo token; the channel
      // needs to be able to handle that anyway since it can never assume an echo token
      // makes it to the client (since the channel can drop messages).
      editor.putString(C2dmConstants.ECHO_PARAM, token);
      if (!editor.commit()) {
          logger.warning("Failed writing shared preferences for: setEchoToken");
      }
  }

  /** Returns the echo token that should be included on HTTP requests. */

  public static String getEchoToken() {
      return getPreferences().getString(C2dmConstants.ECHO_PARAM, null);
  }

  /** Buffers the last message sent by the Ticl. Overwrites any previously buffered message. */
  static void bufferMessage(byte[] message) {
      SharedPreferences.Editor editor = getPreferences().edit();
      String encodedMessage =
              Base64.encodeToString(message, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING);
      editor.putString(BUFFERED_MSG_PREF, encodedMessage);

      // This might fail, but at worst we'll just drop a message, which the Ticl must be prepared to
      // handle.
      if (!editor.commit()) {
          logger.warning("Failed writing shared preferences for: bufferMessage");
      }
  }

  /**
   * Removes and returns the buffered Ticl message, if any. If no message was buffered, returns
   * {@code null}.
   */
  static byte[] takeBufferedMessage() {
      SharedPreferences preferences = getPreferences();
      String message = preferences.getString(BUFFERED_MSG_PREF, null);
      if (message == null) {
          // No message was buffered.
          return null;
      }
      // There is a message to return. Remove the stored value from the preferences.
      SharedPreferences.Editor editor = preferences.edit();
      editor.remove(BUFFERED_MSG_PREF);

      // If this fails, we might send the same message twice, which is fine.
      if (!editor.commit()) {
          logger.warning("Failed writing shared preferences for: takeBufferedMessage");
      }

      // Return the decoded message.
      return Base64.decode(message, Base64.URL_SAFE);
  }

  /**
   * Sets the registration token returned from GCM for the sender id stored against
   * {@code GCM_SENDER_ID}.
   */
  static void setRegistrationToken(String token) {
      if (token == null) {
          return;
      }
      SharedPreferences.Editor editor = getPreferences().edit();
      editor.putString(GCM_REGISTRATION_TOKEN_PREF, token);
      if (!editor.commit()) {
          logger.warning("Failed writing shared preferences for: setRegistrationToken");
      }
  }

  /**
   * Returns the registration token stored or an empty string if no token is found.
   */
  static String getRegistrationToken() {
      return getPreferences().getString(GCM_REGISTRATION_TOKEN_PREF, "");
  }

  /**
   * Sets the GCM channel configuration used.
   * @param type, the channel configuration type specified in {@code GcmChannelType}.
   */
  public static void setGcmChannelType(int type) {
      if (getGcmChannelType() == type) {
          return;
      }
      SharedPreferences.Editor editor = getPreferences().edit();
      editor.putInt(GCM_CHANNEL_TYPE_PREF, type);
      if (!editor.commit()) {
          logger.warning("Failed writing shared preferences for: setGcmChannelType");
      }
  }

  /**
   * Returns the GCM channel configuration used.
   */
  static int getGcmChannelType() {
      return getPreferences().getInt(GCM_CHANNEL_TYPE_PREF, -1);
  }

  /**
   * Stores the client app version for the registration token stored against {@code GCM_APP_VERSION}
   */
  static void setAppVersion(int version) {
      SharedPreferences.Editor editor = getPreferences().edit();

      editor.putInt(GCM_APP_VERSION_PREF, version);
      if (!editor.commit()) {
          logger.warning("Failed writing shared preferences for: setAppVersion");
      }
  }

  /**
   * Returns the client app version or -1 if no version is found.
   */
  static int getAppVersion() {
      return getPreferences().getInt(GCM_APP_VERSION_PREF, -1);
  }

  /** Returns whether a message has been buffered, for tests. */
  public static boolean hasBufferedMessageForTest() {
      return getPreferences().contains(BUFFERED_MSG_PREF);
  }

  /** Returns a new {@link SharedPreferences} instance to access the channel preferences. */
  private static SharedPreferences getPreferences() {
      return ContextUtils.getApplicationContext().getSharedPreferences(
              PREFERENCES_NAME, Context.MODE_PRIVATE);
  }
}
