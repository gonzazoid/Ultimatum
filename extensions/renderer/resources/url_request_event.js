// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CHECK = requireNative('logging').CHECK;
var idGeneratorNatives = requireNative('id_generator');
var utils = require('utils');
var urlRequestInternal = getInternalApi('urlRequestInternal');
const isServiceWorkerContext =
    requireNative('service_worker_natives').IsServiceWorkerContext();

// Returns an ID that is either globally unique (in this process) or unique
// within this given context. Note that we use separate prefixes ('g' and 's')
// to ensure there are no collisions between these two groups.
function getGloballyUniqueSubEventName(eventName) {
  return eventName + '/g' + idGeneratorNatives.GetNextId();
}
function getScopedUniqueSubEventName(eventName) {
  return eventName + '/s' + idGeneratorNatives.GetNextScopedId();
}

// A sub-event-name uses a suffix with an additional identifier. For service
// worker contexts, we use a context-specific identifier; this allows multiple
// runs of the service worker script to produce subevents with the same IDs.
// For non-service worker contexts, we need to use a global identifier. This is
// because there may be multiple contexts, each with listeners (such as multiple
// webviews [https://crbug.com/1309302] or multiple frames
// [https://crbug.com/1297276]) that run in the same process. This would result
// in collisions between the event listener IDs in the urlRequest API. This
// isn't an issue with service worker contexts because, even though they run in
// the same process, they have additional identifiers of the service worker
// thread and version.
function getUniqueSubEventName(eventName) {
  return isServiceWorkerContext ?
      getScopedUniqueSubEventName(eventName) :
      getGloballyUniqueSubEventName(eventName);
}

// UrlRequestEventImpl object. This is used for special urlRequest events
// with extra parameters. Each invocation of addListener creates a new named
// sub-event. That sub-event is associated with the extra parameters in the
// browser process, so that only it is dispatched when the main event occurs
// matching the extra parameters.
// Note: this is not used for the onActionIgnored event.
//
// Example:
//   chrome.urlRequest.onBeforeRequest.addListener(
//       callback, {urls: 'http://*.google.com/*'});
//   ^ callback will only be called for onBeforeRequests matching the filter.
function UrlRequestEventImpl(eventName, opt_argSchemas, opt_extraArgSchemas,
                             opt_eventOptions, opt_webViewInstanceId) {
  if (typeof eventName != 'string')
    throw new Error('chrome.UrlRequestEvent requires an event name.');

  bindingUtil.addCustomSignature(eventName, opt_extraArgSchemas);

  this.eventName = eventName;
  this.argSchemas = opt_argSchemas;
  this.extraArgSchemas = opt_extraArgSchemas;
  this.webViewInstanceId = opt_webViewInstanceId || 0;
  this.subEvents = [];
}
$Object.setPrototypeOf(UrlRequestEventImpl.prototype, null);

// Test if the given callback is registered for this event.
UrlRequestEventImpl.prototype.hasListener = function(cb) {
  return this.findListener_(cb) > -1;
};

// Test if any callbacks are registered fur thus event.
UrlRequestEventImpl.prototype.hasListeners = function() {
  return this.subEvents.length > 0;
};

// Registers a callback to be called when this event is dispatched. If
// opt_filter is specified, then the callback is only called for events that
// match the given filters. If opt_extraInfo is specified, the given optional
// info is sent to the callback.
UrlRequestEventImpl.prototype.addListener =
    function(cb, opt_filter) {
  // NOTE(benjhayden) New APIs should not use this subEventName trick! It does
  // not play well with event pages. See downloads.onDeterminingFilename and
  // ExtensionDownloadsEventRouter for an alternative approach.
  var subEventName = getUniqueSubEventName(this.eventName);
  // Note: this could fail to validate, in which case we would not add the
  // subEvent listener.
  bindingUtil.validateCustomSignature(this.eventName,
                                      $Array.slice(arguments, 1));
  urlRequestInternal.addEventListener(
      cb, opt_filter, this.eventName, subEventName,
      this.webViewInstanceId);

  var supportsFilters = false;
  var supportsLazyListeners = true;
  var subEvent =
      bindingUtil.createCustomEvent(subEventName, supportsFilters,
                                    supportsLazyListeners);

  var subEventCallback = cb;

  var eventName = this.eventName;
  var webViewInstanceId = this.webViewInstanceId;
  subEventCallback = function() {
    var requestId = arguments[0].requestId;
    try {
      var result = $Function.apply(cb, null, arguments);
      if (result.response) {
        Promise.resolve(result.response)
          .then(response => {
            if (!(response instanceof Response)) {
              throw new Error ("not instance of Response");
            }
            response.arrayBuffer().then(buf => {
              const headers = [];
              for (const key of response.headers.keys()) {
                headers.push({ name: key, value: response.headers.get(key) }); // TODO binaryValue
              };
              const responseToSend = {
                body: buf,
                headers,
                status: `${response.status}`,
                statusText: `${response.statusText}`
              };
              result.response = responseToSend; 
              urlRequestInternal.eventHandled(
                  eventName, subEventName, requestId, webViewInstanceId, result);
            });
          });
      } else {
        urlRequestInternal.eventHandled(
            eventName, subEventName, requestId, webViewInstanceId, result);
      }
    } catch (e) {
      urlRequestInternal.eventHandled(
          eventName, subEventName, requestId, webViewInstanceId);
      throw e;
    }
  };

  $Array.push(this.subEvents,
      {subEvent: subEvent, callback: cb, subEventCallback: subEventCallback});
  subEvent.addListener(subEventCallback);
};

// Unregisters a callback.
UrlRequestEventImpl.prototype.removeListener = function(cb) {
  var idx;
  while ((idx = this.findListener_(cb)) >= 0) {
    var e = this.subEvents[idx];
    e.subEvent.removeListener(e.subEventCallback);
    if (e.subEvent.hasListeners()) {
      console.error(
          'Internal error: urlRequest subEvent has orphaned listeners.');
    }
    $Array.splice(this.subEvents, idx, 1);
  }
};

UrlRequestEventImpl.prototype.findListener_ = function(cb) {
  for (var i in this.subEvents) {
    var e = this.subEvents[i];
    if (e.callback === cb) {
      if (e.subEvent.hasListener(e.subEventCallback))
        return i;
      console.error('Internal error: urlRequest subEvent has no callback.');
    }
  }

  return -1;
};

UrlRequestEventImpl.prototype.addRules = function(rules, opt_cb) {
  throw new Error('This event does not support rules.');
};

UrlRequestEventImpl.prototype.removeRules =
    function(ruleIdentifiers, opt_cb) {
  throw new Error('This event does not support rules.');
};

UrlRequestEventImpl.prototype.getRules = function(ruleIdentifiers, cb) {
  throw new Error('This event does not support rules.');
};

function UrlRequestEvent() {
  privates(UrlRequestEvent).constructPrivate(this, arguments);
}

// Our util code requires we construct a new UrlRequestEvent via a call to
// 'new UrlRequestEvent', which wouldn't work well with calling a v8::Function.
// Provide a wrapper for native bindings to call into.
function createUrlRequestEvent(eventName, opt_argSchemas, opt_extraArgSchemas,
                               opt_eventOptions, opt_webViewInstanceId) {
  return new UrlRequestEvent(eventName, opt_argSchemas, opt_extraArgSchemas,
                             opt_eventOptions, opt_webViewInstanceId);
}

utils.expose(UrlRequestEvent, UrlRequestEventImpl, {
  functions: [
    'hasListener',
    'hasListeners',
    'addListener',
    'removeListener',
    'addRules',
    'removeRules',
    'getRules',
  ],
});

exports.$set('UrlRequestEvent', UrlRequestEvent);
exports.$set('createUrlRequestEvent', createUrlRequestEvent);
