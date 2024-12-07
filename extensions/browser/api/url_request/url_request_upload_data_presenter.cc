// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/url_request/url_request_upload_data_presenter.h"

#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/browser/api/url_request/url_request_form_data_parser.h"
#include "extensions/browser/api/url_request/url_request_api_constants.h"
#include "net/base/upload_file_element_reader.h"

namespace keys = extension_url_request_api_constants;

namespace {

// Takes |dictionary| of <string, list of strings> pairs, and gets the list
// for |key|, creating it if necessary.
base::Value::List& GetOrCreateList(base::Value::Dict& dictionary,
                                   const std::string& key) {
  base::Value::List* list = dictionary.FindList(key);
  if (list)
    return *list;
  return dictionary.Set(key, base::Value::List())->GetList();
}

}  // namespace

namespace extensions {

namespace subtle {

void UrlRequestAppendKeyValuePair(const char* key,
                        base::Value value,
                        base::Value::List& list) {
  base::Value::Dict dictionary;
  dictionary.Set(key, std::move(value));
  list.Append(std::move(dictionary));
}

}  // namespace subtle

UrlRequestUploadDataPresenter::~UrlRequestUploadDataPresenter() = default;

UrlRequestRawDataPresenter::UrlRequestRawDataPresenter() = default;

UrlRequestRawDataPresenter::~UrlRequestRawDataPresenter() = default;

void UrlRequestRawDataPresenter::FeedBytes(std::string_view bytes) {
  FeedNextBytes(bytes.data(), bytes.size());
}

void UrlRequestRawDataPresenter::FeedFile(const base::FilePath& path) {
  FeedNextFile(path.AsUTF8Unsafe());
}

bool UrlRequestRawDataPresenter::Succeeded() {
  return true;
}

std::optional<base::Value> UrlRequestRawDataPresenter::TakeResult() {
  return base::Value(std::move(list_));
}

void UrlRequestRawDataPresenter::FeedNextBytes(const char* bytes, size_t size) {
  subtle::UrlRequestAppendKeyValuePair(
      keys::kRequestBodyRawBytesKey,
      base::Value(base::as_bytes(base::make_span(bytes, size))), list_);
}

void UrlRequestRawDataPresenter::FeedNextFile(const std::string& filename) {
  // Insert the file path instead of the contents, which may be too large.
  subtle::UrlRequestAppendKeyValuePair(keys::kRequestBodyRawFileKey,
                             base::Value(filename), list_);
}

UrlRequestParsedDataPresenter::UrlRequestParsedDataPresenter(
    const net::HttpRequestHeaders& request_headers)
    : parser_(UrlRequestFormDataParser::Create(request_headers)),
      success_(parser_ != nullptr) {
  if (success_)
    dictionary_.emplace();
}

UrlRequestParsedDataPresenter::~UrlRequestParsedDataPresenter() = default;

void UrlRequestParsedDataPresenter::FeedBytes(std::string_view bytes) {
  if (!success_)
    return;

  if (!parser_->SetSource(bytes)) {
    Abort();
    return;
  }

  UrlRequestFormDataParser::Result result;
  while (parser_->GetNextNameValue(&result)) {
    base::Value::List& list =
        GetOrCreateList(dictionary_.value(), result.name());
    list.Append(result.take_value());
  }
}

void UrlRequestParsedDataPresenter::FeedFile(const base::FilePath& path) {}

bool UrlRequestParsedDataPresenter::Succeeded() {
  if (success_ && !parser_->AllDataReadOK())
    Abort();
  return success_;
}

std::optional<base::Value> UrlRequestParsedDataPresenter::TakeResult() {
  if (!success_)
    return std::nullopt;
  return base::Value(std::move(dictionary_.value()));
}

// static
std::unique_ptr<UrlRequestParsedDataPresenter> UrlRequestParsedDataPresenter::CreateForTests() {
  return base::WrapUnique(
      new UrlRequestParsedDataPresenter("application/x-www-form-urlencoded"));
}

UrlRequestParsedDataPresenter::UrlRequestParsedDataPresenter(const std::string& form_type)
    : parser_(UrlRequestFormDataParser::CreateFromContentTypeHeader(&form_type)),
      success_(parser_.get() != nullptr) {
  if (success_)
    dictionary_.emplace();
}

void UrlRequestParsedDataPresenter::Abort() {
  success_ = false;
  dictionary_.reset();
  parser_.reset();
}

}  // namespace extensions
