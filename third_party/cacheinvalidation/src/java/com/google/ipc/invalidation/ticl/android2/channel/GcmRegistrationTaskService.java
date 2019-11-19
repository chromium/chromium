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
import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.GoogleCloudMessaging;
import com.google.android.gms.gcm.TaskParams;
import com.google.android.gms.iid.InstanceID;
import com.google.ipc.invalidation.common.GcmSharedConstants;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.android2.AndroidTiclManifest;
import com.google.ipc.invalidation.ticl.android2.ProtocolIntents;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelPreferences.GcmChannelType;

import android.content.Context;
import android.content.Intent;

import java.io.IOException;

/**
 * Implementation of {@link GcmTaskService} to fetch the registration token from GCM. The service is
 * started by the {@link GcmNetworkManager} when a Task scheduled using the GcmNetworkManager is
 * ready to be executed. The task to start this service is scheduled in
 * {@link AndroidGcmController#fetchToken}.
 *
 * <p>The service fetches a token from GCM, stores it and sends an update to the server. In case of
 * failure to fetch the token, the task is rescheduled using the GcmNetworkManager which uses
 * exponential back-offs to control when the task is executed.
 */
public class GcmRegistrationTaskService extends GcmTaskService {
  private static final Logger logger = AndroidLogger.forTag("RegistrationTaskService");

  public InstanceID getInstanceID(Context context) {
    return InstanceID.getInstance(context);
  }

  /**
   * Called when the task is ready to be executed. Registers with GCM using
   * {@link InstanceID#getToken} and stores the registration token.
   *
   * <p>Returns {@link GcmNetworkManager#RESULT_SUCCESS} when the token is successfully retrieved.
   * On failure {@link GcmNetworkManager#RESULT_RESCHEDULE} is used which reschedules the service
   * to be executed again using exponential back-off.
   */
  @Override
  public int onRunTask(TaskParams params) {
    if (!AndroidChannelConstants.GCM_REGISTRATION_TASK_SERVICE_TAG.equals(params.getTag())) {
     logger.warning("Unknown task received with tag: %s", params.getTag());
     return GcmNetworkManager.RESULT_FAILURE;
    }

    String senderId = GcmSharedConstants.GCM_UPDATED_SENDER_ID;
    try {
      String token = getInstanceID(this).getToken(
          senderId, GoogleCloudMessaging.INSTANCE_ID_SCOPE, null);
      storeToken(token);
      return GcmNetworkManager.RESULT_SUCCESS;
    } catch (IOException exception) {
      logger.warning("Failed to get token for sender: %s. Exception : %s", senderId, exception);
      return GcmNetworkManager.RESULT_RESCHEDULE;
    } catch (SecurityException exception) {
      // InstanceID#getToken occasionally throws a security exception when trying send the
      // registration intent to GMSCore. Catching the exception here to prevent crashes.
      logger.warning("Security exception when fetching token: %s", exception);
      return GcmNetworkManager.RESULT_RESCHEDULE;
    }
  }

  /** Stores the registration token and the current application version in Shared Preferences. */
  private void storeToken(String token) {
      AndroidChannelPreferences.setRegistrationToken(token);
      AndroidChannelPreferences.setAppVersion(
              CommonUtils.getPackageVersion(this, getPackageName()));
      // Send the updated token to the server.
      updateServer();
  }

  /** Sends a message to the server to update the GCM registration token. */
  private void updateServer() {
    // Inform the sender service that the registration token has changed. If the sender service
    // had buffered a message because no registration token was previously available, this intent
    // will cause it to send that message.
    Intent sendBuffered = new Intent();
    final String ignoredData = "";
    sendBuffered.putExtra(AndroidChannelConstants.MESSAGE_SENDER_SVC_GCM_REGID_CHANGE, ignoredData);

    // Select the sender service to use for upstream message.
    if (AndroidChannelPreferences.getGcmChannelType() == GcmChannelType.GCM_UPSTREAM) {
        String upstreamServiceClass = new AndroidTiclManifest(this).getGcmUpstreamServiceClass();
        if (upstreamServiceClass == null) {
            logger.warning("GcmUpstreamSenderService class not found.");
            return;
        }
        sendBuffered.setClassName(this, upstreamServiceClass);
    } else {
        sendBuffered.setClass(this, AndroidMessageSenderService.class);
    }
    try {
      startService(sendBuffered);
    } catch (IllegalStateException exception) {
      logger.warning("Unable to send buffered message(s): %s", exception);
    }

    // Inform the Ticl service that the registration id has changed. This will cause it to send
    // a message to the data center and update the GCM registration id stored at the data center.
    Intent updateServer = ProtocolIntents.InternalDowncalls.newNetworkAddrChangeIntent();
    updateServer.setClassName(this, new AndroidTiclManifest(this).getTiclServiceClass());
    try {
      startService(updateServer);
    } catch (IllegalStateException exception) {
      logger.warning("Unable to inform server about new registration id: %s", exception);
    }
  }
}
