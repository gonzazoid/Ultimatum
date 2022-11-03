#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RUN_NATIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RUN_NATIVE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/core_export.h"


namespace blink {

typedef ScriptPromise::InternalResolver Resolver;
class LocalDOMWindow;
class ScriptState;

class CORE_EXPORT RunNative {
  STATIC_ONLY(RunNative);

 public:
  static ScriptPromise runNative(ScriptState* script_state,
                          LocalDOMWindow& window
                         );
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_RUN_NATIVE_H_
