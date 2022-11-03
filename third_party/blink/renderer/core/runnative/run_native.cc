#include "third_party/blink/renderer/core/runnative/run_native.h"

namespace blink {

ScriptPromise RunNative::runNative(ScriptState* script_state,
                            LocalDOMWindow& window
                           ) {
  UseCounter::Count(window.GetExecutionContext(), WebFeature::kFetch);
  if (!window.GetFrame()) {
    return ScriptPromise();
  }
  Resolver resolver(script_state);
  ScriptPromise promise = resolver.Promise();
  resolver.Resolve(V8String(script_state->GetIsolate(), "Hello, sailor!!!!"));
  return promise;
}

}  // namespace blink
