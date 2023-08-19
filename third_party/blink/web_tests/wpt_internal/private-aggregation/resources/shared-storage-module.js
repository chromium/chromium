class ContributeToHistogramOperation {
  async run(data) {
    if (data.enableDebugMode) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
    for (const contribution of data.contributions) {
      privateAggregation.contributeToHistogram(contribution);
    }
    if (data.enableDebugModeAfterOp) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
  }
}

register('contribute-to-histogram', ContributeToHistogramOperation);
