// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_URL_REQUEST_HOOKS_H_
#define EXTENSIONS_RENDERER_API_URL_REQUEST_HOOKS_H_

#include <string>

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8-forward.h"

namespace extensions {

// Custom hooks for the UrlRequest API.
class UrlRequestHooks : public APIBindingHooksDelegate {
 public:
  UrlRequestHooks();

  UrlRequestHooks(const UrlRequestHooks&) = delete;
  UrlRequestHooks& operator=(const UrlRequestHooks&) = delete;

  ~UrlRequestHooks() override;

  // APIBindingHooksDelegate:
  // Creates a new UrlRequest event.
  // TODO(devlin): UrlRequest events are a very unfortunate implementation
  // detail, but refactoring means changing a few parts of the internal API.
  // It's not impossible, but it's a bit involved. However, as we move more
  // towards native bindings, it's definitely something we'll want to do.
  bool CreateCustomEvent(v8::Local<v8::Context> context,
                         const std::string& event_name,
                         v8::Local<v8::Value>* event_out) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_URL_REQUEST_HOOKS_H_
