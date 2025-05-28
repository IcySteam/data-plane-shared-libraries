/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SERVICES_COMMON_TELEMETRY_GOOGLE_TELEMETRY_FLAG_H_
#define SERVICES_COMMON_TELEMETRY_GOOGLE_TELEMETRY_FLAG_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "re2/re2.h"
#include "src/metric/definition.h"
#include "src/telemetry/flag/config.pb.h"

namespace privacy_sandbox::server_common::telemetry {

struct GoogleTelemetryFlag {
  GoogleTelemetryConfig server_config;
};

bool AbslParseFlag(std::string_view text, GoogleTelemetryFlag* flag,
                   std::string* err);

std::string AbslUnparseFlag(const GoogleTelemetryFlag&);

class GoogleTelemetryConfigWrapper {
 public:
  static absl::StatusOr<GoogleTelemetryConfigWrapper> Create(
      GoogleTelemetryConfig config, std::set<std::string> available_metrics,
      std::set<std::string> default_metrics);

  bool TelemetryEnabled() const;

  std::set<std::string> GetMetricsToExport();

 private:
  static absl::Status CheckArgsValidity(
      const GoogleTelemetryConfig& config,
      const std::set<std::string>& available_metrics,
      const std::set<std::string>& default_metrics);

  GoogleTelemetryConfigWrapper(GoogleTelemetryConfig&& config,
                               std::set<std::string>&& available_metrics,
                               std::set<std::string>&& default_metrics);

  std::set<std::string> GetMatchedMetrics(const RE2& r);

  GoogleTelemetryConfig server_config_;

  std::set<std::string> available_metrics_;
  std::set<std::string> default_metrics_;

  std::set<std::string> metrics_to_export_;
};

}  // namespace privacy_sandbox::server_common::telemetry

#endif  // SERVICES_COMMON_TELEMETRY_GOOGLE_TELEMETRY_FLAG_H_
