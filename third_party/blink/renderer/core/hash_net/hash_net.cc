#include "third_party/blink/renderer/core/hash_net/hash_net.h"

namespace blink {

String HashNet::getHashNetPublicKey(ScriptState* script_state,
                            LocalDOMWindow& window
                           ) {
  UseCounter::Count(window.GetExecutionContext(), WebFeature::kFetch);
  if (!window.GetFrame()) return "";
  return String(window.GetFrame()->GetHashNetPublicKey());
}

}  // namespace blink
