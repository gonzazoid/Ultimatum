// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DISKCACHE_DISKCACHE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DISKCACHE_DISKCACHE_API_H_

#include <string>

#include "mojo/public/cpp/bindings/remote.h"
#include "chrome/common/extensions/api/disk_cache.h"
#include "extensions/browser/extension_function.h"
#include "third_party/blink/public/mojom/cache_storage_raw/cache_storage_raw.mojom.h"
#include "services/network/public/mojom/disk_cache_raw_api.mojom.h"

class Profile;

namespace extensions {

class DiskCacheAPI {
 public:
  explicit DiskCacheAPI(content::BrowserContext* context);
  ~DiskCacheAPI();

 private:

  raw_ptr<content::BrowserContext> browser_context_;

};

// Base class for diskCache functions.
class DiskCacheFunction : public ExtensionFunction {
 public:
   explicit DiskCacheFunction();
 protected:
  ~DiskCacheFunction() override;

  void Init();
  void GetPath(std::string target, base::OnceCallback<void (const base::FilePath&)>);
  Profile* GetProfile() const;

  mojo::Remote<blink::mojom::CacheStorageRaw> cache_storage_;
 private:

};

class DiskCacheKeysFunction : public DiskCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("diskCache.keys", DISKCACHE_KEYS)

 protected:
  ~DiskCacheKeysFunction() override {}

  ResponseAction Run() override;

 private:
  void OnPath(const base::FilePath& response);
  void OnKeys(const ::network::mojom::DiskCacheKeysResponsePtr remote_response);
};

class DiskCacheGetEntryFunction : public DiskCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("diskCache.getEntry", DISKCACHE_GETENTRY)

 protected:
  ~DiskCacheGetEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnPath(std::string key, const base::FilePath& response);
  void OnEntry(const ::network::mojom::DiskCacheEntryResponsePtr response);
};

class DiskCachePutEntryFunction : public DiskCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("diskCache.putEntry", DISKCACHE_PUTENTRY)

 protected:
  ~DiskCachePutEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnPath(::network::mojom::DiskCacheEntryPtr entry, const base::FilePath& response);
  void OnEntrySaved(const ::std::string& status);
};

class DiskCacheDeleteEntryFunction : public DiskCacheFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("diskCache.deleteEntry", DISKCACHE_DELETEENTRY)

 protected:
  ~DiskCacheDeleteEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnPath(std::string key, const base::FilePath& response);
  void OnEntryDeleted(const ::std::string& status);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DISKCACHE_DISKCACHE_API_H_
