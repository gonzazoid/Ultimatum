// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HSTSCACHE_HSTSCACHE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HSTSCACHE_HSTSCACHE_API_H_

#include <string>

#include "chrome/common/extensions/api/hsts_cache.h"
#include "extensions/browser/extension_function.h"
#include "services/network/public/mojom/hsts_cache_raw_api.mojom.h"

class Profile;

namespace extensions {

// Base class for hstsCache functions.
class HstsCacheFunction : public ExtensionFunction {
 public:
   explicit HstsCacheFunction();
 protected:
  ~HstsCacheFunction() override;

  Profile* GetProfile() const;
};

class HstsCacheKeysFunction : public HstsCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hstsCache.keys", HSTSCACHE_KEYS)

 protected:
  ~HstsCacheKeysFunction() override {}

  ResponseAction Run() override;

 private:
  void OnKeys(const ::network::mojom::HstsCacheKeysResponsePtr remote_response);
};

class HstsCacheGetEntryFunction : public HstsCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hstsCache.getEntry", HSTSCACHE_GETENTRY)

 protected:
  ~HstsCacheGetEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnEntry(const ::network::mojom::HstsCacheGetEntryResponsePtr remote_response);
};

class HstsCachePutEntryFunction : public HstsCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hstsCache.putEntry", HSTSCACHE_PUTENTRY)

 protected:
  ~HstsCachePutEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnEntrySaved(const ::std::string& status);
};

class HstsCacheDeleteEntryFunction : public HstsCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hstsCache.deleteEntry", HSTSCACHE_DELETEENTRY)

 protected:
  ~HstsCacheDeleteEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnEntryDeleted(const ::std::string& status);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HSTSCACHE_HSTSCACHE_API_H_
