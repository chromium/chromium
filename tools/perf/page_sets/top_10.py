# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class SimplePage(page_module.Page):
  def __init__(self, url, page_set, name=''):
    if name == '':
      name = url
    super(SimplePage, self).__init__(
        url, page_set=page_set, name=name,
        shared_page_state_class=shared_page_state.SharedDesktopPageState)

  def RunPageInteractions(self, action_runner):
    pass


class Google(SimplePage):
  def __init__(self, page_set):
    super(Google, self).__init__(
      url='https://www.google.com/#hl=en&q=barack+obama', page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(Google, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class Gmail(SimplePage):
  def __init__(self, page_set):
    super(Gmail, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(Gmail, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendar(SimplePage):
  def __init__(self, page_set):
    super(GoogleCalendar, self).__init__(
      url='https://www.google.com/calendar/',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(GoogleCalendar, self).RunNavigateSteps(action_runner)
    action_runner.ExecuteJavaScript('''
        (function() { var elem = document.createElement("meta");
          elem.name="viewport";
          elem.content="initial-scale=1";
          document.body.appendChild(elem);
        })();''')
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')


class Youtube(SimplePage):
  def __init__(self, page_set):
    super(Youtube, self).__init__(
      url='http://www.youtube.com',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(Youtube, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class Facebook(SimplePage):
  def __init__(self, page_set):
    super(Facebook, self).__init__(
      url='https://www.facebook.com/barackobama',
      page_set=page_set,
      name='Facebook')

  def RunNavigateSteps(self, action_runner):
    super(Facebook, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='About')


class Top10PageSet(story.StorySet):
  """10 Pages chosen from Alexa top sites"""

  def __init__(self):
    super(Top10PageSet, self).__init__(
      archive_data_file='data/top_10.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    # top google property; a google tab is often open
    self.AddStory(Google(self))

    # productivity, top google properties
    # TODO(dominikg): fix crbug.com/386152
    #self.AddStory(Gmail(self))

    # productivity, top google properties
    self.AddStory(GoogleCalendar(self))

    # #3 (Alexa global)
    self.AddStory(Youtube(self))

    # top social, Public profile
    self.AddStory(Facebook(self))

    # #6 (Alexa) most visited worldwide,Picked an interesting page
    self.AddStory(SimplePage('http://en.wikipedia.org/wiki/Wikipedia',
                                  self, name='Wikipedia'))

    # #1 world commerce website by visits; #3 commerce in the US by time spent
    self.AddStory(SimplePage('http://www.amazon.com', self))

    # #4 Alexa
    self.AddStory(SimplePage('http://www.yahoo.com/', self))

    # #16 Alexa
    self.AddStory(SimplePage('http://www.bing.com/', self))

    # #20 Alexa
    self.AddStory(SimplePage('http://www.ask.com/', self))
