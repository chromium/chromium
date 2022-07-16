#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from jinja2 import Template
from jinja2 import FileSystemLoader
from jinja2.environment import Environment
from utils import BROWSERS_DEFINITION

import argparse
import os
import shutil
""" Script to generate browser driver scripts from templates.

The generated scripts can be used to have browsers go
through scenarios in a repeatable way.
"""


def get_render_targets(template_file, output_filename):
  """For certain scenarios more than one driver script is generated.
  Return a list dicts that describes them all."""

  # In the case of idle_on_site render for different sites.
  if template_file.endswith("idle_on_site"):
    render_targets = []
    render_targets.append({
        "output_filename":
        output_filename.replace("site", "wiki"),
        "idle_site":
        "http://www.wikipedia.com/wiki/Alessandro_Volta"
    })
    render_targets.append({
        "output_filename":
        output_filename.replace("site", "youtube"),
        "idle_site":
        "https://www.youtube.com/watch?v=9EE_ICC_wFw?autoplay=1"
    })
    return render_targets

  return [{"output_filename": output_filename}]


def render(file_prefix, template_file, process_name, extra_args):
  """Render a single scenario script."""

  if file_prefix:
    file_prefix = file_prefix.replace(" ", "_") + "_"
    file_prefix = file_prefix.lower()

  # Roughly the Alexa top 50 at the time of writing this.
  background_sites_list = [
      "https://google.com", "https://youtube.com", "https://tmall.com",
      "https://baidu.com", "https://qq.com", "https://sohu.com",
      "https://amazon.com", "https://taobao.com", "https://facebook.com",
      "https://360.cn", "https://yahoo.com", "https://jd.com",
      "https://wikipedia.org", "https://zoom.us", "https://sina.com.cn",
      "https://weibo.com", "https://live.com", "https://xinhuanet.com",
      "https://reddit.com", "https://microsoft.com", "https://netflix.com",
      "https://office.com", "https://microsoftonline.com",
      "https://okezone.com", "https://vk.com", "https://myshopify.com",
      "https://panda.tv", "https://alipay.com", "https://csdn.net",
      "https://instagram.com", "https://zhanqi.tv", "https://yahoo.co.jp",
      "https://ebay.com", "https://apple.com", "https://bing.com",
      "https://bongacams.com", "https://google.com.hk", "https://naver.com",
      "https://stackoverflow.com", "https://aliexpress.com",
      "https://twitch.tv", "https://amazon.co.jp", "https://amazon.in",
      "https://adobe.com", "https://tianya.cn", "https://huanqiu.com",
      "https://aparat.com", "https://amazonaws.com", "https://twitter.com",
      "https://yy.com"
  ]
  background_sites = ",".join(background_sites_list)

  output_filename = f"./driver_scripts/{file_prefix}{template_file}.scpt"

  for render_target in get_render_targets(template_file, output_filename):

    render_target = {**render_target, **extra_args}

    env = Environment()
    env.loader = FileSystemLoader('.')
    template = env.get_template("driver_scripts_templates/" + template_file)

    with open(render_target["output_filename"], 'w') as output:
      output.write(
          template.render(**render_target,
                          directory=os.getcwd(),
                          background_sites=background_sites,
                          navigation_cycles=30,
                          per_navigation_delay=30,
                          delay=3600,
                          browser=process_name))


def render_runner_scripts(extra_args):
  """Render all scenario driver scripts for all browsers (if applicable)."""

  # Generate all driver scripts from templates.
  for _, _, files in os.walk("./driver_scripts_templates"):
    for template_file in files:
      if not template_file.endswith(".scpt") and not template_file.endswith(
          ".swp"):
        if template_file.startswith("safari"):
          # Generate for Safari
          render("", template_file, "", extra_args)
        else:
          # Generate for all Chromium based browsers
          for browser in ['Chrome', 'Canary', "Chromium", "Edge"]:
            process_name = BROWSERS_DEFINITION[browser]["process_name"]
            render(browser, template_file, process_name, extra_args)


def generate_all(extra_args):
  """Delete all existing generated scripts. Scripts should not be
  modified by hand.
  """

  args = {"hash_bang": "#!/usr/bin/osascript"}
  args = {**args, **extra_args}

  shutil.rmtree("driver_scripts/", ignore_errors=True)
  os.makedirs("driver_scripts", exist_ok=True)

  # Generate scripts for all scenarios.
  render_runner_scripts(args)

  # Copy the files that don't need any substitutions.
  for _, _, files in os.walk("./driver_scripts_templates"):
    for script in files:
      if script.endswith(".scpt"):
        shutil.copyfile(f"./driver_scripts_templates/{script}",
                        f"./driver_scripts/{script}")


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description='Generate browser driver scripts to execute usage scenarios.')
  parser.add_argument("--meet_meeting_id",
                      help="ID of meeting for Meet base scnearios.",
                      required=False)
  args = parser.parse_args()

  extra_args = {}
  if args.meet_meeting_id:
    extra_args["meeting_id"] = args.meet_meeting_id

  generate_all(extra_args)
