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

# This script allows the user to control a Kasa Smart Plug Power Strip (KP303)
# to start and stop the charging of devices connected to it.
# The smart switch has three plugs and it needs to be configured to have plug
# names that are the identical to the host names of the machines that are
# connected to it.  For example for $HOST=macbook-air use the Kasa app to name
# a plug macbook-air and plug a charger in said plug and the laptop. Hardcoding
# things in this way minimizes the chances of turning on/off a unrelated
# machine which invalidates the whole benchmark/profile run.


class KasaPlugController():
  """Class that manages communication with the Kasa smart plug
  """

  def __init__(self, kasa_switch_ip: str):
    # Create the event loop
    self.loop = asyncio.new_event_loop()
    asyncio.set_event_loop(self.loop)

    # Create the strip controller
    self.strip = SmartStrip(kasa_switch_ip)
    self.loop.run_until_complete(self.strip.update())

    self.closed = False

  def __del__(self):
    self.close()

  def turn_on(self, device: str):
    """Turns the plug for |device| to the on position
    """

    for plug in self.strip.children:
      if plug.alias == device:
        self.loop.run_until_complete(plug.turn_on())
        return
    print("Cannot find device!")

  def turn_off(self, device: str):
    """Turns the plug for |device| to the off position
    """
    for plug in self.strip.children:
      if plug.alias == device:
        self.loop.run_until_complete(plug.turn_off())
        return
    print("Cannot find device!")

  def discharge_to(self, level: int, battery):
    while battery.percent > level:
      print(f"Waiting to discharge to {level}%."
            f" Currently at {battery.percent}%")
      battery = psutil.sensors_battery()

      # Perform arbitrary operations as fast as possible to burn
      # CPU and discharge faster.
      f_value = 0.81
      start = datetime.now()
      while ((datetime.now() - start).total_seconds() < 10):
        f_value = f_value * 1.7272882
        f_value = f_value / 1.7272882

    print("Discharge complete")

  def charge_to(self, level: int):
    """Ensures the current device reaches the charge level |level|.
    """
    battery = psutil.sensors_battery()
    # Get the host name of the device
    device = os.uname()[1].split('.')[0]

    if battery.percent > level:
      self.turn_off(device)
      self.discharge_to(level, battery)
      return

    print(f"Plugging in {device}...")
    self.turn_on(device)

    while not battery.power_plugged:
      battery = psutil.sensors_battery()
      time.sleep(0.100)
      print(f"Waiting for {device} to start charging.")

    while battery.percent < level:
      print(f"Waiting for {device} to be charged to {level}%."
            f" Currently at {battery.percent}%")
      battery = psutil.sensors_battery()
      time.sleep(10)

    print(f"Unplugging {device}...")
    self.turn_off(device)
    battery = psutil.sensors_battery()
    while battery.power_plugged:
      battery = psutil.sensors_battery()
      time.sleep(0.100)
      print(f"Waiting for {device} to stop charging.")

  def close(self):
    """Closes the message loop.
    """
    if self.closed:
      return
    self.closed = True
    self.loop.close()


def get_plug_controller(ip: str):
  return KasaPlugController(ip)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description='Controls kasa power switch connected to this device.')
  parser.add_argument("--kasa_switch_ip",
                      required=True,
                      help="IP address of the kasa power switch.")
  parser.add_argument("--charge_level",
                      type=int,
                      required=True,
                      help="Desired charge level.")
  args = parser.parse_args()

  kasa_plug_controller = KasaPlugController(args.kasa_switch_ip)
  kasa_plug_controller.charge_to(args.charge_level)
  kasa_plug_controller.close()
