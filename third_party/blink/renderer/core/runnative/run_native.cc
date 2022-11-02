#include "third_party/blink/renderer/core/runnative/run_native.h"

namespace blink {

String RunNative::runNative(ScriptState* script_state,
                            LocalDOMWindow& window
                           ) {
  UseCounter::Count(window.GetExecutionContext(), WebFeature::kFetch);
  if (!window.GetFrame()) {
    return "No way!!!";
  }
  return "Hello, sailor!!!!";
}

}  // namespace blink
