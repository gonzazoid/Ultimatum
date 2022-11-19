// Copyright 2022 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/common/url_constants.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/hash_net/hash_net_ui.h"
#include "chrome/browser/ui/webui/hash_net/hash_net_handler.h"

#include "components/grit/components_resources.h"
#include "components/hash_net_ui/hash_net_ui_constants.h"

#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using content::WebUIDataSource;

namespace {

WebUIDataSource* CreateHashNetUIDataSource() {
  WebUIDataSource* html_source =
      WebUIDataSource::Create(content::kHashNetUIScheme);

  html_source->AddResourcePath(hash_net_ui::kHashNetJS, IDR_HASH_NET_UI_JS);
  html_source->SetDefaultResource(IDR_HASH_NET_UI_HTML);

  return html_source;
}

}  // namespace

HashNetUI::HashNetUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<HashNetHandler>());

  WebUIDataSource::Add(profile, CreateHashNetUIDataSource());
}

HashNetUI::~HashNetUI() {}
