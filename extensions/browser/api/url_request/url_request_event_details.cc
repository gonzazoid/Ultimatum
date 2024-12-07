// Copyright 2016 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_event_details.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/url_request/url_request_api_constants.h"
#include "extensions/browser/api/url_request/url_request_api_helpers.h"
#include "extensions/browser/api/url_request/url_request_info.h"
#include "extensions/browser/api/url_request/url_request_permissions.h"
#include "extensions/browser/api/url_request/url_request_resource_type.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/auth.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

namespace helpers = extension_url_request_api_helpers;
namespace keys = extension_url_request_api_constants;

namespace extensions {
namespace {

}  // namespace

UrlRequestEventDetails::UrlRequestEventDetails(const UrlRequestInfo& request,
                                               int extra_info_spec)
    : extra_info_spec_(extra_info_spec),
      render_process_id_(content::ChildProcessHost::kInvalidUniqueID) {
  dict_.Set(keys::kMethodKey, request.method);
  dict_.Set(keys::kRequestIdKey, base::NumberToString(request.id));
  dict_.Set(keys::kTimeStampKey,
            base::Time::Now().InMillisecondsFSinceUnixEpoch());
  dict_.Set(keys::kTypeKey,
            UrlRequestResourceTypeToString(request.url_request_type));
  dict_.Set(keys::kUrlKey, request.url.spec());
  dict_.Set(keys::kTabIdKey, request.frame_data.tab_id);
  dict_.Set(keys::kFrameIdKey, request.frame_data.frame_id);
  dict_.Set(keys::kParentFrameIdKey, request.frame_data.parent_frame_id);
  if (request.frame_data.document_id) {
    dict_.Set(keys::kDocumentIdKey, request.frame_data.document_id.ToString());
  }
  if (request.frame_data.parent_document_id) {
    dict_.Set(keys::kParentDocumentIdKey,
              request.frame_data.parent_document_id.ToString());
  }
  if (request.frame_data.frame_id >= 0) {
    dict_.Set(keys::kFrameTypeKey, ToString(request.frame_data.frame_type));
    dict_.Set(keys::kDocumentLifecycleKey,
              ToString(request.frame_data.document_lifecycle));
  }
  initiator_ = request.initiator;
  render_process_id_ = request.render_process_id;
}

UrlRequestEventDetails::~UrlRequestEventDetails() = default;

void UrlRequestEventDetails::SetRequestBody(UrlRequestInfo* request) {
  request_body_ = std::nullopt;
  if (request->request_body_data) {
    request_body_ = std::move(request->request_body_data);
    request->request_body_data.reset();
  }
}

void UrlRequestEventDetails::SetRequestHeaders(
    const net::HttpRequestHeaders& request_headers) {

  request_headers_ = base::Value::List();
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    request_headers_->Append(
        helpers::CreateHeaderDictionary(it.name(), it.value()));
  }
}

base::Value::Dict UrlRequestEventDetails::GetFilteredDict(
    int extra_info_spec,
    UrlRequestPermissionHelper* permission_helper,
    const extensions::ExtensionId& extension_id,
    bool crosses_incognito) const {
  base::Value::Dict result = dict_.Clone();
  if (/* (extra_info_spec & ExtraInfoSpec::REQUEST_BODY) && */ request_body_) {
    result.Set(keys::kRequestBodyKey, request_body_->Clone());
  }
  if (/* (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS) && */ request_headers_) {

    base::Value::List request_headers = request_headers_->Clone();
    result.Set(keys::kRequestHeadersKey, std::move(request_headers));
  }

  // Only listeners with a permission for the initiator should receive it.
  if (initiator_) {
    int tab_id = dict_.FindInt(keys::kTabIdKey).value_or(-1);
    if (initiator_->opaque() ||
        UrlRequestPermissions::CanExtensionAccessInitiator(
            permission_helper, extension_id, initiator_, tab_id,
            crosses_incognito)) {
      result.Set(keys::kInitiatorKey, initiator_->Serialize());
    }
  }
  return result;
}

base::Value::Dict UrlRequestEventDetails::GetAndClearDict() {
  return std::move(dict_);
}

}  // namespace extensions
