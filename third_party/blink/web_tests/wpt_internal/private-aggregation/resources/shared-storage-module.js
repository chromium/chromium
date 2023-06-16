class ContributeToHistogramOperation {
  async run(data) {
    if (data.enableDebugMode) {
      privateAggregation.enableDebugMode();
    }
    for (const contribution of data.contributions) {
      privateAggregation.contributeToHistogram(contribution);
    }
  }
}

register('contribute-to-histogram', ContributeToHistogramOperation);
