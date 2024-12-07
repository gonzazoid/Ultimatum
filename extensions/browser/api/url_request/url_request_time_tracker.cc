// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_time_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"

ExtensionUrlRequestTimeTracker::RequestTimeLog::RequestTimeLog() = default;
ExtensionUrlRequestTimeTracker::RequestTimeLog::~RequestTimeLog() = default;

ExtensionUrlRequestTimeTracker::ExtensionUrlRequestTimeTracker() = default;
ExtensionUrlRequestTimeTracker::~ExtensionUrlRequestTimeTracker() = default;

void ExtensionUrlRequestTimeTracker::LogRequestStartTime(
    int64_t request_id,
    const base::TimeTicks& start_time,
    bool has_listener) {
  auto iter = request_time_logs_.find(request_id);
  if (iter != request_time_logs_.end())
    return;

  RequestTimeLog& log = request_time_logs_[request_id];
  log.request_start_time = start_time;
  log.has_listener = has_listener;
  log.has_extra_headers_listener = false;
}

void ExtensionUrlRequestTimeTracker::LogBeforeRequestDispatchTime(
    int64_t request_id,
    base::TimeTicks dispatch_time) {
  auto iter = request_time_logs_.find(request_id);
  CHECK(iter != request_time_logs_.end(), base::NotFatalUntil::M130);
  iter->second.before_request_listener_dispatch_time = dispatch_time;
}

void ExtensionUrlRequestTimeTracker::LogBeforeRequestCompletionTime(
    int64_t request_id,
    base::TimeTicks completion_time) {
  auto iter = request_time_logs_.find(request_id);
  if (iter == request_time_logs_.end()) {
    // This probably *shouldn't* happen, but there's enough subtlety in handling
    // network requests that we handle it gracefully.
    return;
  }

  iter->second.before_request_listener_completion_time = completion_time;
}

void ExtensionUrlRequestTimeTracker::LogRequestEndTime(
    int64_t request_id,
    const base::TimeTicks& end_time) {
  auto iter = request_time_logs_.find(request_id);
  if (iter == request_time_logs_.end())
    return;

  AnalyzeLogRequest(iter->second, end_time);

  request_time_logs_.erase(iter);
}

void ExtensionUrlRequestTimeTracker::AnalyzeLogRequest(
    const RequestTimeLog& log,
    const base::TimeTicks& end_time) {
  base::TimeDelta request_duration = end_time - log.request_start_time;

  if (log.has_listener) {
    UMA_HISTOGRAM_TIMES("Extensions.UrlRequest.TotalRequestTime",
                        request_duration);
  }

  if (log.has_extra_headers_listener) {
    UMA_HISTOGRAM_TIMES("Extensions.UrlRequest.TotalExtraHeadersRequestTime",
                        request_duration);
  }

  if (log.block_duration.is_zero())
    return;

  UMA_HISTOGRAM_TIMES("Extensions.UrlRequest.TotalBlockingRequestTime",
                      request_duration);
  UMA_HISTOGRAM_TIMES("Extensions.NetworkDelay", log.block_duration);

  // Ignore really short requests. Time spent on these is negligible, and any
  // extra delay the extension adds is likely to be noise.
  constexpr auto kMinRequestTimeToCare = base::Milliseconds(10);
  if (request_duration >= kMinRequestTimeToCare) {
    const int percentage =
        base::ClampRound(log.block_duration / request_duration * 100);
    UMA_HISTOGRAM_PERCENTAGE("Extensions.NetworkDelayPercentage", percentage);
  }

  constexpr int kBucketCount = 50;

  // Record the time spent in listeners in onBeforeRequest. Only do this if
  // we have a time for both the dispatch and completion time (we may not,
  // if the request were canceled).
  if (!log.before_request_listener_dispatch_time.is_null() &&
      !log.before_request_listener_completion_time.is_null()) {
    base::TimeDelta listener_time =
        log.before_request_listener_completion_time -
        log.before_request_listener_dispatch_time;
    UMA_HISTOGRAM_TIMES(
        "Extensions.UrlRequest.BeforeRequestListenerEvaluationTime."
        /* "UrlRequestAndDeclarativeNetRequest" */,
        listener_time);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.UrlRequest."
        "BeforeRequestListenerEvaluationTimeInMicroseconds."
        /* "UrlRequestAndDeclarativeNetRequest" */ ,
        listener_time, base::Microseconds(1), base::Seconds(30),
        kBucketCount);
  }
}

void ExtensionUrlRequestTimeTracker::IncrementTotalBlockTime(
    int64_t request_id,
    const base::TimeDelta& block_time) {
  auto iter = request_time_logs_.find(request_id);
  if (iter != request_time_logs_.end())
    iter->second.block_duration += block_time;
}

void ExtensionUrlRequestTimeTracker::SetRequestCanceled(int64_t request_id) {
  // Canceled requests won't actually hit the network, so we can't compare
  // their request time to the time spent waiting on the extension. Just ignore
  // them.
  request_time_logs_.erase(request_id);
}

void ExtensionUrlRequestTimeTracker::SetRequestRedirected(int64_t request_id) {
  // When a request is redirected, we have no way of knowing how long the
  // request would have taken, so we can't say how much an extension slowed
  // down this request. Just ignore it.
  request_time_logs_.erase(request_id);
}
