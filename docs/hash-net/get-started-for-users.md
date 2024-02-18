#### Download

You can download the compiled binary (for now only for macOS) here http://172.86.96.172/downloads/hash-net.dmg (it's http link, not https, so chrome can block downloading, just do right click on the link and select Save Link As)

sha256 checksum f918c432e7535780de2c610a52bbbd7b0b0b421be357db91f70fa6bb7e63c4e5

Instructions for assembling from source will be posted later.

#### Initial setup #Net chromium

Generate and add your private key in the settings. It looks like: ``secp256r1.sha256:335e6e0c87921c72fd2c8c46b753f80ed0a28ae47cfaa33bf0feab4a06cfe495``.

``secp256r1`` is the only supported algorithm at the moment (in the previous build there was support for ``secp256k1``, I had added it to boringSSL, but it didn't survive the update, in boringSSL they focused on optimizing Montgomery curves, and ``secp256k1`` is not a Montgomery curve so supporting it became a little difficult, but I’ll come back to this later).

The supported hash functions are ``sha1`` (because of torrents and git), ``sha256`` and ``sha512``.

The private key value itself can be generated here https://kjur.github.io/jsrsasign/sample/sample-ecdsa.html

After that, enter the key value into the settings (#Net tab, you'll see it) and save.

Now you need to add a list of active #Net agents.

At the moment, only one node is up at the address 172.86.96.172, so the agent URL template looks like this: ``http://172.86.96.172/{{request}}/{{function}}/{{path}}``.

Add this line to the list of agents in settings and save.

If you suddenly decide to launch your own agent (here is the repo https://github.com/gonzazoid/hashnet-agent), then change address in the line above and just add it to the list (line break is the separator).

This completes the #Net chromium setup, you can start surfing now. Just enter some #Net address in the address bar, for example:

```
signed://secp256r1.sha256/03f702c0dd795a16a33feb25c9c09ba4885a08e24b8c6c1bd2c1201a0304f922fd/index.html
```

Enjoy!!!
