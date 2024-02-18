# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the web.

The project's web site is https://www.chromium.org.

To check out the source code locally, don't use `git clone`! Instead,
follow [the instructions on how to get the code](docs/get_the_code.md).

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory
Structure](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.

If you found a bug, please file it at https://crbug.com/new.

#### #Net

#Net chromium is a fork of the chromium browser with content addressing support. It aims to be a testing ground for experiments to build web3.0. In discussions about what web3.0 should be, the focus tends to shift to what the network should be like and little attention is paid to what the client should be like. This project (which I hope will eventually become part of another, larger project) attempts to fill the gap.

#### Why?

URLs are an integral part of the regular Internet and the main question that the url answers is - **where** is the content we need? This inevitably leads to centralization (one url - one location). In addition, users cannot be sure that they received exactly what they needed, for example a server can send different responses to different users for requests for the same URL and clients have no way to detect this situation.

Content addressing works differently. URN does not answer the question **where?** Instead, it describes exactly **what** content we need, in the hope that the answer to the question **where** will be found elsewhere. But at the same time, having received the content by URN, we can reliably verify that we received exactly **what** we requested. The issue of content localization does not go away; we still need to know where to look for it. But due to the fact that we know in advance some property of the content (which should be hard to fake), we can get it from anyone, including an unreliable sources, and still be sure that we received exactly what we wanted. On the other hand, if the property of the content is known in advance, it is not necessary to store it on one server with one domain. Any server supporting this scheme can serve content, and any client can check the response of any such server.

Thus, content addressing does not replace the usual one, but, firstly, complements it with reliability and, secondly, makes it decentralized due to the fact that the user can decide for himself where to look for content.

#### How it works?

Implementing content addressing support in #Net chromium is quite simple. The user specifies in the settings the addresses of the nodes to which requests will be addressed. Nodes (or agents) are ordinary web servers (in the future it will be possible to add support for other transport protocols, but for now I think it’s better to keep it simple) When the browser encounters an URN like ``hash://sha256/...``, it begins to sequentially poll agents from the list. If any agent owns content with the specified hash, it returns this content. The browser calculates the hash of the received content and if it matches the requested one, it accepts the answer, otherwise it continues the requesting. If the content is not found, the 404 response is returned.

Content addressing of this type has a flaw. As soon as the content changes, the hash changes, and therefore the URN. Let's consider a website or blog. Its start page will change every time its owner decides to publish a new article, news, or even just correct a typo found. This means that the hash of this page will also change each time. And each time the owner will need to somehow inform its users about the new URN. To solve this problem, support for the ``signed://`` scheme has been added. This is also a type of content addressing, but unlike the ``hash://`` scheme, the verified property of the content is not its hash, but the presence of a signature of this content with a certain key (which is known in advance and does not change). The site owner can announce to the world the presence of a page with an URN like ``signed://secp256r1.sha256/ae56f76d...78fa/index.html``, which is the start page of his site, where secp256r1 is the signature algorithm, sha256 is the signed hash algorithm, hexadecimal value followed - the public key, and /index.html - the message label, which in this case is part of the URN. Regardless of the number of updates, this URN will not change and can be used as an entry point to the site (the public key acts as something like a domain in terms of the regular web). Every time the page is updated, the site owner signs a new hash (of new version of the page). In order to distinguish which version is old and which is new, a nonce parameter is introduced, which plays the role of the content version.

The browser treats ``signed://`` requests a little differently than ``hash://`` ones. It also goes through the list of agents, but after receiving a response from any agent, it does not stop and continues to query everyone else on the list. All responses are checked for validity, including checking the signature, then the response with the largest nonce is selected and content is requested with the hash specified in the response.

In addition to ``hash://`` and ``signed://`` requests, there is also support for the ``related://`` scheme. Each signed message has an optional ``relatedTo`` field,  which can be any non-empty string, including url/urn. The ``related://`` request requests all signed messages with the ``relatedTo`` field having a specified value. The comment system is built on this, but this is not the only application of the scheme. Processing ``related://`` responses is not much different from processing ``signed://`` responses - the list of agents is also traversed to the end, messages are also checked for validity. But unlike ``hash://`` and ``signed://`` requests, a ``related://`` request cannot participate in navigation, it is only supported in fetch, moreover, a ``related://`` request can only be executed using the get method.

In general, this is enough to get an idea of what this fork is and does; as more detailed and more accurate documentation is ready, I will post links here.
