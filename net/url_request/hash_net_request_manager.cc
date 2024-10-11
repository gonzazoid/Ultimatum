#include "net/url_request/hash_net_request_manager.h"

#include "base/strings/string_util.h"

bool ShouldBeOnlyOne(const std::string& substr, const std::string& str) {
  size_t pos = str.find(substr);
  if (pos == std::string::npos) return false;
  size_t next = str.find(substr, pos + substr.length());
  return next == std::string::npos;
}

bool IsValidHashNetAgentUrlPattern(const std::string& url) {
  GURL gurl = GURL(url);

  if (!gurl.is_valid()) return false;
  if (!gurl.SchemeIsHTTPOrHTTPS()) return false;

  return (
    ShouldBeOnlyOne("{{request}}", url) &&
    ShouldBeOnlyOne("{{function}}", url) &&
    ShouldBeOnlyOne("{{path}}", url)
  );
}

namespace net {
  HashNetRequestManager::HashNetRequestManager(const std::string& agentsList, const GURL& hash_net_url)
      : attempt_(0),
        agents_(),
        hash_net_url_(hash_net_url) {
    std::string url;
    std::istringstream f(agentsList);
    while(std::getline(f, url, '\n')) {
      if (IsValidHashNetAgentUrlPattern(url)) {
        agents_.push_back(url);
      }
    }
  }

  HashNetRequestManager::~HashNetRequestManager() = default;

  bool HashNetRequestManager::HasNextHashNetAgent() {
    return attempt_ != agents_.size();
  }

  std::string HashNetRequestManager::GetNextHashNetAgentRequestUrl() {
    if(attempt_ == agents_.size()) return "";

    std::string agent_url = agents_[attempt_];
    attempt_++;

    base::ReplaceFirstSubstringAfterOffset(&agent_url, 0, "{{request}}", hash_net_url_.scheme());
    base::ReplaceFirstSubstringAfterOffset(&agent_url, 0, "{{function}}", hash_net_url_.host());
    base::ReplaceFirstSubstringAfterOffset(&agent_url, 0, "{{path}}", hash_net_url_.path().substr(1));

    return agent_url;
  }
}
