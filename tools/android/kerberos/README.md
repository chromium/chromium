## Kerberos Test Utils

Simple app and scripts used to test Kerberos auth on Chrome and WebView.


### Setup

#### 1: Build and install the authenticator app

See the next section for more info about the app.

```sh
ninja -C out/Debug spnego_authenticator_apk
adb install -r out/Debug/apks/SpnegoAuthenticator.apk
```

#### 2: Start the dummy server

```sh
$CHROMIUM_SRC/tools/android/kerberos/negotiate_test_server.py
```

#### 3: Configure Chrome

 -  With command line arguments

    ```sh
    $CHROMIUM_SRC/build/android/adb_chrome_public_command_line \
    '--auth-server-allowlist="*" \
    --auth-spnego-account-type="org.chromium.tools.SpnegoAuthenticator"'
    ```

 -  By setting policies

    The policies to set are:

     *   AuthServerAllowlist: `*`
     *   AuthAndroidNegotiateAccountType: `org.chromium.tools.SpnegoAuthenticator`

    To set them you have to be able to set restrictions for apps on the device.
    This can be achieved using the TestDPC app ([Play store][testdpc-play],
    [Github][testdpc-gh]), which is made for testing enterprise related Android
    features, including app restrictions.

    Set it up, then search for Chrome under "Manage app restrictions", tap
    "Load manifest restrictions" and change the value for the restrictions
    mentioned above.

#### 4: Set up port forwarding via the Chrome inspector

 -  Go to <chrome://inspect>
 -  Click **Port forwarding**
 -  `8080` to `localhost:8080` should be prefilled
 -  Check **Enable port forwarding** and click **Done**

#### 5: Load the protected page

 -  Go to <http://localhost:8080>
 -  The page will display whether or not it managed to talk to the SPNEGO
    authenticator


### SpnegoAuthenticator

This app declares and sets up an accounts to be used for Negotiate auth, as
described in the chromium.org wiki
([Writing a SPNEGO Authenticator for Chrome on Android][crwiki]).
Those accounts use the type `org.chromium.tools.SpnegoAuthenticator`.

![Account administration activity preview][screenshot]

Features:

 -  Set up up to 2 accounts.
 -  Account 1 will start authenticated.
 -  Account 2 will start unauthenticated. The first token request will require
    an additional confirmation step.
 -  Accounts can be added and removed from the Android account settings screen

[testdpc-play]: https://play.google.com/store/apps/details?id=com.sample.android.testdpc
[testdpc-gh]: https://github.com/googlesamples/android-testdpc
[crwiki]:https://sites.google.com/a/chromium.org/dev/developers/design-documents/http-authentication/writing-a-spnego-authenticator-for-chrome-on-android
[screenshot]:SpnegoAuthenticator/preview.png
