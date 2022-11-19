// Copyright 2022 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HASH_NET_HASH_NET_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HASH_NET_HASH_NET_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

// Handler class for HashNet.
class HashNetHandler : public content::WebUIMessageHandler {
 public:
  HashNetHandler();

  HashNetHandler(const HashNetHandler&) = delete;
  HashNetHandler& operator=(const HashNetHandler&) = delete;

  ~HashNetHandler() override;

  // content::WebUIMessageHandler implementation.
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

};

#endif  // CHROME_BROWSER_UI_WEBUI_HASH_NET_HASH_NET_HANDLER_H_
