# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import page_cycler_story
from telemetry import story
from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import shared_page_state


class IntlEsFrPtBrPage(page_cycler_story.PageCyclerStory):

  def __init__(self, url, page_set, cache_temperature=None):
    if cache_temperature == cache_temperature_module.COLD:
      temp_suffix = '_cold'
    elif cache_temperature == cache_temperature_module.WARM:
      temp_suffix = '_warm'
    else:
      raise NotImplementedError

    super(IntlEsFrPtBrPage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=shared_page_state.SharedDesktopPageState,
        cache_temperature=cache_temperature,
        name=url + temp_suffix)


class IntlEsFrPtBrPageSet(story.StorySet):

  """
  Popular pages in Romance languages Spanish, French and Brazilian Portuguese.
  """

  def __init__(self, cache_temperatures=(cache_temperature_module.COLD,
                                         cache_temperature_module.WARM)):
    super(IntlEsFrPtBrPageSet, self).__init__(
      archive_data_file='data/intl_es_fr_pt-BR.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)
    if cache_temperatures is None:
      cache_temperatures = [cache_temperature_module.ANY]

    urls_list = [
      'http://elmundo.es/',
      'http://terra.es/',
      # pylint: disable=line-too-long
      'http://www.ebay.es/sch/i.html?_sacat=382&_trkparms=clkid%3D6548971389060485883&_qi=RTM1381637',
      'http://www.eltiempo.es/talavera-de-la-reina.html',
      'http://www.free.fr/adsl/index.html',
      'http://www.voila.fr/',
      'http://www.leboncoin.fr/annonces/offres/limousin/',
      'http://www.orange.fr/',
      # Why: #5 site in Brazil
      'http://www.uol.com.br/',
      # Why: #10 site in Brazil
      # pylint: disable=line-too-long
      'http://produto.mercadolivre.com.br/MLB-468424957-pelicula-protetora-smartphone-h5500-e-h5300-43-frete-free-_JM'
    ]

    for url in urls_list:
      for temp in cache_temperatures:
        self.AddStory(IntlEsFrPtBrPage(url, self, cache_temperature=temp))
