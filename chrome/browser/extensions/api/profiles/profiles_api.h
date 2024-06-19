// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROFILES_PROFILES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROFILES_PROFILES_API_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/values.h"
#include "chrome/common/extensions/api/profiles.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

class ProfilesAPI {
 public:
  explicit ProfilesAPI(content::BrowserContext* context);
  ~ProfilesAPI();


 private:

  raw_ptr<content::BrowserContext> browser_context_;

};

// Base class for history function APIs.
class ProfilesFunction : public ExtensionFunction {
 protected:
  ~ProfilesFunction() override {}

  Profile* GetProfile() const;
};

class ProfilesSetUserAgentFunction : public ProfilesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("profiles.setUserAgent", PROFILES_SETUSERAGENT)

 protected:
  ~ProfilesSetUserAgentFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROFILES_PROFILES_API_H_
