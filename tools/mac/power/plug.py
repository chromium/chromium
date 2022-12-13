#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import psutil
import time
import os
import argparse
import logging
from datetime import datetime

from kasa import SmartStrip

class KasaPlugController():
  """Provides control of a device's charger.

  The device's charger must be plugged into one of the 3 outlets of a Kasa Smart
  Plug Power Strip (KP303). The outlet name must match the device's host name
  (this is intended to prevent inadvertently controlling the wrong device's
  charger).
  """

  def __init__(self, kasa_power_strip_ip: str):
    """Constructs a KasaPlugController to control the current device's charger.

    Args:
        kasa_power_strip_ip: IP of the Kasak Smart Plug Power Strip in which
            this device's charger is connected.
    """
    # The outlet name must match the device's host name.
    self.kasa_outlet_name = os.uname()[1].split('.')[0]

    # Create the event loop
    self.loop = asyncio.new_event_loop()
    asyncio.set_event_loop(self.loop)

    # Create the strip controller
    self.strip = SmartStrip(kasa_power_strip_ip)
    self.loop.run_until_complete(self.strip.update())

    self.closed = False

  def __del__(self):
    self.close()

  def turn_on(self):
    """Turns on this device's charger.
    """
    logging.info("Turning on the charger")
    for plug in self.strip.children:
      if plug.alias == self.kasa_outlet_name:
        self.loop.run_until_complete(plug.turn_on())
        return
    logging.error("Cannot find device to turn on")

  def turn_off(self):
    """Turns off this device's charger.
    """
    logging.info("Turning off the charger")
    for plug in self.strip.children:
      if plug.alias == self.kasa_outlet_name:
        self.loop.run_until_complete(plug.turn_off())

        battery = psutil.sensors_battery()
        while battery.power_plugged:
          logging.info("Waiting for device to no longer be plugged in")
          time.sleep(1)
          battery = psutil.sensors_battery()
        return
    logging.error("Cannot find device to turn off")

  def discharge_to(self, level: int):
    """Discharges the battery until it reaches a target level.

    Args:
        level: The target battery level.
    """

    self.turn_off()

    battery = psutil.sensors_battery()

    while battery.percent > level:
      logging.info(f"Waiting to discharge to {level}%."
                   f" Currently at {battery.percent}%")

      # Perform arbitrary operations as fast as possible to burn
      # CPU and discharge faster.
      f_value = 0.81
      start = datetime.now()
      while ((datetime.now() - start).total_seconds() < 10):
        f_value = f_value * 1.7272882
        f_value = f_value / 1.7272882

      battery = psutil.sensors_battery()

    logging.info(f"Discharge to {level}% complete")

  def charge_to(self, level: int):
    """Charges the battery until it reaches a target level.

    Args:
        level: The target battery level.
    """

    self.turn_on()

    battery = psutil.sensors_battery()

    while battery.percent < level:
      logging.info(f"Waiting to charge to {level}%."
                   f" Currently at {battery.percent}%")
      time.sleep(10)
      battery = psutil.sensors_battery()

    logging.info(f"Charge to {level}% complete")

  def charge_or_discharge_to(self, level: int):
    """Charges or discharges the battery until it reaches a target level.

    Leaves the charger in an unplugged state.

    Args:
        level: The target battery level.
    """
    battery = psutil.sensors_battery()
    if battery.percent < level:
      self.charge_to(level)
    elif battery.percent > level:
      self.discharge_to(level)
    else:
      logging.info(f"Battery is already at the target level {level}%")
    self.turn_off()

  def close(self):
    """Closes the message loop."""
    if self.closed:
      return
    self.closed = True
    self.loop.close()


def get_plug_controller(ip: str):
  return KasaPlugController(ip)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description='Controls kasa power switch connected to this device.')
  parser.add_argument("--kasa_power_strip_ip",
                      required=True,
                      help="IP address of the kasa power switch.")
  parser.add_argument("--charge_level",
                      type=int,
                      required=True,
                      help="Desired charge level.")
  args = parser.parse_args()

  kasa_plug_controller = KasaPlugController(args.kasa_power_strip_ip)
  kasa_plug_controller.charge_to(args.charge_level)
  kasa_plug_controller.close()
