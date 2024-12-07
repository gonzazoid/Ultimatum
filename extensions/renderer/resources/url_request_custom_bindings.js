// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the urlRequest API.

if (!apiBridge) {
  var binding = require('binding').Binding.create('urlRequest');
  var urlRequestEvent = require('urlRequestEvent').UrlRequestEvent;
  binding.registerCustomEvent(urlRequestEvent);
  exports.$set('binding', binding.generate());
}
