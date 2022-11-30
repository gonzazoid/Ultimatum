// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hash_net/hash_net_background_fetcher.h"

#include <iostream>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/search/background/ntp_backgrounds.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

// namespace hash_net_ui {

HashNetBackgroundFetcher::HashNetBackgroundFetcher() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("hash_net_request", R"(
        semantics {
          sender: "Navi Onboarding NTP background module"
          description:
            "As part of the Navi Onboarding flow, the NTP background module "
            "allows users to preview what a custom background for the "
            "New Tab Page would look like. The list of available backgrounds "
            "is manually whitelisted."
          trigger:
            "The user selects an image to preview."
          data:
            "User-selected image URL."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings, but it is only "
            "triggered by a user action."
          policy_exception_justification: "Not implemented."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:3000/");
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);

  network::mojom::URLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, base::BindOnce(&HashNetBackgroundFetcher::OnFetchCompleted,
                                     base::Unretained(this)));
}

HashNetBackgroundFetcher::~HashNetBackgroundFetcher() = default;

void HashNetBackgroundFetcher::OnFetchCompleted(
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    std::cout << "RESPONSE!!!!\n" << *response_body << "\n\n\n";
  } else {
    std::cout << "FUCK!!!!\n";
  }
}

// }  // namespace welcome
