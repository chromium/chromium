class ContributeToHistogram {
  async run(urls, data) {
    if (data.enableDebugMode) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
    for (const contribution of data.contributions) {
      privateAggregation.contributeToHistogram(contribution);
    }
    return 1;
  }
}

register('contribute-to-histogram', ContributeToHistogram);
