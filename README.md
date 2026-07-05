# Madnight Engine (psyqo)

TLDR: Use the engine to make a game however you want, I don't care. It can be free or commerical, either is fine. All that you need to do is open source any changes that you make to the engine, as outlined in the [LICENSE](./LICENSE) in the file.

I suck at writing README files so this is probably missing a lot of important info about how to use scenes, game objects, camera, etc. etc. but take a gander around the engine code and it should be easy enough to figure out.. I hope.

## Disclaimer

This engine is **far from finished** to the point that **I don't even know if you can actually make a game using it yet**. It's in like the pre-pre-pre-pre-pre alpha stage or something.

I wanted to open-source this now as there is zero advantage for both myself or Madnight Games to keep this closed source. Pull requests with fixes and improvements are more than welcome!

## Getting Started

This engine is just a library that links into your game code and so you will need to create a new Git repository with the engine as a submodule, along with the nugget submodule that is inside of that.

### Asset Usage
If you wish to dynamically load assets rather than having them hardcoded as header files or whatever then you will need to take a look at pcsx-redux's [authoring tool](https://github.com/grumpycoders/pcsx-redux/tree/main/tools/authoring). 

In the `cdrom` directory you will see an example [toc.json](./cdrom/toc.json) that you can pass into the authoring tool to generate your ISO.

### Creating your game

Create a new directory to store your game code in and run the following:

```
git init
git submodule add https://github.com/Forceh91/psyqo-madnight-engine madnight_engine
git submodule update --init --recursive
```

With this completed you can then copy the contents of the `getting-started` folder (`Makefile` and the `src` folder) into your game code's directory.

Now you need to setup your development environment. This has been explained in great detail in the "The toolchain" section of [**psyqo Getting Started guide**](https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/psyqo/GETTING_STARTED.md#the-toolchain) so refer to that next.

Once you've got your toolchain setup and ready to go, you should be able to just run the following in your game code's directory.

```
make
```

If all goes well then this should generate a `madnight-engine-game.ps-exe` in a `build` folder, which you can then drop into your emulator of choice!

## Did it work?

With the build complete and the `ps-exe` running in your emulator then you should see a grey scene with a debug hud on top with some information about FPS and Heap Usage.

## Credits

Huge thank you to the following for their involvement in getting this engine started and understanding so much about psyqo; along with the PS1 and its hardware.

- [PSX.Dev Discord](https://discord.gg/QByKPpH)
- [Nicolas Noble](https://github.com/nicolasnoble)
