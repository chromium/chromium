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

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.OneoffTask;
import com.google.ipc.invalidation.common.GcmSharedConstants;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.android2.AndroidTiclManifest;
import com.google.ipc.invalidation.ticl.android2.ProtocolIntents;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelConstants.C2dmConstants;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelPreferences.GcmChannelType;
import com.google.ipc.invalidation.ticl.proto.AndroidChannel.AddressedAndroidMessage;
import com.google.ipc.invalidation.util.ProtoWrapper.ValidationException;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Base64;


/**
 * A controller class used by the client to initialize the GCM channel and forward downstream
 * messages received from GCM.
 */
public class AndroidGcmController {
  private static final Logger logger = AndroidLogger.forTag("AndroidGcmController");

  /** Package name for Google Play Services APK. */
  private static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";

  /** Google Play Services APK version for Queso. */
  private static final int QUESO_PLAY_SERVICES_VERSION = 7571000;

  private static final Object lock = new Object();

  private static AndroidGcmController androidGcmController;

  private GcmNetworkManager gcmNetworkManager;

  private Context context;

  /**
   * A getter method for AndroidGcmController singleton which also initializes it if it wasn't
   * already initialized.
   *
   * @param context the application context.
   * @return a singleton instance of the AndroidGcmController
   */
  public static AndroidGcmController get(Context context) {
    synchronized (lock) {
      if (androidGcmController == null) {
        androidGcmController =
            new AndroidGcmController(context, GcmNetworkManager.getInstance(context));
      }
    }
    return androidGcmController;
  }

  /**
   * Override AndroidGcmController with a custom GcmNetworkManager in tests. This overrides the
   * existing instance of AndroidGcmController if any.
   *
   * @param context the application context.
   * @param gcmNetworkManager the custom GcmNetworkManager to use.
   */
  public static void overrideAndroidGcmControllerForTests(
      Context context, GcmNetworkManager gcmNetworkManager) {
    synchronized (lock) {
      androidGcmController = new AndroidGcmController(context, gcmNetworkManager);
    }
  }

  private AndroidGcmController(Context context, GcmNetworkManager gcmNetworkManager) {
    this.context = context;
    this.gcmNetworkManager = gcmNetworkManager;
  }

  /**
   * Returns true if no registration token is stored or the current application version is higher
   * than the version for the token stored.
   */
  private boolean shouldFetchToken() {
    String pkgName = context.getPackageName();
    return AndroidChannelPreferences.getRegistrationToken().isEmpty()
            || AndroidChannelPreferences.getAppVersion()
            < CommonUtils.getPackageVersion(context, pkgName);
  }

  /**
   * Analogous to the MultiplexingGcmListener#initializeGcm call but used to support the updated
   * GCM channel. Sets the {@link AndroidChannelPreferences.GcmChannelType} and fetches the
   * registration token from GCM if no token is stored or if the application has been updated.
   *
   * @param useGcmUpstream if true, the upstream messages from the client to the data center are
   * sent using GCM.
   */
  public void initializeGcm(boolean useGcmUpstream) {
    if (useGcmUpstream) {
      logger.info("Initializing Gcm. Use Gcm Upstream Sender Service");
      AndroidChannelPreferences.setGcmChannelType(GcmChannelType.GCM_UPSTREAM);
    } else {
      logger.info("Initializing Gcm updated.");
      AndroidChannelPreferences.setGcmChannelType(GcmChannelType.UPDATED);
    }
    if (shouldFetchToken()) {
      fetchToken();
    }
  }

  /**
   * Clears the current registration token and schedules a {@link OneoffTask} to start the
   * GcmRegistrationTaskService if Google Play Services is available.
   *
   * <p>Declared public to be used by the client to update the token if they define an
   * implementation of InstanceIDListenerService.
   */
  public void fetchToken() {
    // Clear the current token. If the call to InstanceID#getToken fails a new token will be fetched
    // on the next call to {@code initializeGcm}.
    AndroidChannelPreferences.setRegistrationToken("");

    // The GMS client library requires the corresponding version of Google Play Services APK to be
    // installed on the device.
    if (CommonUtils.getPackageVersion(context, GOOGLE_PLAY_SERVICES_PACKAGE)
        < QUESO_PLAY_SERVICES_VERSION) {
      logger.warning("Google Play Services unavailable. Initialization failed.");
      return;
    }

    OneoffTask registrationTask =
        new OneoffTask.Builder()
            .setExecutionWindow(0, 1)
            .setTag(AndroidChannelConstants.GCM_REGISTRATION_TASK_SERVICE_TAG)
            .setService(GcmRegistrationTaskService.class)
            .build();

    try {
      gcmNetworkManager.schedule(registrationTask);
    } catch (IllegalArgumentException exception) {
      // Scheduling the service can throw an exception due to a framework error on Android when
      // the the look up for the GCMTaskService being scheduled to be run fails.
      // See crbug/548314.
      logger.warning("Failed to schedule GCM registration task. Exception: %s", exception);
    }
  }

  /**
   * Used by the client to get the sender id to filter the GCM downstream messages forwarded to
   * {@code onMessageReceived}.
   */
  public String getSenderId() {
    return GcmSharedConstants.GCM_UPDATED_SENDER_ID;
  }

  /**
   * Used by the client to forward downstream messages received from GCM.
   *
   * @param data the data bundle of the downstream message.
   */
  public void onMessageReceived(Bundle data) {
    String content = data.getString(C2dmConstants.CONTENT_PARAM);
    if (content != null) {
      byte[] msgBytes = Base64.decode(content, Base64.URL_SAFE);
      try {
        // Look up the name of the Ticl service class from the manifest.
        String serviceClass = new AndroidTiclManifest(context).getTiclServiceClass();
        AddressedAndroidMessage addrMessage = AddressedAndroidMessage.parseFrom(msgBytes);
        Intent msgIntent =
            ProtocolIntents.InternalDowncalls.newServerMessageIntent(addrMessage.getMessage());
        msgIntent.setClassName(context, serviceClass);
        context.startService(msgIntent);
      } catch (ValidationException exception) {
        logger.warning("Failed parsing inbound message: %s", exception);
      } catch (IllegalStateException exception) {
        logger.warning("Unable to handle inbound message: %s", exception);
      }
    } else {
      logger.warning("GCM Intent has no message content: %s", data);
    }

    // Store the echo token.
    
    String echoToken = data.getString(C2dmConstants.ECHO_PARAM);
    if (echoToken != null) {
        AndroidChannelPreferences.setEchoToken(echoToken);
    }
  }
}
