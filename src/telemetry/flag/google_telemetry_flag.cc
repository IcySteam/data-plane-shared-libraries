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
#include "src/logger/request_context_impl.h"
#include "src/logger/request_context_logger.h"

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
GoogleTelemetryConfigWrapper::Create(GoogleTelemetryConfig config,
                                     std::set<std::string> available_metrics,
                                     std::set<std::string> default_metrics) {
  auto result = CheckArgsValidity(config, available_metrics, default_metrics);
  if (!result.ok()) {
    return result;
  }
  return GoogleTelemetryConfigWrapper(std::move(config),
                                      std::move(available_metrics),
                                      std::move(default_metrics));
}

// static
absl::Status GoogleTelemetryConfigWrapper::CheckArgsValidity(
    const GoogleTelemetryConfig& config,
    const std::set<std::string>& available_metrics,
    const std::set<std::string>& default_metrics) {
  return absl::OkStatus();
}

bool GoogleTelemetryConfigWrapper::TelemetryEnabled() const {
  if (server_config_.disable()) {
    return false;
  }
  return true;
}

std::set<std::string> GoogleTelemetryConfigWrapper::GetMetricsToExport() {
  return metrics_to_export_;
}

GoogleTelemetryConfigWrapper::GoogleTelemetryConfigWrapper(
    GoogleTelemetryConfig&& config, std::set<std::string>&& available_metrics,
    std::set<std::string>&& default_metrics)
    : server_config_(config),
      available_metrics_(available_metrics),
      default_metrics_(default_metrics) {
  if (!TelemetryEnabled()) {
    metrics_to_export_ = {};
    PS_LOG(INFO, log::SystemLogContext::Get())
        << "Telemetry export to Google is disabled.";
    return;
  }

  if (!server_config_.override_metrics().empty()) {
    for (const auto& pattern : server_config_.override_metrics()) {
      const std::set<std::string> matched_metrics =
          GetMatchedMetrics(RE2(pattern));
      metrics_to_export_.insert(matched_metrics.begin(), matched_metrics.end());
    }
  } else {
    metrics_to_export_ = default_metrics_;

    for (const auto& pattern : server_config_.additional_metrics()) {
      const std::set<std::string> matched_metrics =
          GetMatchedMetrics(RE2(pattern));
      metrics_to_export_.insert(matched_metrics.begin(), matched_metrics.end());
    }

    for (const auto& pattern : server_config_.omitted_metrics()) {
      const std::set<std::string> matched_metrics =
          GetMatchedMetrics(RE2(pattern));
      for (const auto& metric : matched_metrics) {
        metrics_to_export_.erase(metric);
      }
    }
  }

  std::string final_metrics;
  for (const auto& metric : metrics_to_export_) {
    final_metrics.append("\n");
    final_metrics.append(metric);
  }
  PS_LOG(INFO, log::SystemLogContext::Get())
      << "Final metrics to export to Google: " << final_metrics;
}

std::set<std::string> GoogleTelemetryConfigWrapper::GetMatchedMetrics(
    const RE2& r) {
  if (!r.ok()) {
    PS_LOG(ERROR, log::SystemLogContext::Get())
        << "Invalid regex pattern '" << r.pattern() << "': " << r.error();
    return {};
  }

  std::set<std::string> metrics;
  bool found_match = false;
  for (const auto& metric : available_metrics_) {
    if (RE2::FullMatch(metric, r)) {
      metrics.insert(metric);
      found_match = true;
      PS_LOG(INFO, log::SystemLogContext::Get())
          << "Found metric " << metric << " matching pattern '" << r.pattern()
          << "'";
    }
  }
  if (!found_match) {
    PS_LOG(WARNING, log::SystemLogContext::Get())
        << "No matching metric found for pattern '" << r.pattern() << "'";
  }
  return metrics;
}

}  // namespace privacy_sandbox::server_common::telemetry
