// Copyright 2022 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hash_net/hash_net_handler.h"
#include "components/hash_net_ui/hash_net_ui_constants.h"

HashNetHandler::HashNetHandler() {}
HashNetHandler::~HashNetHandler() {}

void HashNetHandler::MakeNetworkRequest(const base::Value::List& args) {
  _fetcher = std::make_unique<HashNetBackgroundFetcher>();
}

void HashNetHandler::OnJavascriptDisallowed() {}
void HashNetHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      hash_net_ui::kHashNetRequest,
      base::BindRepeating(&HashNetHandler::MakeNetworkRequest,
                          base::Unretained(this)));
}
