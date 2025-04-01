# ![Logo](../chrome/app/theme/chromium/product_logo_64.png) Ultimatum

A little help needed! I've abused my savings pretty much hard working on Ultimatum (it's temporary, I think) so if you like the project and want to help - here is my eth wallet [0x17E5DEB23d5a0ca1379d1d240cD9ba54EbEE4c63](https://etherscan.io/address/0x17E5DEB23d5a0ca1379d1d240cD9ba54EbEE4c63). Also I'm looking for short-term job, I'm good with chromium sources and webextensions, so if you can hook me up with that - it would be great help! Any help is much appreciated!


This is my attempt to port webextensions system on android. Something is already working, something still in progress.

### Tested extensions

- [uBlock Origin](https://addons.opera.com/en/extensions/details/ublock/)
- [uBlock Origin Lite](https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh)
- [Tampermonkey](https://chromewebstore.google.com/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo)
- [Violentmonkey](https://chromewebstore.google.com/detail/violentmonkey/jinjaccalgkegednnccohejagnlnfdag)
- [Browsec VPN](https://chromewebstore.google.com/detail/browsec-vpn-%D0%B1%D0%B5%D1%81%D0%BF%D0%BB%D0%B0%D1%82%D0%BD%D1%8B%D0%B9-%D0%B2%D0%BF/omghfjlpggmjjaagoclmmobgdodcjboh)
- [MetaMask](https://chromewebstore.google.com/detail/metamask/nkbihfbeogaeaoehlefnkodbefgpgknn)

- Reported by users:
  - [shipwr3ckd](https://github.com/shipwr3ckd)
    - [Midnight Lizard](https://chromewebstore.google.com/detail/midnight-lizard/pbnndmlekkboofhnbonilimejonapojg)
    - [ClearURLs](https://chromewebstore.google.com/detail/clearurls/lckanjgmijmafbedllaakclkaicjfmnk)
    - [BookmarkHub](https://chromewebstore.google.com/detail/bookmarkhub-%D1%81%D0%B8%D0%BD%D1%85%D1%80%D0%BE%D0%BD%D0%B8%D0%B7%D0%B0%D1%86%D0%B8%D1%8F/fohimdklhhcpcnpmmichieidclgfdmol)

If you find some extension working - let me know, I'll add it to the list.

Join us in telegram:
- [ultimatumBrowser](https://t.me/ultimatumBrowser)
- [ultimatumBrowserGroup](https://t.me/ultimatumBrowserGroup)

Mentions:

- (en) [popMODS](http://t.me/popMODS) popMODS is a Telegram channel dedicated to sharing open-source mobile and desktop applications, websites, and browser extensions, often hosting giveaways of paid apps. It also occasionnally shares memes, discussions, and ideas.
- (ru) [Open Source](https://t.me/open_source_friend) Open Source - a collection of useful programs and scripts with open source code.

### How to install webextensions?

Pretty much the same as you do on desktops. [Here](docs/ultimatum/webext_install/install.md) you can find instructions with pictures.

### How to build?

Basicaly the same way you build chromium but there is a couple tricks. First of all, before you do ``gn gen out/Default`` create ``out/Default`` manually (like ``mkdir out/Default``) and put this ``args.gn`` there:

```
# target_os = "android"
# target_cpu = "arm64"
is_debug = false
is_component_build = false
symbol_level = 0
blink_symbol_level = 0
v8_symbol_level = 0
ffmpeg_branding = "Chrome"
proprietary_codecs = true
# enable_desktop_android_extensions = true
# enable_guest_view = true
# enable_platform_apps = false
```

You can change symbols level as you wish, the point is to build for desktop first.

Run gen and then run build, [as usual](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md). After build is complete uncomment all commented lines in args.gn and run build again. In the end you'll get your apk to play with.

This step with building for Linux first instead of Android is nesessary to generate headers that are used (yet) in building but not generated when you build for Android. I'm working on it, there won't be any such nonsense in the final release .

You can learn how to play with apk [here](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_build_instructions.md#installing-and-running-chromium-on-a-device).

[Here](https://developer.chrome.com/docs/devtools/remote-debugging) you can find some handy tips about remote debugging.

### So, what's working?

Installation (webextensions) from opera and google stores, also you can install an extension from any site that gives the crx file with proper header (``"Content-Type": "application/x-chrome-extension"``). Installation for unpacked extensions ~~doesn't work yet but it's on the list~~ works as well. Installation fron .crx and .zip files is coming soon.

You can install, delete, turn off/on extensions, just like on desktops (doesn't mean that all of them will work properly).

Installation from Google Webstore is silent, if extension is downloaded from any other site there is notification about it and user can agree or cancel installation.

#### flaws

- there is no modal window when an extension tries to increase permissions (like Ublock lite). If you have installed extensions from one of the stores - it's ok. But be careful when installing them from other sites.

Below you can see list of apis and their statuses.

- ✅ means working (may be with differences)
- ❌ not working, not present (from js pov is undefined)
- ❌ ✅  is present (not undefined) but useless (either do nothing or next to nothing)

### full support

- ✅ chrome.cookies
- ✅ chrome.proxy
- ✅ chrome.storage
- ✅ chrome.contextMenus (except text select, coming soon)
- ✅ chrome.webRequest

#### private apis (are used in chromium underhood)

- ✅ chrome.developerPrivate
- ✅ chrome.guestViewInternal
- ✅ chrome.settingsPrivate
- ✅ chrome.webstorePrivate

#### chrome.tabs api

##### properties

- ✅ ``MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND``
- ✅ ``TAB_ID_NONE``
- ✅ ``TAB_INDEX_NONE``

##### methods

- ✅ captureVisibleTab
- ✅ connect
- ✅ create
- ✅ detectLanguage
- ❌ discard
- ❌ duplicate
- ✅ get
- ✅ getCurrent
- ❌ getZoom
- ❌ getZoomSettings
- ✅ goBack
- ✅ goForward
- ✅ group
- ❌ highlight (coming soon)
- ❌ move (coming soon)
- ✅ query
- ❌ reload
- ✅ remove
- ✅ sendMessage
- ❌ setZoom
- ❌ setZoomSettings
- ✅ ungroup
- ❌ update
- ✅ executeScript
- ✅ insertCSS
- ✅ removeCSS

##### events

- ✅ onActivated
- ❌ onAttached
- ✅ onCreated
- ❌ onDetached
- ❌ onHighlighted
- ✅ onMoved
- ✅ onRemoved
- ❌ onReplaced
- ✅ onUpdated
- ❌ onZoomChange

#### chrome.windows api

Be aware! On Android there is no such thing as window like we have on desktops. But there is incognito mode, so windows api behaves as if it was two windows - regular and incognito.

##### properties

- ✅ WINDOW_ID_CURRENT
- ✅ WINDOW_ID_NONE

#### Methods

- ❌ ✅ create (does nothing, just returns current window)
- ✅ get
- ✅ getAll
- ✅ getCurrent
- ✅ getLastFocused (returns current window)
- ✅ remove (can lead to crush if you try to close regular window)
- ❌ ✅ update (does nothing, just returns current window)

#### events

- ❌ onBoundsChanged
- ❌ onCreated
- ❌ onFocusChanged
- ❌ onRemoved

#### added but not tested yet

- chrome.history
- chrome.scripting
- chrome.declarativeNetRequest
- chrome.i18n
- chrome.idle
- chrome.metricsPrivate
- chrome.management
- chrome.offscreen
- chrome.runtime (messaging works though)

Enjoy!

#### Ultimatum - desktop

Ultimatum is a fork of the chromium browser with content addressing support. It aims to be a testing ground for experiments to build web3.0. In discussions about what web3.0 should be, the focus tends to shift to what the network should be like and little attention is paid to what the client should be like. This project (which I hope will eventually become part of another, larger project) attempts to fill the gap.

#### Why?

URLs are an integral part of the regular Internet and the main question that the url answers is - **where** is the content we need? This inevitably leads to centralization (one url - one location). In addition, users cannot be sure that they received exactly what they needed, for example a server can send different responses to different users for requests for the same URL and clients have no way to detect this situation.

Content addressing works differently. URN does not answer the question **where?** Instead, it describes exactly **what** content we need, in the hope that the answer to the question **where** will be found elsewhere. But at the same time, having received the content by URN, we can reliably verify that we received exactly **what** we requested. The issue of content localization does not go away; we still need to know where to look for it. But due to the fact that we know in advance some property of the content (which should be hard to fake), we can get it from anyone, including an unreliable sources, and still be sure that we received exactly what we wanted. On the other hand, if the property of the content is known in advance, it is not necessary to store it on one server with one domain. Any server supporting this scheme can serve content, and any client can check the response of any such server.

Thus, content addressing does not replace the usual one, but, firstly, complements it with reliability and, secondly, makes it decentralized due to the fact that the user can decide for himself where to look for content.

#### How it works?

Implementing content addressing support in Ultimatum is quite simple. The user specifies in the settings the addresses of the nodes to which requests will be addressed. Nodes (or agents) are ordinary web servers (in the future it will be possible to add support for other transport protocols, but for now I think it’s better to keep it simple) When the browser encounters an URN like ``hash://sha256/...``, it begins to sequentially poll agents from the list. If any agent owns content with the specified hash, it returns this content. The browser calculates the hash of the received content and if it matches the requested one, it accepts the answer, otherwise it continues the requesting. If the content is not found, the 404 response is returned.

Content addressing of this type has a flaw. As soon as the content changes, the hash changes, and therefore the URN. Let's consider a website or blog. Its start page will change every time its owner decides to publish a new article, news, or even just correct a typo found. This means that the hash of this page will also change each time. And each time the owner will need to somehow inform its users about the new URN. To solve this problem, support for the ``signed://`` scheme has been added. This is also a type of content addressing, but unlike the ``hash://`` scheme, the verified property of the content is not its hash, but the presence of a signature of this content with a certain key (which is known in advance and does not change). The site owner can announce to the world the presence of a page with an URN like ``signed://secp256r1.sha256/ae56f76d...78fa/index.html``, which is the start page of his site, where secp256r1 is the signature algorithm, sha256 is the signed hash algorithm, hexadecimal value followed - the public key, and /index.html - the message label, which in this case is part of the URN. Regardless of the number of updates, this URN will not change and can be used as an entry point to the site (the public key acts as something like a domain in terms of the regular web). Every time the page is updated, the site owner signs a new hash (of new version of the page). In order to distinguish which version is old and which is new, a nonce parameter is introduced, which plays the role of the content version.

The browser treats ``signed://`` requests a little differently than ``hash://`` ones. It also goes through the list of agents, but after receiving a response from any agent, it does not stop and continues to query everyone else on the list. All responses are checked for validity, including checking the signature, then the response with the largest nonce is selected and content is requested with the hash specified in the response.

In addition to ``hash://`` and ``signed://`` requests, there is also support for the ``related://`` scheme. Each signed message has an optional ``relatedTo`` field,  which can be any non-empty string, including url/urn. The ``related://`` request requests all signed messages with the ``relatedTo`` field having a specified value. The comment system is built on this, but this is not the only application of the scheme. Processing ``related://`` responses is not much different from processing ``signed://`` responses - the list of agents is also traversed to the end, messages are also checked for validity. But unlike ``hash://`` and ``signed://`` requests, a ``related://`` request cannot participate in navigation, it is only supported in fetch, moreover, a ``related://`` request can only be executed using the get method.

In general, this is enough to get an idea of what this fork is and does; as more detailed and more accurate documentation is ready, I will post links here.

#### get started as a user

[get compiled binary and start serfing](/docs/hash-net/get-started-for-users.md)


#### get started as a web master

coming soon, have a look at #Net utils and badger's layer code.

#### related resources

- [#Net agent](https://github.com/gonzazoid/hashnet-agent) reference backend implementation
- [#Net utils](https://github.com/gonzazoid/hashnet-utils) helps to prepare sites and upload them to #Net
[badger's lair](https://github.com/gonzazoid/badgers-lair) just example how #Net site might look like (with comments, emoji and everything) Available on #Net by address ``signed://secp256r1.sha256/03f702c0dd795a16a33feb25c9c09ba4885a08e24b8c6c1bd2c1201a0304f922fd/index.html``
- [hashnet-client](https://github.com/gonzazoid/hashnet-client) Collection of functions making #Net interactions easier (quite raw yet, sorry)

#### TODOS

- ``content/browser/renderer_host/render_frame_host_impl.cc`` look at git diff
