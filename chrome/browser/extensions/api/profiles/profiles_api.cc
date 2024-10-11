// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/profiles/profiles_api.h"

#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/extensions/api/profiles.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace SetParameter = api::profiles::SetParameter;
namespace SetParameterSubstitution = api::profiles::SetParameterSubstitution;

namespace {

}  // namespace

ProfilesAPI::ProfilesAPI(content::BrowserContext* context)
    : browser_context_(context) {}

ProfilesAPI::~ProfilesAPI() {
}


Profile* ProfilesFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

// parameter
ExtensionFunction::ResponseAction ProfilesSetParameterFunction::Run() {
  std::optional<SetParameter::Params> params = SetParameter::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<std::pair<std::string, std::string>> names = {
    std::make_pair<std::string, std::string>("navigator.userAgent", prefs::kChameleonUserAgent),
    std::make_pair<std::string, std::string>("navigator.productSub", prefs::kChameleonProductSub),
    std::make_pair<std::string, std::string>("navigator.platform", prefs::kChameleonPlatform),
    std::make_pair<std::string, std::string>("navigator.vendor", prefs::kChameleonVendor),
  };
  auto it = std::find_if(
    names.begin(),
    names.end(),
    [&params](std::pair<std::string, std::string>& element){ return element.first == params->key;}
  );
  if (it == names.end()) {
    std::string error = params->key + " key not supported";
    return RespondNow(Error(std::move(error)));
  }

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetString(it->second, params->value);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ProfilesSetParameterSubstitutionFunction::Run() {
  std::optional<SetParameterSubstitution::Params> params = SetParameterSubstitution::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<std::pair<std::string, std::string>> names = {
    std::make_pair<std::string, std::string>("navigator.userAgent", prefs::kChameleonUserAgentSubstitution),
    std::make_pair<std::string, std::string>("navigator.productSub", prefs::kChameleonProductSubSubstitution),
    std::make_pair<std::string, std::string>("navigator.platform", prefs::kChameleonPlatformSubstitution),
    std::make_pair<std::string, std::string>("navigator.vendor", prefs::kChameleonVendorSubstitution),
  };

  auto it = std::find_if(
    names.begin(),
    names.end(),
    [&params](std::pair<std::string, std::string>& element){ return element.first == params->key;}
  );
  if (it == names.end()) {
    std::string error = params->key + " key not supported";
    return RespondNow(Error(std::move(error)));
  }

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(it->second, params->value);

  return RespondNow(NoArguments());
}

}  // namespace extensions
