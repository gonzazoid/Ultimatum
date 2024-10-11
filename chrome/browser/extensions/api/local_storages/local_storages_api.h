// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOCALSTORAGES_LOCALSTORAGES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOCALSTORAGES_LOCALSTORAGES_API_H_

#include <string>

#include "mojo/public/cpp/bindings/remote.h"
#include "chrome/common/extensions/api/local_storages.h"
#include "extensions/browser/extension_function.h"
#include "components/services/storage/public/mojom/local_storage_raw.mojom.h"

class Profile;

namespace extensions {

// Base class for localStorages functions.
class LocalStoragesFunction : public ExtensionFunction {
 public:
   explicit LocalStoragesFunction();
 protected:
  ~LocalStoragesFunction() override;

  Profile* GetProfile() const;

 private:

};

class LocalStoragesKeysFunction : public LocalStoragesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("localStorages.keys", LOCALSTORAGES_KEYS)

 protected:
  ~LocalStoragesKeysFunction() override {}

  ResponseAction Run() override;

 private:
  void OnKeys(const ::storage::mojom::LocalStorageKeysResponsePtr remote_response);
};

class LocalStoragesGetEntryFunction : public LocalStoragesFunction {
 public:

  DECLARE_EXTENSION_FUNCTION("localStorages.getEntry", LOCALSTORAGES_GETENTRY)

 protected:
  ~LocalStoragesGetEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnEntry(const ::storage::mojom::LocalStorageGetEntryResponsePtr remote_response);
};

class LocalStoragesPutEntryFunction : public LocalStoragesFunction {
 public:

  DECLARE_EXTENSION_FUNCTION("localStorages.putEntry", LOCALSTORAGES_PUTENTRY)

 protected:
  ~LocalStoragesPutEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnEntrySaved(const ::std::string& status);
};

class LocalStoragesDeleteEntryFunction : public LocalStoragesFunction {
 public:

  DECLARE_EXTENSION_FUNCTION("localStorages.deleteEntry", LOCALSTORAGES_DELETEENTRY)

 protected:
  ~LocalStoragesDeleteEntryFunction() override {}

  ResponseAction Run() override;

 private:
  void OnEntryDeleted(const ::std::string& status);
};

class LocalStoragesFlushFunction : public LocalStoragesFunction {
 public:

  DECLARE_EXTENSION_FUNCTION("localStorages.flush", LOCALSTORAGES_FLUSH)

 protected:
  ~LocalStoragesFlushFunction() override {}

  ResponseAction Run() override;

 private:

};

class LocalStoragesPurgeMemoryFunction : public LocalStoragesFunction {
 public:

  DECLARE_EXTENSION_FUNCTION("localStorages.purgeMemory", LOCALSTORAGES_PURGEMEMORY)

 protected:
  ~LocalStoragesPurgeMemoryFunction() override {}

  ResponseAction Run() override;

 private:

};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOCALSTORAGES_LOCALSTORAGES_API_H_
