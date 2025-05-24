TODO(crbug.com/415223384):
`document.requestStorageAccess` is racy when permission has been overridden (e.g. via `test_driver.set_permission`).
This is because the RFHI in the browser process may not be aware that the renderer has requested (and gotten) permission by the time StorageAccessHandle tries to bind mojo endpoints.
This virtual test suite ensures no WPTs go stale while we wait on the less temporary fix in the task linked above.