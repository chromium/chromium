# File Manager aka Files app

This directory contains the UI (HTML/JS/TS/CSS) for ChromeOS File Manager.

In this directory we find 2 apps:

* Files app in [./file_manager/](./file_manager/): Main code for Files app.
* Image Loader in [./image_loader/](./image_loader/): Auxiliary Chrome extension that tries to extract an image representation of a given file (image, raw image, video, pdf, etc).

Other relevant directories:

* [//ash/webui/file_manager/](/ash/webui/file_manager/): C++ and HTML/JS for the SWA (System Web App) wrapper.
* [//chrome/browser/ash/file_manager/](/chrome/browser/ash/file_manager/): Most of our C++ code running on the browser process.
* [//chrome/browser/ash/extensions/file_manager/](/chrome/browser/ash/extensions/file_manager/): `fileManagerPrivate` Our private API and some other code running on the browser process.
* [//chrome/common/extensions/api/file_manager_private.idl](/chrome/common/extensions/api/file_manager_private.idl) and [file_manager_private_internal.idl](/chrome/common/extensions/api/file_manager_private_internal.idl): The Private API definition.

## Other READMEs

See the following READMEs for more details on the sub-directories.

* [./file_manager/lib/README.md](/ui/file_manager/file_manager/lib/README.md)
* [./file_manager/widgets/README.md](/ui/file_manager/file_manager/widgets/README.md)
* [./file_manager/state/README.md](/ui/file_manager/file_manager/state/README.md)
* [./file_manager/containers/README.md](/ui/file_manager/file_manager/containers/README.md)
* [./image_loader/piex/README.md](/ui/file_manager/image_loader/piex/README.md)

## More info

[Internal only] Files app team maintains some more information in http://go/xf-site.
