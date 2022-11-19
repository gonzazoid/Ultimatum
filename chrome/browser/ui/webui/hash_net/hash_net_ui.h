// Copyright 2022 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HASH_NET_HASH_NET_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HASH_NET_HASH_NET_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI handler for hash:// scheme.
class HashNetUI : public content::WebUIController {
 public:
  explicit HashNetUI(content::WebUI* web_ui);

  HashNetUI(const HashNetUI&) = delete;
  HashNetUI& operator=(const HashNetUI&) = delete;

  ~HashNetUI() override;

};

#endif  // CHROME_BROWSER_UI_WEBUI_HASH_NET_HASH_NET_UI_H_
