Hello, everyone! My name is Timur and I am a code addict.

Today I want to introduce my fork of chromium - Ultimatum. It's already capable of much to proudly bear its own name.

In general, all the features I've added can be divided into three sets:

- advanced features for webextensions
- features to bypass user tracking
- web3.0 support

I also slightly improved the webrequest api.

Let me walk you all through it.

Here you can find [all the commits](https://github.com/chromium/chromium/compare/129.0.6668.101...gonzazoid:chromium:ultimatum_129.0.6668.101) I'll cover in this article.

You can download the binaries from [my blog](https://gonzazoid.com) - OSX, Windows, Ubuntu.

### advanced features for webextensions

First, let's talk about web-extensions in general.

In order to install some extension, you either download it from the store (Google, Opera, Microsoft) - depending on which chromium-based browser you use . Or you can install the extension by pointing the browser to the directory in which it's located - but for this you need to enable developer mode. Or you can install the extension from third-party stores, but for this you need to do additional steps, here is a quote from the edge browser documentation:

```
 to allow users to install self-hosted extensions, you need to add the extension CRX IDs to the ExtensionInstallAllowList policy
 and add the URL of the location where the CRX file is hosted to the ExtensionInstallSources policy.
```

See the pattern? As soon as the extension is not distributed from the official website, everything becomes a little uncomfortable. I, for one, find this slightly disturbing. To change the order of things, I weaned Chromium from its bad habit of blocking third-party-distibuted extensions - now they can be installed from any website that serves a .crx file for downloading with the correct headers ("Content-Type": "application/x-chrome-extension"). There is no security flaws, it still warns the user what is happening and the user has the opportunity to confirm the action or refuse. And at the same time, extension developers get the opportunity to distribute their webextensions from their own websites without suffering from bureaucratic review processes in stores and regardless of the restrictions of the industry whales. Updates work as well and you can also host them yourself - here I have provided an example of a web-server that hosts both an extension and an update to it. 

You can also try installing the Pomogator extension from my blog and see how the procedure works, but before you do this, please read the article to the end - just to be sure that you understand what you are doing.

Here's a [commit](https://github.com/gonzazoid/chromium/commit/4295004b1d0fb511ff5871bf264fe43a8e4693a7) that does the trick.

Now lets talk about some standart api-s. 

To begin with, I suggest taking a look at these files (in the official chromium repository):
- https://github.com/chromium/chromium/blob/main/chrome/common/extensions/api/_api_features.json
- https://github.com/chromium/chromium/blob/main/chrome/common/extensions/api/_manifest_features.json
- https://github.com/chromium/chromium/blob/main/chrome/common/extensions/api/_permission_features.json
- https://github.com/chromium/chromium/blob/main/extensions/common/api/_api_features.json
- https://github.com/chromium/chromium/blob/main/extensions/common/api/_behavior_features.json
- https://github.com/chromium/chromium/blob/main/extensions/common/api/_manifest_features.json
- https://github.com/chromium/chromium/blob/main/extensions/common/api/_permission_features.json

What are we looking at exactly? There’s a lot of interesting stuff there, but let’s start with baby steps. Pay attention to the ``allowlist`` fields, for example for ``sockets`` in  extensions/common/api/_manifest_features.json. Sockets API is only available for web-apps and not for webextensions. But, as you can see, an exception was made for the Secure Shell extension and it can enjoy this api. If you skim through the files above, you can find many such APIs that are only accessible to a private club of exclusive extensions. Everything is clear with Google’s policy; it’s all pretty much self-explanatory. Another thing is interesting. If there is some (at least one) extension that uses this api, then this means that the api code is written in such a way that it can be accessed in extensions. Which means that presumably it is enough to remove allowlist and this api will be available for all extensions.

Well, the hypothesis was so promising and alluring that I just couldn't resist. Of course I did that and it worked!
So let me introduce: dns, sockets (sockets.udp, sockets.tcp, sockets.tcpServer) and fileSystem apis, available for all webextensions, ready to work for your pleasure! Here is an [example manifest](https://github.com/gonzazoid/extended_permissions_webext/blob/main/manifest.json) of an extension that is going to use them.

Lets step back for a moment to appreciate the gravity of the situation. With one [ugly-duckling commit](https://github.com/chromium/chromium/commit/c3f25bfc567a563e833d9e8ac7edeb8e83e174ea), we get capabilities comparable to those that electron gives us, but with the solution from electron coming with price - they pull node.js into the same bed with the browser. My solution on the other hand doesn't compromise the browser's virginity. It can be used as browser itself,  or, if you're perverted enough, you can even pull this fork into electron build instead of the regular chromuim code. I also intend to put some effort into making it available in CEF.

Yes, we still have limitations with the file api, I am aware and I intend to change this, and there is still a lot of work to be done, of course. But the gauntlet has already been thrown, and it's a matter of time before it's done. It's when, not if and I'm sure there will be people who will support the idea and take up the mantle.

That's it for quick and dirty solutions, but that's not the whole story. I'm just warming myself up and easing you all in. Buckle up, it's gonna be fun!

Now let's have a look at APIs I've added.

### features to bypass user tracking

First, let's talk briefly about user tracking. All user tracking options come down to:

- determine the user's platform/browser model/build (where you can reach from js - useragent, indirect fingerprints of hardware, platform, etc.)
- assign an id to the user and write it down in some secluded place.

There are also intermediate points, for example, the determination of a set of fonts can be considered both as a determination of an assembly/model, and at the same time - if the set of fonts is unique enough - it can be used as a user id. There are countless articles on this topic, google for help, lets skip this part, the article is already quite voluminous. For those who are not in the know but want to dig deeper, try googling supercookies and you'll get hooked, so help you God.

I decided to start by focusing on techniques for assigning an id to a user (in my understanding, this is exactly what tracking is)

In order to prevent tracking a user, in my opinion, it is enough to take control of all the places where the id can be recorded. And there are not so many such places in the browser after all:

- http cache for all tracking techniques checking whether a certain resource has been downloaded and cached
- hsts records (hsts pinning technique)
- favicons (because favicons have their own cache and are not written to the http cache)
- localStorages
- IndexedDB
- CacheStorage

Perhaps there is something else, I do not pretend to be complete, if you know other places - tell me, let's see what can be done with it.

So here it is. The main idea is that if we can take control of these places, then we can erase the data (which amounts to losing the track id) or remember it (followed by erasure) and restore it at the right time (which amounts to replacing the id)

It looked good and I decided to work in this direction. The result is below.

Attention! All api described below are strictly for webextensions and require non-standard permissions and resp. extensions written using these api cannot be published on stores (chrome, opera, edge) because from the point of view of stores they have an invalid manifest.

### diskCache

To access the API, you need to specify the ``diskCache`` permission in the extension manifest. After installation, an extension with this permission gains access to the api:

```
await chrome.diskCache.keys(cache_name); // returns an array of keys
await chrome.diskCache.getEntry(cache_name, key); // returns the specified cache entry
await chrome.diskCache.putEntry(cache_name, entry); // writes to the specified cache, key is specified in entry
await chrome.diskCache.deleteEntry(cache_name, key); // removes the specified entry
```

The cache entry has the following format:
```
{
  key: "string",
  stream0: ArrayBuffer,
  stream1: ArrayBuffer,
  stream2: ArrayBuffer,
  ranges: Array
}
// where ranges consists of objects:
{
  offset: number,
  length: number,
}
```
The ranges property is optional and is specified only for sparse entities. stream0, 1, 2 are required for everyone, but for sparse entities only stream0 and stream1 are used, while stream1 contains all the chunks following each other (without empty spaces) and ranges indicate where they (chunks) should have been located. That is, the length of stream1 must match the sum of all lengths specified in ranges. (This is all a reflection of the details of the implementation of disk_cache in Chromium, this is not my initiative)

You can see how disk_cache works [here](https://github.com/chromium/chromium/blob/main/net/disk_cache/README.md), but unfortunately the details are mostly scattered throughout the code and I couldn’t find any proper documentation. Someday I will get around to describing how it works.

cache_name can be ``http``, ``js_code``, ``wasm_code`` and ``webui_js_code``. So far I have only worked with ``http``, if you experiment with other caches, feel free to share the results.

So, http cache. Having access to it, we can pull out the entire cache, save it in some place, we can completely erase it, or we can write down what we need, for example, the cache from the previous session. I implemented all this in my Pomogator extension; in one of the following articles I will explain how to use this extension and what opportunities it provides.

What tracking techniques are we removing from this plane of existence with this api? From the list of techniques [evercookie](https://github.com/samyk/evercookie):

- Storing cookies in HTTP ETags (Backend server required)
- Storing cookies in Web cache (Backend server required)

But in general - any technique that is based on checking whether a resource has already been downloaded or not (with the exception of favicon - it has its own cache) - will slip and stall on these possibilities.


This API still has a small leak - the code does not handle parallel requests, so it is better to wait until the cache element is received and only then make the next request. The same goes with writing. I hope this is all temporary, I’m working on stabilizing the api and only this api has such a problem, all the others work stably and easily perform parallel requests.

### sqliteCache

Allows you to access the favicon cache and history cache (they are both implemented based on sqlite). History hasn’t been involved in tracking for quite a long time, but I decided to let it be. 
In order to gain access to the api, the extension must have permission ``sqlCache`` in its manifest.

The API is as follows:

```
await chrome.sqlCache.exec(storage, sql_request, bindings);
```

where:
- ``storage`` - string, specifies which database the request is sent to. Can be ``faviconCache`` or ``historyCache``. If you know any sqlite databases in the depths of chromium that you would like to look into - let me know, we’ll discuss.
-  ``sql_request`` - string, the sqlite query itself.
- ``bindings`` that's intresting. In the request itself, specific values ​​are not specified; instead, the wildcard character ``?`` is specified . And in bindings we indicate what should actually be substituted there. That is, bindings is an array of elements, each of which can be (js->c++):
  - ``string`` (literal, not object) - becomes ``sql::ColumnType::kText``
  - ``number`` - becomes ``sql::ColumnType::kFloat`` (in js numbers are floats and not integers, we remember that, right?)
  -  object with fields {
      type: "int",
      value: "string, decimal"
  } becomes ``sql::ColumnType::kInteger``. 
Such difficulties with integer are due to the fact that sqlite supports int up to 64 bits and, firstly, float (in js) does not support such precision, and secondly, if we start using js float (which is number) for kInteger, then we will still have to distinguish it from use for kFloat. It would be possible to adapt the js BigInt for this, but in fact it doesn’t make anything easier, so I left it like that.
  - ``ArrayBuffer`` - becomes ``sql::ColumnType::kBlob``
  - ``null`` - becomes ``sql::ColumnType::kNull``

This covers all types of sqlite, details can be found on [their website](https://www.sqlite.org/), the documentation is quite decent.

As a result of the request, we get an array in which each element displays one row of the result and is itself an array of elements. Each row element has one of the types specified above for bindings. That is something like:
```
[
  [ /* first row */ "string", 3.14, { type: "int", value: "73" } ],
  [ /* second row */ "yet another string", 2.718, { type: "int", value: "43" } ],
  ...
]
```

Why did we need to make a separate API for favIcons if there is an http cache? Well, thing is that chrome/chromium work with favicons "strangely". There is a separate cache for favicons, not http (there are a lot of articles on the network that mention that this cache cannot be reset, but this is no longer the case, when browsing data is deleted it is also deleted, I can’t say exactly since which version of chromium, 129th does that for sure). This cache is quite actively used to track users, for example in the supercookies. 

I will tell you in more detail how the favicon cache and history cache work in a separate article; for now this is just an overview of the API.

### hstsCache

At the moment, ``hsts pinning`` is the most impenetrable tracking technique (of which I know), so the need to multiply it by zero was obvious. Chromium provides a rather poor interface for working with hsts, available at ``chrome://net-internals/#hsts`` and the reasons for this poverty became clear when I gutted the code, this is described below.

The tracking technique itself is described in many places, there is a paper on the topic [HSTS Supports Targeted Surveillance](https://www.researchgate.net/publication/329402608_HSTS_Supports_Targeted_Surveillance).  It won't take long to figure it out if you want.

So, the problem is that Chromium does not provide any tools to see what domains are recorded in the hsts cache. That is, you can only look at it if you know the domain, but you won’t get a list of domains in any way. The fact is that chromium does not store the domains themselves; the key to the rule records is the hash from the domain. I'm still wondering if this is worth fixing, but for now I just implemented the standard interface for access. The api looks like this (available for extensions with permission hstsCache):

```
await chrome.hstsCache.keys(); // returns all available keys in hsts cache, each key is an ArrayBuffer
await chrome.hstsCache.getEntry(key); // returns the hstsCache entry with the specified key
await chrome.hstsCache.putEntry(entry); // writes the entry to the cache
await chrome.hstsCache.deleteEntry(key); // removes a cache entry with the specified key
```

Entry has the form:

```
{
  key, // ArrayBuffer(32),
  upgradeMode, // number,
  includeSubdomains, // boolean,
  expiry, // number-timestamp like Date.now()
  lastObserved, // number-timestamp like Date.now()
}
```

I won’t go into detail, those who are familiar with the hsts-pinning technique will understand how to use it, those who are not will have to figure it out in order to use it.

### localStorages

An extension with this permission gets access to all records in localStorage, regardless of origin and other things. That is, we can read/write/delete any record of any localStorage. API looks like this:

```
await chrome.localStorages.keys(); // returns an array of keys, each key is an arrayBuffer
await chrome.localStorages.getEntry(key); // returns the entry corresponding to the key, the result is arrayBuffer
await chrome.localStorages.putEntry(key, entry); // if the record exists, we change it, if not, we create it
await chrome.localStorages.deleteEntry(key); // delete the entry
await  chrome.localStorages.flush(); // explained below
await  chrome.localStorages.purgeMemory(); // explained below
```

The key is a buffer, if we translate it into a string we will get values ​​like this:

```
[
  "META:chrome://settings",
  "META:devtools://devtools",
  "META:https://habr.com",
  "METAACCESS:chrome://settings",
  "METAACCESS:devtools://devtools",
  "METAACCESS:https://habr.com",
  "VERSION",
  "_chrome://settings\u0000\u0001privacy-guide-promo-count",
  "_devtools://devtools\u0000\u0001console-history",
  "_devtools://devtools\u0000\u0001experiments",
  "_devtools://devtools\u0000\u0001localInspectorVersion",
  "_devtools://devtools\u0000\u0001previously-viewed-files",
  "_https://habr.com\u0000\u0001rom-session-start",
  "_https://www.google.com/^0https://stackoverflow.com\u0000\u0001rc::h",
  "_https://www.youtube.com/^0https://habr.com\u0000\u0001ytidb::LAST_RESULT_ENTRY_KEY"
]
```

We are interested in the keys with the prefix ``_http`` - they are the ones related to the web, but as we can see we have access to other interesting things here. I haven’t really researched this yet, if anyone digs deeper and finds something interesting, let me know.

The first 4 functions speak for themselves, there is nothing particularly new here, let's look at flush and purgeMemory memory. To begin with, here is a piece from the corresponding mojom file:

``components/services/storage/public/mojom/local_storage_control.mojom``

```
  // Tells the service to immediately commit any pending operations to disk.
  Flush();

  // Purges any in-memory caches to free up as much memory as possible. The
  // next access to the StorageArea will reload data from the backing database.
  PurgeMemory();
```

So, how does this work? There is a certain database that lies somewhere, no matter where and no matter how. During the surfing process, when displaying tabs and frames from this base, a selection is made and all the records for the corresponding origins are pulled out. (it’s a little more complicated actually, the origin of the current frame is taken and either the main frame or the parent frame, I can’t say for sure from memory) 
Next, all frames that need these records work with their copies in memory. And that's fine performance-wise. But! When we try to read records from the database, we don’t know how valid they are. Therefore, we do flush() and force all changes to be committed to the database. After this, we can READ and be sure that we are working with up-to-date data. All cached data also remains in their caches so tabs and frames do not suffer any performance hit.

Next. We read the data, made some decisions and decided to change something. We write these changes to the database. But at the same time, as we remember, already open tabs/frames have their own caches and they will not see these changes. That's why we do purgeMemory(). The caches are reset and the next request to localStorage of the domain will fetch records from the database - yes, along with our changes if these changes concerned this domain. That is, we do purgeMemory() after RECORDING to the database, and here some kind of performance drawdown is inevitable.

### web 3.0 support

I won’t go into too much detail here; the README of the repository has more detailed information with links. In a nutshell - Ultimatum supports hash://, signed:// and related:// schemes and based on this there is already code that allows you to create sites that can be hosted by any network participant, you can exchange messages and even set emodji. This is still an experiment, not production-ready, but at least you can play with it right now - usual front-end skills are applicable.

### urlRequest

This is really interesting. Remember how in 2019 Google announced that the webRequest api was not good anough and its support (in terms of blocking or more precisely blocked requests) would be discontinued? And then they did not transfer this api to the v3 manifest. And the people were seething, and the ad blockers fell off. And Google rolled out its decalrative network requests and people were seething even more. And then Opera (and it seems like Microsoft, but this is not certain) announced that they would support webrequests until the утв ща ешьу, but they remained in the v2 manifest. Right? I remember this too. And to be honest, I still don't understand what all the fuss was about.. I mean, well, Google wants to shoot itself in the foot - so it’s its foot, it has the right. They don’t force us to shoot at our own feet. In the end, I simply copied the webRequest api code, renamed it to urlRequest, raised it up to the third manifest and removed all the code (for now) related to events, leaving only onBeforeRequest. But I edited it a little to make it look prettier:
- all requests are intercepted, there are no protected domains (there was Google domains in chromium code, requests to which were not intercepted)
- in addition to cancel and redirect, you can return a Response object or a promise that will resolve to Response and in this case there will be no request to the network at all - the request initiator will be given the Response data.
- all requests that fall under the requestFilter are always blocked; if it turns out that the request is not interesting to us, we can return an empty response or cancel: false, in this case the request will go to the network without any intervention.

How does this work in code? The extension must have a permission ``urlRequest``, this is what attaching a listener looks like:

```
chrome.urlRequest.onBeforeRequest.addListener((evt) => {
    console.log(evt);
    if (evt.url !== "https://habr.com/") return { cancel: false };

    return {
      response: new Promise((resolve) => {
        resolve(
          new Response(
            "<html>haha</html>",
           { statusText: "OK", headers: { "Content-Type": "text/html; charset=utf-8"}}
          )
        );
    })
  };
}, {urls: ["https://habr.com/*"]})
```

That is, now you can write extensions that, in essence, can (including but not limited to) perform the functions of a web server. There are still restrictions on headers (they are built into the Response code) - I will not change this, but I will add the ability to create any response without restrictions.

What's next? I intend to return all other events from the webRequest api, removing restrictions along the way where it makes sense, (tcp and udp modules in tandem with intercepting requests make it possible to register and implement support for any protocols at the level of js extensions). Taking into account the fact that we already have access to the http cache - a significant part of servicing network requests can move to js - this will reduce the amount of C++ code in the project and provide greater opportunities for implementing new protocols, I think this may be of interest to many teams. 

After that, I'm going to rewrite web3.0 support and move most of the code to inline webextension, which will greatly simplify further code maintenance (because the less my code intersects with the original chromium code, the fewer conflicts there are during updates)

### In conclusion

The code is open and free, BSD license.

If you want to support the project, I have information on my blog, any help is appreciated!

That’s all for today, how to use these api and what plans for the development of the project will be in the following articles.

There is no more news for today, Timur was with you, have a good day!

PS. As you may have noticed noticed I'm not native, so feel free to correct me, I would appreciate that.
