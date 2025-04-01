# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Ultimatum

This is my attempt to port webextensions system on android. Something is already working, something still in progress.

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

This step with building for Linux first instead of Android is nesessary to generate headers that are used (yet) in building but not generates when you build for Android. I'm working on it, there won't be any such nonsense in the final release .

You can learn how to play with apk [here](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_build_instructions.md#installing-and-running-chromium-on-a-device).

[Here](https://developer.chrome.com/docs/devtools/remote-debugging) you can find some handy tips about remote debugging.

### So, what's working?

Installation (webextensions) from opera and google stores, also you can install an extension from any site that gives the crx file with proper header (``"Content-Type": "application/x-chrome-extension"``). Installation for unpacked extensions doesn't work yet but it's on the list.

You can install, delete, turn off/on extensions, just like on desktops (doesn't mean that all of them will work properly).

Below you can see list of apis and their statuses.

- ✅ means working (may be with differences)
- ❌ not working, not present (from js pov is undefined)
- ❌ ✅  is present (not undefined) but useless (either do nothing or next to nothing)

#### chrome.tabs api

Be aware! On Android there is no such thing as window like we have on desktops. But there is incognito mode, so windows api behaves as if it was two windows - regular and incognito.

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

### full support
- ✅ chrome.cookies api
- ✅ chrome.scripting api (not tested fully yet)
- ✅ chrome.proxy api

#### private apis (are used in chromium underhood)
- ✅ chrome.developerPrivate
- ✅ chrome.webstorePrivate
- ✅ chrome.guestViewInternal

#### added but not tested yet

- chrome.declarativeNetRequest
- chrome.i18n
- chrome.idle
- chrome.metricsPrivate
- chrome.management
- chrome.offscreen
- chrome.runtime (messaging working though)
- chrome.storage (seems to be fully working but still need more thorough tests)
- chrome.webRequest

Enjoy!
