// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/telemetry/flag/google_telemetry_flag.h"

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "google/protobuf/text_format.h"
#include "re2/re2.h"

namespace privacy_sandbox::server_common::telemetry {

namespace {

template <typename T>
inline absl::StatusOr<T> ParseText(std::string_view text) {
  T message;
  if (!google::protobuf::TextFormat::ParseFromString(text.data(), &message)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid proto format:{", text, "}"));
  }
  return message;
}

}  // namespace

bool AbslParseFlag(std::string_view text, GoogleTelemetryFlag* flag,
                   std::string* err) {
  absl::StatusOr<GoogleTelemetryConfig> s =
      ParseText<GoogleTelemetryConfig>(text);
  if (!s.ok()) {
    *err = s.status().message();
    return false;
  }
  flag->server_config = *s;
  return true;
}

std::string AbslUnparseFlag(const GoogleTelemetryFlag& flag) {
  return flag.server_config.ShortDebugString();
}

// static
absl::StatusOr<GoogleTelemetryConfigWrapper>
GoogleTelemetryConfigWrapper::Create(
    GoogleTelemetryConfig config,
    const std::set<std::string_view>& available_metrics,
    const std::set<std::string_view>& default_metrics) {
  auto result = CheckArgsValidity(config, available_metrics, default_metrics);
  if (!result.ok()) {
    return result;
  }
  return GoogleTelemetryConfigWrapper(std::move(config), available_metrics,
                                      default_metrics);
}

// static
absl::Status GoogleTelemetryConfigWrapper::CheckArgsValidity(
    const GoogleTelemetryConfig& config,
    const std::set<std::string_view>& available_metrics,
    const std::set<std::string_view>& default_metrics) {
  return absl::OkStatus();
}

bool GoogleTelemetryConfigWrapper::TelemetryEnabled() const {
  if (server_config_.disable()) {
    return false;
  }
  return true;
}

bool GoogleTelemetryConfigWrapper::MetricsEnabled() const {
  return TelemetryEnabled();
}

std::set<std::string_view> GoogleTelemetryConfigWrapper::GetMetricsToExport() {
  if (has_calculated_metrics_to_export_) {
    return metrics_to_export_;
  }

  if (!MetricsEnabled()) {
    metrics_to_export_ = {};
    has_calculated_metrics_to_export_ = true;
    return metrics_to_export_;
  }

  if (!server_config_.override_metrics().empty()) {
    for (const auto& pattern : server_config_.override_metrics()) {
      AddMatchedMetricsToSet(metrics_to_export_, RE2(pattern));
    }
    return metrics_to_export_;
  }

  metrics_to_export_ = default_metrics_;

  for (const auto& pattern : server_config_.additional_metrics()) {
    AddMatchedMetricsToSet(metrics_to_export_, RE2(pattern));
  }

  for (const auto& pattern : server_config_.omitted_metrics()) {
    RemoveMatchedMetricsFromSet(metrics_to_export_, RE2(pattern));
  }

  ABSL_LOG(INFO) << "All available metrics exportable to Google: ";
  for (const auto& metric : available_metrics_) {
    ABSL_LOG(INFO).NoPrefix() << metric;
  }
  ABSL_LOG(INFO) << "Override metric patterns to export to Google: ";
  for (const auto& metric : server_config_.override_metrics()) {
    ABSL_LOG(INFO).NoPrefix() << metric;
  }
  ABSL_LOG(INFO) << "Default metrics to export to Google: ";
  for (const auto& metric : default_metrics_) {
    ABSL_LOG(INFO).NoPrefix() << metric;
  }
  ABSL_LOG(INFO)
      << "Default metrics - additional metric patterns to export to Google: ";
  for (const auto& metric : server_config_.additional_metrics()) {
    ABSL_LOG(INFO).NoPrefix() << metric;
  }
  ABSL_LOG(INFO)
      << "Default metrics - metric patterns omitted from export to Google: ";
  for (const auto& metric : server_config_.omitted_metrics()) {
    ABSL_LOG(INFO).NoPrefix() << metric;
  }
  ABSL_LOG(INFO) << "Final metrics to export to Google: ";
  for (const auto& metric : metrics_to_export_) {
    ABSL_LOG(INFO).NoPrefix() << metric;
  }

  has_calculated_metrics_to_export_ = true;
  return metrics_to_export_;
}

GoogleTelemetryConfigWrapper::GoogleTelemetryConfigWrapper(
    GoogleTelemetryConfig&& config,
    const std::set<std::string_view>& available_metrics,
    const std::set<std::string_view>& default_metrics)
    : server_config_(config),
      available_metrics_(available_metrics),
      default_metrics_(default_metrics),
      has_calculated_metrics_to_export_(false) {}

std::vector<std::string_view> GoogleTelemetryConfigWrapper::GetMatchedMetrics(
    const RE2& r) {
  std::vector<std::string_view> metrics;
  bool found_match = false;
  for (auto it = available_metrics_.begin(); it != available_metrics_.end();
       ++it) {
    if (RE2::FullMatch(*it, r)) {
      metrics.emplace_back(*it);
      found_match = true;
      ABSL_LOG(INFO) << "Found metric " << *it << " matching pattern "
                     << r.pattern();
    }
  }
  if (!found_match) {
    ABSL_LOG(WARNING) << "No matching metric found for pattern " << r.pattern();
  }
  return metrics;
}

void GoogleTelemetryConfigWrapper::AddMatchedMetricsToSet(
    std::set<std::string_view>& metrics, const RE2& r) {
  const std::vector<std::string_view> matched_metrics = GetMatchedMetrics(r);
  for (const auto& matched_metric : matched_metrics) {
    if (metrics.find(matched_metric) == metrics.end()) {
      metrics.insert(matched_metric);
    }
  }
}

void GoogleTelemetryConfigWrapper::RemoveMatchedMetricsFromSet(
    std::set<std::string_view>& metrics, const RE2& r) {
  const std::vector<std::string_view> matched_metrics = GetMatchedMetrics(r);
  for (const auto& matched_metric : matched_metrics) {
    if (metrics.find(matched_metric) != metrics.end()) {
      metrics.erase(matched_metric);
    }
  }
}

}  // namespace privacy_sandbox::server_common::telemetry
