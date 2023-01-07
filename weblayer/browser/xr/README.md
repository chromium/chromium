# WebXR

WebLayer supports WebAR (with no current plans to support VR).

## Testing

Using [a device that supports AR](https://developers.google.com/ar/discover/supported-devices), build and run with

```
   $ autoninja -C out/Default run_weblayer_shell
   $ ./out/Default/run_weblayer_shell https://immersive-web.github.io/webxr-samples/immersive-ar-session.html
```

`run_weblayer_shell_webview` and `run_weblayer_shell_trichrome` should also work.

If [Google Play Services for AR](https://play.google.com/store/apps/details?id=com.google.ar.core) is installed, then
the demo should work. If it is not installed or not up to date, then the demo will silently fail (the button will shake).

## TODO

WebLayer should support an [install flow](https://crbug.com/1177948) for handling the case when Play Services for AR is
not up to date.
