# Web Bluetooth New Permissions Backend

This virtual test suite runs content_shell with
`--enable-features=WebBluetoothNewPermissionsBackend`. This flag enables the
Web Bluetooth tests to use the [`FakeBluetoothDelegate`] interface for
granting and checking permissions. This class emulates the behavior of the
new Web Bluetooth permissions backend based on [`ObjectPermissionContextBase`].

The new permissions backend is implemented as part of the [Web Bluetooth
Persistent Permissions] project.

TODO(https://crbug.com/589228): Remove this virtual test suite when the
`WebBluetoothNewPermissionsBackend` flag is enabled by default.

[`FakeBluetoothDelegate`]:
../../../../../content/shell/browser/web_test/fake_bluetooth_delegate.h
[`ObjectPermissionContextBase`]:
../../../../../components/permissions/object_permission_context_base.h
[Web Bluetooth Persistent Permissions]:
https://docs.google.com/document/d/1h3uAVXJARHrNWaNACUPiQhLt7XI-fFFQoARSs1WgMDM
