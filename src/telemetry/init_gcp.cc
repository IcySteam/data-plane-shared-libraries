// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string_view>

#include <grpcpp/security/credentials.h>

#include <nlohmann/json.hpp>

#include "absl/strings/substitute.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/sdk/trace/random_id_generator_factory.h"

#include "init.h"

namespace {

constexpr std::string_view kAudience = "//iam.googleapis.com/$0";
constexpr std::string_view kImpersonationUrl =
    "https://iamcredentials.googleapis.com/v1/projects/-/serviceAccounts/"
    "$0:generateAccessToken";
constexpr std::string_view kStsTokenUrl = "https://sts.googleapis.com/v1/token";
constexpr std::string_view kAttestationTokenPath =
    "/run/container_launcher/attestation_verifier_claims_token";
constexpr std::string_view kMetricWriterScope =
    "https://www.googleapis.com/auth/monitoring.write";

}  // namespace

namespace privacy_sandbox::server_common {
std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateSpanExporter(
    absl::optional<std::string> collector_endpoint) {
  opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
  if (collector_endpoint.has_value() && !collector_endpoint->empty()) {
    opts.endpoint = *collector_endpoint;
  }
  return opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(opts);
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
CreatePeriodicExportingMetricReader(
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        reader_options,
    absl::optional<std::string> collector_endpoint) {
  opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions exporter_options;
  if (collector_endpoint.has_value() && !collector_endpoint->empty()) {
    exporter_options.endpoint = *collector_endpoint;
  }

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
      opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
          exporter_options);
  return std::make_unique<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), reader_options);
}

absl::StatusOr<std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>>
CreatePeriodicExportingMetricReaderForGoogleTelemetry(
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        reader_options,
    std::string otlp_endpoint, std::string quota_project,
    std::string wip_provider, std::string service_account_to_impersonate) {
  opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions exporter_options;
  exporter_options.endpoint = otlp_endpoint;
  exporter_options.metadata.insert(
      {std::string("x-goog-user-project"), quota_project});

  // Create configuration for External Account Credentials
  const nlohmann::json configuration{
      {"type", "external_account"},
      {"audience", absl::Substitute(kAudience, wip_provider)},
      {"subject_token_type", "urn:ietf:params:oauth:token-type:jwt"},
      {"token_url", kStsTokenUrl},
      {"service_account_impersonation_url",
       absl::Substitute(kImpersonationUrl, service_account_to_impersonate)},
      {"credential_source", nlohmann::json{{"file", kAttestationTokenPath}}},
  };
  std::string credentials_json = configuration.dump();

  // Create External Account Credentials
  auto ssl_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  auto call_creds = grpc::ExternalAccountCredentials(
      credentials_json, {grpc::string(kMetricWriterScope)});
  auto channel_creds = grpc::CompositeChannelCredentials(ssl_creds, call_creds);
  exporter_options.credentials = channel_creds;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
      opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
          exporter_options);
  return std::make_unique<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), reader_options);
}

std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> CreateIdGenerator() {
  return opentelemetry::sdk::trace::RandomIdGeneratorFactory::Create();
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
CreateLogRecordExporter(absl::optional<std::string> collector_endpoint) {
  opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions opts;
  if (collector_endpoint.has_value() && !collector_endpoint->empty()) {
    opts.endpoint = *collector_endpoint;
  }
  return opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterFactory::
      Create(opts);
}

}  // namespace privacy_sandbox::server_common
