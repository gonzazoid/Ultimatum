# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Ultimatum

This is my attempt to port webextensions system on android. Something is already working, something still in progress. Desktop related readme you can find [here](https://github.com/gonzazoid/Ultimatum/tree/ultimatum_132.0.6834.46)

### Tested extensions

- [Browsec VPN](https://chromewebstore.google.com/detail/browsec-vpn-%D0%B1%D0%B5%D1%81%D0%BF%D0%BB%D0%B0%D1%82%D0%BD%D1%8B%D0%B9-%D0%B2%D0%BF/omghfjlpggmjjaagoclmmobgdodcjboh)
- [uBlock Origin Lite](https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh)

If you find some extension working - let me know, I'll add it to the list.

### How to install webextensions?

Pretty much the same as you do on desktops. [Here](docs/ultimatum/webext_install/install.md) you can find instruction with pictures.

### How to build.

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

Installation (webextensions) from opera and google stores, also you can install an extension from any site that gives the crx file with proper header (``"Content-Type": "application/x-chrome-extension"``). Installation for unpacked extensions ~~doesn't work yet but it's on the list~~ works as well.

You can install, delete, turn off/on extensions, just like on desktops (doesn't mean that all of them will work properly).

#### flaws

- there is no modal window when an extension tries to increase permissions (like Ublock lite). If you have installed extensions from one of the stores - it's ok. But be careful when installing them from other sites.


Below you can see list of apis and their statuses.

- ✅ means working (may be with differences)
- ❌ not working, not present (from js pov is undefined)
- ❌ ✅  is present (not undefined) but useless (either do nothing or next to nothing)

### full support

- ✅ chrome.cookies
- ✅ chrome.scripting (not tested fully yet)
- ✅ chrome.proxy
- ✅ chrome.storage

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
- ❌ goBack (coming soon)
- ❌ goForward (coming soon)
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

##### events

- ✅ onActivated
- ❌ onAttached
- ✅ onCreated
- ❌ onDetached
- ❌ onHighlighted
- ✅ onMoved
- ✅ onRemoved
- ❌ onReplaced
- ❌ onUpdated
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

#### chrome.contextMenus api

Completely present and completely useless for now (I'm gonna change this)

#### added but not tested yet

- chrome.declarativeNetRequest
- chrome.i18n
- chrome.idle
- chrome.metricsPrivate
- chrome.management
- chrome.offscreen
- chrome.runtime (messaging working though)
- chrome.webRequest
- chrome.scripting

Enjoy!
