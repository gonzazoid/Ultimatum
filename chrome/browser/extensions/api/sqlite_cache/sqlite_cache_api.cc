// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sqlite_cache/sqlite_cache_api.h"

#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/history/core/browser/history_service.h"
#include "components/favicon/core/favicon_service.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

SqliteCacheFunction::SqliteCacheFunction() {}
SqliteCacheFunction::~SqliteCacheFunction() {}

Profile* SqliteCacheFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

SqliteCacheExecFunction::SqliteCacheExecFunction() {}
SqliteCacheExecFunction::~SqliteCacheExecFunction() {}

void SqliteCacheExecFunction::OnResponse(std::unique_ptr<sql::SqliteResponse> remote_response) {
  if (remote_response->status != "ok") {
    Respond(Error(remote_response->status));
    return;
  }

  base::ListValue json_rows;
  for (const auto& row : *remote_response->result) {
    base::ListValue json_row;
    for (const auto& column : row) {
      switch (column.type) {
        case sql::ColumnType::kInteger: {
          base::DictValue integer;
          integer.Set("type", "int");
          integer.Set("value", column.value);
          json_row.Append(std::move(integer));
          break;
        }
        case sql::ColumnType::kFloat:
          json_row.Append(base::Value(std::stod(column.value)));
          break;
        case sql::ColumnType::kText:
          json_row.Append(base::Value(column.value));
          break;
        case sql::ColumnType::kBlob:
          json_row.Append(base::Value(*column.buffer));
          break;
        case sql::ColumnType::kNull:
          json_row.Append(base::Value());
          break;
      }
    }
    json_rows.Append(std::move(json_row));
  }
  Respond(WithArguments(std::move(json_rows)));
  Release();
}

ExtensionFunction::ResponseAction SqliteCacheExecFunction::Run() {
  std::optional<api::sqlite_cache::Exec::Params> params = api::sqlite_cache::Exec::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* profile = GetProfile();

  if (params->storage == "faviconCache") {
    favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
    if (favicon_service) {
      tracker_ = std::make_unique<base::CancelableTaskTracker>();
      sql::SqliteResponseCallback callback = base::BindOnce(&SqliteCacheExecFunction::OnResponse, base::Unretained(this));
      favicon_service->ExecRawSql(params->sql_request, std::move(params->bindings), std::move(callback), tracker_.get());
    } else {
      return RespondNow(Error("favicon service not found"));
    }
  } else if (params->storage == "historyCache") {
    history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      tracker_ = std::make_unique<base::CancelableTaskTracker>();
      sql::SqliteResponseCallback callback = base::BindOnce(&SqliteCacheExecFunction::OnResponse, base::Unretained(this));
      history_service->ExecHistoryRawSql(params->sql_request, std::move(params->bindings), std::move(callback), tracker_.get());
    } else {
      return RespondNow(Error("history service not found"));
    }
  } else {
    return RespondNow(Error("unknown storage"));
  }

  AddRef();
  return RespondLater();
}

}  // namespace extensions
