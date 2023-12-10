// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.spnegoauthenticator;

import android.accounts.AccountAuthenticatorActivity;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

import org.chromium.base.Log;

/** Provides a UI to administrate the Spnego accounts. */
public class SpnegoAuthenticatorActivity extends AccountAuthenticatorActivity {
    private static final String TAG = Constants.TAG;

    // Constants for passing information via intents.
    private static final String KEY_MODE = "mode";
    private static final String KEY_ACCOUNT = "account";
    private static final int MODE_INVALID = 0;
    private static final int MODE_ADD_ACCOUNT = 1;
    private static final int MODE_CONFIRM_CREDENTIALS = 2;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_account_authenticator);

        Intent intent = getIntent();
        initUi(intent.getIntExtra(KEY_MODE, MODE_INVALID), intent.getStringExtra(KEY_ACCOUNT));
    }

    /** Returns an intent that can be used to start the activity in AddAcount mode */
    public static Intent getAddAccountIntent(
            Context context, AccountAuthenticatorResponse response) {
        Intent intent = new Intent(context, SpnegoAuthenticatorActivity.class);
        intent.putExtra(KEY_MODE, MODE_ADD_ACCOUNT);
        intent.putExtra(AccountManager.KEY_ACCOUNT_AUTHENTICATOR_RESPONSE, response);
        return intent;
    }

    /** Returns an intent that can be used to start the activity in ConfirmCredentials mode */
    public static Intent getConfirmCredentialsIntent(
            Context context, String accountName, AccountAuthenticatorResponse response) {
        Intent intent = new Intent(context, SpnegoAuthenticatorActivity.class);
        intent.putExtra(KEY_MODE, MODE_CONFIRM_CREDENTIALS);
        intent.putExtra(KEY_ACCOUNT, accountName);
        intent.putExtra(AccountManager.KEY_ACCOUNT_AUTHENTICATOR_RESPONSE, response);
        intent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS
                        | Intent.FLAG_ACTIVITY_NO_HISTORY);
        return intent;
    }

    private void addAccount(String accountName) {
        Log.d(TAG, "Adding account '%s'", accountName);

        AccountData accountData = AccountData.create(accountName, this);
        accountData.save(this);
        Intent intent = accountData.getAccountAddedIntent();
        intent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS
                        | Intent.FLAG_ACTIVITY_NO_HISTORY);
        setAccountAuthenticatorResult(intent.getExtras());
        setResult(RESULT_OK, intent);
        finish();
    }

    private void confirmCredentials(String accountName) {
        Log.d(TAG, "Confirming credentials for account '%s'", accountName);

        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        nm.cancel(Constants.CONFIRM_CREDENTIAL_NOTIFICATION_ID);

        AccountData accountData = AccountData.get(accountName, this);
        accountData.setIsAuthenticated(true);
        accountData.save(this);

        Intent intent = accountData.getCredentialsConfirmedIntent();
        setAccountAuthenticatorResult(intent.getExtras());
        setResult(RESULT_OK, intent);
        finish();
    }

    private void initUi(final int mode, final String account) {
        Button signInButton1 = (Button) findViewById(R.id.sign_in_button_1);
        Button signInButton2 = (Button) findViewById(R.id.sign_in_button_2);
        Button confirmCredentialsButton = (Button) findViewById(R.id.confirm_credentials_button);

        switch (mode) {
            case MODE_ADD_ACCOUNT:
                signInButton1.setOnClickListener(
                        new OnClickListener() {
                            @Override
                            public void onClick(View view) {
                                addAccount(Constants.ACCOUNT_1_NAME);
                            }
                        });
                signInButton2.setOnClickListener(
                        new OnClickListener() {
                            @Override
                            public void onClick(View view) {
                                addAccount(Constants.ACCOUNT_2_NAME);
                            }
                        });
                confirmCredentialsButton.setEnabled(false);
                break;

            case MODE_CONFIRM_CREDENTIALS:
                signInButton1.setEnabled(false);
                signInButton2.setEnabled(false);
                confirmCredentialsButton.setOnClickListener(
                        new OnClickListener() {
                            @Override
                            public void onClick(View view) {
                                confirmCredentials(account);
                            }
                        });
                break;

            default:
                Log.w(TAG, "Opened the activity in an invalid mode: %d", mode);
                signInButton1.setEnabled(false);
                signInButton2.setEnabled(false);
                confirmCredentialsButton.setEnabled(false);
                break;
        }
    }
}
