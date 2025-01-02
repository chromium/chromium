// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <mutex>
#include <deque>
#include <map>
#include <sstream>

#include "core/graph/onnx_protobuf.h"
#include "onnx/defs/schema.h"
#include "core/graph/constants.h"
#include "core/common/common.h"
#include "core/common/status.h"

namespace onnxruntime {
using OpName_Domain_Version_Schema_Map = std::unordered_map<
    std::string,
    std::unordered_map<std::string, std::map<ONNX_NAMESPACE::OperatorSetVersion, ONNX_NAMESPACE::OpSchema>>>;

/**
@struct SchemaRegistryVersion
onnxruntime schema registry is a supplement to the built-in ONNX schema.
Every schema registry represent a collection of schema deltas from baseline_opset_version to opset_version
*/
struct SchemaRegistryVersion {
  int baseline_opset_version;
  int opset_version;
};

using DomainToVersionMap = std::unordered_map<std::string, int>;
using DomainToVersionRangeMap = std::unordered_map<std::string, SchemaRegistryVersion>;

class IOnnxRuntimeOpSchemaCollection : public ONNX_NAMESPACE::ISchemaRegistry {
 public:
  virtual DomainToVersionMap GetLatestOpsetVersions(bool is_onnx_only) const = 0;

  using ISchemaRegistry::GetSchema;

  const ONNX_NAMESPACE::OpSchema* GetSchema(const std::string& key, const int maxInclusiveVersion,
                                            const std::string& domain) const final {
    const ONNX_NAMESPACE::OpSchema* latest_schema = nullptr;
    int earliest_opset_where_unchanged = std::numeric_limits<int>::max();
    GetSchemaAndHistory(key, maxInclusiveVersion, domain, &latest_schema, &earliest_opset_where_unchanged);

    assert(latest_schema == nullptr || (latest_schema->SinceVersion() <= maxInclusiveVersion &&
                                        earliest_opset_where_unchanged == latest_schema->SinceVersion()));

    return latest_schema;
  }

  virtual void GetSchemaAndHistory(
      const std::string& key,
      int maxInclusiveVersion,
      const std::string& domain,
      const ONNX_NAMESPACE::OpSchema** latest_schema,
      int* earliest_opset_where_unchanged) const = 0;
};

/**
@class OnnxRuntimeOpSchemaRegistry

OnnxRuntimeOpSchemaRegistry is used to provide supplement for built-in ONNX schemas.
Each OnnxRuntimeOpSchemaRegistry must register complete opsets delta from a baseline version to max opset version.
(Please notice that baseline opsets are not include in the delta)

For example, ONNXRuntime is build with ONNX 1.2 which is at opset7, to use ONNX opset8 and opset9,
user could create a OnnxRuntimeOpSchemaRegistry and config it as {baseline_opset_version = 7, opset_version = 9}
it means this OnnxRuntimeOpSchemaRegistry contains the complete delta from opset7 to opset9.
*/
class OnnxRuntimeOpSchemaRegistry : public IOnnxRuntimeOpSchemaCollection {
 public:
  OnnxRuntimeOpSchemaRegistry() = default;

  common::Status SetBaselineAndOpsetVersionForDomain(
      const std::string& domain,
      int baseline_opset_version,
      int opset_version);

  DomainToVersionMap GetLatestOpsetVersions(bool is_onnx_only) const override;

  // OnnxRuntimeOpSchemaRegistry must register complete delta for a opset.
  common::Status RegisterOpSet(
      std::vector<ONNX_NAMESPACE::OpSchema>& schemas,
      const std::string& domain,
      int baseline_opset_version,
      int opset_version);

  using IOnnxRuntimeOpSchemaCollection::GetSchema;

  void GetSchemaAndHistory(const std::string& key, int maxInclusiveVersion, const std::string& domain,
                           const ONNX_NAMESPACE::OpSchema** latest_schema,
                           int* earliest_opset_where_unchanged) const override;

  bool empty() const {
    return map_.empty();
  }

 private:
  common::Status RegisterOpSchema(ONNX_NAMESPACE::OpSchema&& op_schema);

  common::Status RegisterOpSchemaInternal(ONNX_NAMESPACE::OpSchema&& op_schema);

  std::mutex mutex_;

  OpName_Domain_Version_Schema_Map map_;
  DomainToVersionRangeMap domain_version_range_map_;
};

/**
@class SchemaRegistryManager

SchemaRegistryManager provides a view based on built-in ONNX schema and a list of
OnnxRuntimeOpSchemaRegistry as supplement.

The user needs to make sure the customized schema registry is valid, otherwise the behavior is undefined.

@todo We may add more consistency checks later.
*/
class SchemaRegistryManager : public onnxruntime::IOnnxRuntimeOpSchemaCollection {
 public:
  /**
  Register a new schema registry instance.
  @remarks The schema registry priority is the reverse of registration order. i.e. the last registry added will be
  searched first for a matching OpSchema.
  */
  void RegisterRegistry(std::shared_ptr<IOnnxRuntimeOpSchemaCollection> registry);

  /** Gets the latest opset versions.
  @param is_onnx_only If true, return the latest ONNX schemas. If false, return the latest schemas for all domains.
  */
  DomainToVersionMap GetLatestOpsetVersions(bool is_onnx_only) const override;

  /** Gets the last released opset versions.
  @param is_onnx_only If true, return ONNX schemas only. If false, return the schemas for all domains.
  */
  DomainToVersionMap GetLastReleasedOpsetVersions(bool is_onnx_only) const;
  /**
  Gets the OpSchema and its history.
  Searches custom schema registries starting with the last one added. \
  If the OpSchema is not found the default ONNX schema registry is searched.

  @param key Operator type.
  @param max_inclusive_version Maximum opset version allowed, inclusive.
  @param domain The domain of the operator.
  @param[out] latest_schema Returns the latest OpSchema if found. nullptr otherwise.
  @param[out] earliest_opset_where_unchanged The earliest opset version preceding max_inclusive_version where the
  operator is known to be unchanged.
  */
  void GetSchemaAndHistory(const std::string& key, int max_inclusive_version, const std::string& domain,
                           const ONNX_NAMESPACE::OpSchema** latest_schema,
                           int* earliest_opset_where_unchanged) const override;

 private:
  void GetDomainToVersionMapForRegistries(DomainToVersionMap& domain_version_map, bool is_onnx_only) const;

  std::deque<std::shared_ptr<IOnnxRuntimeOpSchemaCollection>> registries;
};

}  // namespace onnxruntime
