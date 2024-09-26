#include <string>
#include "url/gurl.h"

namespace net {
  class HashNetRequestManager {
   public:
    HashNetRequestManager(const std::string& agentsList, const GURL& hash_net_url);
    ~HashNetRequestManager();

    bool HasNextHashNetAgent();
    std::string GetNextHashNetAgentRequestUrl();

   private:
    unsigned int attempt_;
    std::vector<std::string> agents_;
    GURL hash_net_url_;
  };
}
