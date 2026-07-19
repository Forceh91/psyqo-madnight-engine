# Madnight Engine (psyqo)

TLDR: Use the engine to make a game however you want, I don't care. It can be free or commerical, either is fine. All that you need to do is open source any changes that you make to the engine, as outlined in the [LICENSE](./LICENSE) in the file.

## Disclaimer

This engine is **far from finished** to the point that **I don't even know if you can actually make a game using it yet**. It's in like the pre-pre-pre-pre-pre alpha stage or something.

I wanted to open-source this now as there is zero advantage for both myself or Madnight Games to keep this closed source. Pull requests with fixes and improvements are more than welcome!

## Getting Started

This engine is just a library that links into your game code, so you'll create a new git repository with the engine (and its `nugget` submodule) added as a submodule, then scaffold your game code on top of it.

For the full walkthrough — toolchain setup, adding the submodule, scaffolding your game, building, and running it in an emulator — see [`getting-started/README.md`](./getting-started/README.md).

### Asset Usage

If you wish to dynamically load assets rather than having them hardcoded as header files or whatever then you will need to take a look at pcsx-redux's [authoring tool](https://github.com/grumpycoders/pcsx-redux/tree/main/tools/authoring).

In the `cdrom` directory you will see an example [toc.json](./cdrom/toc.json) that you can pass into the authoring tool to generate your ISO.

## Credits

Huge thank you to the following for their involvement in getting this engine started and understanding so much about psyqo; along with the PS1 and its hardware.

- [PSX.Dev Discord](https://discord.gg/QByKPpH)
- [Nicolas Noble](https://github.com/nicolasnoble)
