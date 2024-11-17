// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.spnegoauthenticator;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.net.HttpNegotiateConstants;

import java.util.Arrays;

/** AccountAuthenticator implementation */
public class SpnegoAuthenticator extends AbstractAccountAuthenticator {

    private static final String TAG = Constants.TAG;
    private final Context mContext;

    public SpnegoAuthenticator(Context context) {
        super(context);
        mContext = context;
    }

    @Override
    public Bundle addAccount(
            AccountAuthenticatorResponse response,
            String accountType,
            String authTokenType,
            String[] requiredFeatures,
            Bundle options)
            throws NetworkErrorException {
        Log.d(TAG, "addAccount()");

        // Delegate to the activity to get the account information from the user.
        Bundle bundle = new Bundle();
        bundle.putParcelable(
                AccountManager.KEY_INTENT,
                SpnegoAuthenticatorActivity.getAddAccountIntent(mContext, response));
        return bundle;
    }

    @Override
    public Bundle confirmCredentials(
            AccountAuthenticatorResponse response, Account account, Bundle options)
            throws NetworkErrorException {
        Log.d(TAG, "confirmCredentials(%s)", account.name);
        return unsupportedOperationBundle("confirmCredentials");
    }

    @Override
    public Bundle editProperties(AccountAuthenticatorResponse response, String accountType) {
        Log.d(TAG, "editProperties(%s)", accountType);
        return unsupportedOperationBundle("editProperties");
    }

    @Override
    public Bundle getAuthToken(
            AccountAuthenticatorResponse response,
            Account account,
            String authTokenType,
            Bundle options)
            throws NetworkErrorException {
        Log.d(TAG, "getAuthToken(%s)", account.name);

        Bundle result = new Bundle();
        if (AccountData.get(account.name, mContext).isAuthenticated()) {
            Log.d(TAG, "getAuthToken(): Returning dummy SPNEGO auth token");
            result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
            result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
            result.putString(AccountManager.KEY_AUTHTOKEN, Constants.AUTH_TOKEN);
            result.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT, 0);
        } else {
            Log.d(TAG, "getAuthToken(): Asking for credentials confirmation");
            Intent intent =
                    SpnegoAuthenticatorActivity.getConfirmCredentialsIntent(
                            mContext, account.name, response);
            result.putParcelable(AccountManager.KEY_INTENT, intent);

            // We need to show a notification in case the caller can't use the intent directly.
            showConfirmCredentialsNotification(mContext, intent);
        }

        return result;
    }

    @Override
    public String getAuthTokenLabel(String authTokenType) {
        Log.d(TAG, "getAuthTokenLabel(%s)", authTokenType);
        return "Spnego " + authTokenType;
    }

    @Override
    public Bundle hasFeatures(
            AccountAuthenticatorResponse response, Account account, String[] features)
            throws NetworkErrorException {
        Log.d(TAG, "hasFeatures(%s)", Arrays.asList(features));
        Bundle result = new Bundle();

        // All our accounts only have the SPNEGO feature, other features are not supported.
        for (String feature : features) {
            if (!feature.equals(HttpNegotiateConstants.SPNEGO_FEATURE)) {
                result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, false);
                return result;
            }
        }
        result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
        return result;
    }

    @Override
    public Bundle updateCredentials(
            AccountAuthenticatorResponse response,
            Account account,
            String authTokenType,
            Bundle options)
            throws NetworkErrorException {
        Log.d(TAG, "updateCredentials(%s)", account.name);
        return unsupportedOperationBundle("updateCredentials");
    }

    private void showConfirmCredentialsNotification(Context context, Intent intent) {
        PendingIntent notificationAction =
                PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification =
                new Notification.Builder(context)
                        .setContentTitle("Authentication required")
                        .setContentText("Credential confirmation required for the Spnego account")
                        .setSmallIcon(android.R.drawable.stat_sys_warning)
                        .setContentIntent(notificationAction)
                        .setAutoCancel(true)
                        .build();

        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);

        notificationManager.notify(Constants.CONFIRM_CREDENTIAL_NOTIFICATION_ID, notification);
    }

    /** Returns a bundle containing a standard error response. */
    private Bundle unsupportedOperationBundle(String operationName) {
        Bundle result = new Bundle();
        result.putInt(
                AccountManager.KEY_ERROR_CODE, AccountManager.ERROR_CODE_UNSUPPORTED_OPERATION);
        result.putString(AccountManager.KEY_ERROR_MESSAGE, "Unsupported method: " + operationName);
        return result;
    }
}
