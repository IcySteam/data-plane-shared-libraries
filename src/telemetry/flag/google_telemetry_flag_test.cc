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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>

#include "absl/log/check.h"
#include "google/protobuf/util/message_differencer.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "src/logger/request_context_impl.h"

namespace privacy_sandbox::server_common::telemetry {
namespace {

using google::protobuf::util::MessageDifferencer;
using testing::ElementsAre;

class GoogleTelemetryFlagTest : public testing::Test {
 protected:
  void SetUp() override {
    static opentelemetry::logs::LoggerProvider* log_provider =
        std::make_unique<opentelemetry::logs::NoopLoggerProvider>().release();
    server_common::log::logger_private =
        log_provider->GetLogger("GoogleTelemetryFlagTest").get();
  };
};

TEST_F(GoogleTelemetryFlagTest, Parse) {
  std::string flag_str = R"pb(disable: false
                              override_metrics: "metric\\.1\\.a"
                              additional_metrics: "metric\\.2\\..+"
                              omitted_metrics: "metric\\.3\\..+")pb";
  GoogleTelemetryFlag f_parsed;
  std::string err;
  EXPECT_TRUE(AbslParseFlag(flag_str, &f_parsed, &err));
  EXPECT_EQ(f_parsed.server_config.disable(), false);
  EXPECT_THAT(f_parsed.server_config.override_metrics(),
              ElementsAre(R"(metric\.1\.a)"));
  EXPECT_THAT(f_parsed.server_config.additional_metrics(),
              ElementsAre(R"(metric\.2\..+)"));
  EXPECT_THAT(f_parsed.server_config.omitted_metrics(),
              ElementsAre(R"(metric\.3\..+)"));
}

TEST_F(GoogleTelemetryFlagTest, ParseError) {
  GoogleTelemetryFlag f_parsed;
  std::string err;
  EXPECT_FALSE(AbslParseFlag("cause_error", &f_parsed, &err));
}

TEST_F(GoogleTelemetryFlagTest, ParseUnParse) {
  GoogleTelemetryFlag f;
  f.server_config.set_disable(false);
  f.server_config.add_override_metrics(R"(metric\.1\.a)");
  f.server_config.add_additional_metrics(R"(metric\.2\..+)");
  f.server_config.add_omitted_metrics(R"(metric\.3\..+)");
  GoogleTelemetryFlag f_parsed;

  std::string err;
  AbslParseFlag(AbslUnparseFlag(f), &f_parsed, &err);
  EXPECT_TRUE(
      MessageDifferencer::Equals(f.server_config, f_parsed.server_config));
}

TEST(GoogleTelemetryConfigWrapper, TelemetryEnabled) {
  GoogleTelemetryConfig config;
  std::set<std::string> available_metrics{
      "metric.1.a", "metric.1.b", "metric.1.c", "metric.2.a", "metric.2.b",
      "metric.2.c", "metric.3.a", "metric.3.b", "metric.3.c"};
  std::set<std::string> default_metrics{"metric.1.b", "metric.3.a"};

  config.set_disable(false);

  absl::StatusOr<GoogleTelemetryConfigWrapper> config_wrapper =
      GoogleTelemetryConfigWrapper::Create(config, available_metrics,
                                           default_metrics);
  CHECK_OK(config_wrapper);
  EXPECT_EQ(config_wrapper.value().TelemetryEnabled(), true);

  config.set_disable(true);
  config_wrapper = GoogleTelemetryConfigWrapper::Create(
      config, available_metrics, default_metrics);
  CHECK_OK(config_wrapper);
  EXPECT_EQ(config_wrapper.value().TelemetryEnabled(), false);
}

TEST(GoogleTelemetryConfigWrapper, GetMetricsToExportOverrideMetrics) {
  GoogleTelemetryConfig config;
  std::set<std::string> available_metrics{
      "metric.1.a", "metric.1.b", "metric.1.c", "metric.2.a", "metric.2.b",
      "metric.2.c", "metric.3.a", "metric.3.b", "metric.3.c"};
  std::set<std::string> default_metrics{"metric.1.b", "metric.3.a"};

  config.set_disable(false);
  config.add_override_metrics(R"(metric\.1\.a)");
  config.add_additional_metrics(R"(metric\.2\..+)");
  config.add_omitted_metrics(R"(metric\.2\.b)");
  config.add_omitted_metrics(R"(metric\.3\..+)");

  absl::StatusOr<GoogleTelemetryConfigWrapper> config_wrapper =
      GoogleTelemetryConfigWrapper::Create(config, available_metrics,
                                           default_metrics);
  CHECK_OK(config_wrapper);
  EXPECT_THAT(config_wrapper.value().GetMetricsToExport(),
              ElementsAre("metric.1.a"));

  config.set_disable(true);
  config_wrapper = GoogleTelemetryConfigWrapper::Create(
      config, available_metrics, default_metrics);
  CHECK_OK(config_wrapper);
  EXPECT_EQ(config_wrapper.value().GetMetricsToExport().size(), 0);
}

TEST(GoogleTelemetryConfigWrapper,
     GetMetricsToExportDefaultWithAdditionalAndOmittedMetrics) {
  GoogleTelemetryConfig config;
  std::set<std::string> available_metrics{
      "metric.1.a", "metric.1.b", "metric.1.c", "metric.2.a", "metric.2.b",
      "metric.2.c", "metric.3.a", "metric.3.b", "metric.3.c"};
  std::set<std::string> default_metrics{"metric.1.b", "metric.3.a"};

  config.set_disable(false);
  config.add_additional_metrics(R"(metric\.2\..+)");
  config.add_omitted_metrics(R"(metric\.2\.b)");
  config.add_omitted_metrics(R"(metric\.3\..+)");

  absl::StatusOr<GoogleTelemetryConfigWrapper> config_wrapper =
      GoogleTelemetryConfigWrapper::Create(config, available_metrics,
                                           default_metrics);
  CHECK_OK(config_wrapper);
  EXPECT_THAT(config_wrapper.value().GetMetricsToExport(),
              ElementsAre("metric.1.b", "metric.2.a", "metric.2.c"));

  config.set_disable(true);
  config_wrapper = GoogleTelemetryConfigWrapper::Create(
      config, available_metrics, default_metrics);
  CHECK_OK(config_wrapper);
  EXPECT_EQ(config_wrapper.value().GetMetricsToExport().size(), 0);
}

}  // namespace
}  // namespace privacy_sandbox::server_common::telemetry
