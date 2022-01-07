"""Utility for running single test on multiple emulator targets."""

def android_multidevice_instrumentation_test(name, target_devices, **kwargs):
    """Generates a android_instrumentation_test rule for each given device.

    Args:
      name: Name prefix to use for the rules. The name of the generated rules will follow:
        name + target_device[-6:] eg name-15_x86
      target_devices: array of device targets
      **kwargs: arguments to pass to generated android_test rules
    """
    for device in target_devices:
        native.android_instrumentation_test(
            name = name + "-" + device[-6:],
            target_device = device,
            **kwargs
        )
