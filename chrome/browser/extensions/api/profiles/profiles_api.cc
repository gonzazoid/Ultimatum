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

namespace SetUserAgent = api::profiles::SetUserAgent;
namespace SetUserAgentSubstitution = api::profiles::SetUserAgentSubstitution;

namespace {

}  // namespace

ProfilesAPI::ProfilesAPI(content::BrowserContext* context)
    : browser_context_(context) {}

ProfilesAPI::~ProfilesAPI() {
}


Profile* ProfilesFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

ExtensionFunction::ResponseAction ProfilesSetUserAgentFunction::Run() {
  std::optional<SetUserAgent::Params> params = SetUserAgent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs = GetProfile()->GetPrefs();

  prefs->SetString(prefs::kChameleonUserAgent, params->user_agent);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ProfilesSetUserAgentSubstitutionFunction::Run() {
  std::optional<SetUserAgentSubstitution::Params> params = SetUserAgentSubstitution::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kChameleonUserAgentSubstitution, params->user_agent_substitution);

  return RespondNow(NoArguments());
}

}  // namespace extensions
