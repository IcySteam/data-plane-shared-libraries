/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORE_HTTP2_CLIENT_HTTP2_CLIENT_H_
#define CORE_HTTP2_CLIENT_HTTP2_CLIENT_H_

#include <memory>

#include "absl/time/time.h"
#include "src/core/common/operation_dispatcher/operation_dispatcher.h"
#include "src/core/http2_client/error_codes.h"
#include "src/core/http2_client/http_connection_pool.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core {

struct HttpClientOptions {
  HttpClientOptions()
      : retry_strategy_options(common::RetryStrategyOptions(
            common::RetryStrategyType::Exponential,
            kDefaultRetryStrategyDelayInMs, kDefaultRetryStrategyMaxRetries)),
        max_connections_per_host(kDefaultMaxConnectionsPerHost),
        http2_read_timeout_in_sec(kDefaultHttp2ReadTimeoutInSeconds) {}

  HttpClientOptions(common::RetryStrategyOptions retry_strategy_options,
                    size_t max_connections_per_host,
                    TimeDuration http2_read_timeout_in_sec)
      : retry_strategy_options(retry_strategy_options),
        max_connections_per_host(max_connections_per_host),
        http2_read_timeout_in_sec(http2_read_timeout_in_sec) {}

  /// Retry strategy options.
  const common::RetryStrategyOptions retry_strategy_options;
  /// Max http connections per host.
  const size_t max_connections_per_host;
  /// nghttp client read timeout.
  const TimeDuration http2_read_timeout_in_sec;
};

/*! @copydoc HttpClientInterface
 */
class HttpClient : public HttpClientInterface {
 public:
  /**
   * @brief Construct a new Http Client object
   *
   * @param async_executor an instance of the async executor.
   * @param retry_strategy_type retry strategy type.
   * @param time_duration_ms delay time duration in ms for http client retry
   * strategy.
   * @param total_retries total retry counts.
   * TODO: Params are outdated.
   */
  explicit HttpClient(AsyncExecutorInterface* async_executor,
                      HttpClientOptions options = HttpClientOptions());

  ExecutionResult Stop() noexcept;

  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept override;

  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context,
      const absl::Duration& timeout) noexcept override;

 private:
  /// An instance of the connection pool that is used by the http client.
  std::unique_ptr<HttpConnectionPool> http_connection_pool_;

  /// Operation dispatcher
  common::OperationDispatcher operation_dispatcher_;
};
}  // namespace google::scp::core

#endif  // CORE_HTTP2_CLIENT_HTTP2_CLIENT_H_
