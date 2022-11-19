// Copyright 2022 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* All the work we do onload. */
function onLoadWork() {
  console.log("IT'S SHOW TIME!!!");
  console.log(`WE ARE ABOUT TO FETCH CONTENT WITH HASH ${document.location.hostname}:${document.location.pathname.slice(1)}`);
}

document.addEventListener('DOMContentLoaded', onLoadWork);
