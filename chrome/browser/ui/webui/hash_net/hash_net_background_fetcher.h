// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HASH_NET_BACKGROUND_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_HASH_NET_BACKGROUND_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "content/public/browser/web_ui_data_source.h"

namespace network {
class SimpleURLLoader;
}

// namespace hash_net_ui {

class HashNetBackgroundFetcher {
 public:
  HashNetBackgroundFetcher();

  HashNetBackgroundFetcher(const HashNetBackgroundFetcher&) = delete;
  HashNetBackgroundFetcher& operator=(const HashNetBackgroundFetcher&) = delete;

  ~HashNetBackgroundFetcher();

 private:
  void OnFetchCompleted(std::unique_ptr<std::string> response_body);

  content::WebUIDataSource::GotDataCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
};

// }  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_FETCHER_H_
