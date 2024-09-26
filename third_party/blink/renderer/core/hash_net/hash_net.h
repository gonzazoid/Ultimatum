#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HASH_NET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HASH_NET_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class LocalDOMWindow;
class ScriptState;

class CORE_EXPORT HashNet {
  STATIC_ONLY(HashNet);

 public:
  static String getHashNetPublicKey(
                  ScriptState* script_state,
                  LocalDOMWindow& window
                );
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HASH_NET_H_
