# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms
from page_sets.login_helpers import linkedin_login


class PathologicalMobileSitesPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.PATHOLOGICAL_MOBILE_SITES]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(PathologicalMobileSitesPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class CnnPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'cnn_pathological'
  YEAR = '2018'
  URL = 'http://edition.cnn.com'


class EspnPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'espn_pathological'
  YEAR = '2018'
  URL = 'http://www.espn.com/nhl/standings'


class RecodePathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'recode_pathological'
  YEAR = '2018'
  URL = 'http://recode.net'


class YahooSportsPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'yahoo_sports_pathological'
  YEAR = '2018'
  URL = 'http://sports.yahoo.com/'


class LaTimesPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'latimes_pathological'
  YEAR = '2018'
  URL = 'http://www.latimes.com'


class PbsPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'pbs_pathological'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://www.pbs.org/newshour/bb/much-really-cost-live-city-like-seattle/#the-rundown'


class GuardianPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'guardian_pathological'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://www.theguardian.com/politics/2015/mar/09/ed-balls-tory-spending-plans-nhs-charging'


class ZDNetPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'zdnet_pathological'
  YEAR = '2018'
  URL = 'http://www.zdnet.com'


class WowWikkiPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'wow_wiki_pathological'
  YEAR = '2018'
  URL = 'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria'


class LinkedInPathological2018Page(PathologicalMobileSitesPage):
  BASE_NAME = 'linkedin_pathological'
  YEAR = '2018'
  URL = 'https://www.linkedin.com/in/linustorvalds'

  def RunNavigateSteps(self, action_runner):
    linkedin_login.LoginMobileAccount(action_runner, 'linkedin')
    super(LinkedInPathological2018Page, self).RunNavigateSteps(action_runner)
