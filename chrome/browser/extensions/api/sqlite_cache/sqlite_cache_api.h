// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SQLITECACHE_SQLITECACHE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SQLITECACHE_SQLITECACHE_API_H_

#include <string>

#include "base/task/cancelable_task_tracker.h"
#include "chrome/common/extensions/api/sqlite_cache.h"
#include "extensions/browser/extension_function.h"
#include "sql/statement.h"

class Profile;

namespace extensions {

// Base class for sqliteCache functions.
class SqliteCacheFunction : public ExtensionFunction {
 public:
  explicit SqliteCacheFunction();
 protected:
  ~SqliteCacheFunction() override;

  Profile* GetProfile() const;

 private:

};

class SqliteCacheExecFunction : public SqliteCacheFunction {
 public:
  explicit SqliteCacheExecFunction();
  DECLARE_EXTENSION_FUNCTION("sqliteCache.exec", SQLITECACHE_EXEC)

 protected:
  ~SqliteCacheExecFunction() override;

  ResponseAction Run() override;

 private:
  void OnResponse(std::unique_ptr<sql::SqliteResponse> remote_response);
  std::unique_ptr<base::CancelableTaskTracker> tracker_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SQLITECACHE_SQLITECACHE_API_H_
