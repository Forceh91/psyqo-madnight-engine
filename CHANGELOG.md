# Changelog

## [1.0.1](https://github.com/Forceh91/psyqo-madnight-engine/compare/v1.0.0...v1.0.1) (2026-11-13)


### Bug Fixes

* **renderer:** fog 2D particles with advanced fog on ([#154](https://github.com/Forceh91/psyqo-madnight-engine/issues/154)) ([7991f6b](https://github.com/Forceh91/psyqo-madnight-engine/commit/7991f6b7bb63ccc872bd57a69733dd48b030af3e))
* **renderer:** set fog distances when fog is turned on after StartScene ([#155](https://github.com/Forceh91/psyqo-madnight-engine/issues/155)) ([88e09dd](https://github.com/Forceh91/psyqo-madnight-engine/commit/88e09dd7b38e3543705cd5ea16a4430deedc0d88))

## [1.0.0](https://github.com/Forceh91/psyqo-madnight-engine/compare/v0.1.0...v1.0.0) (2026-11-10)


### ⚠ BREAKING CHANGES

* **renderer:** removed `NEAR_FOG_DISTANCE` and `FULL_FOG_DISTANCE` from `renderer.hh`
* **renderer:** applying camera position twice when rendering ([#121](https://github.com/Forceh91/psyqo-madnight-engine/issues/121))

### Features

* deprecate `atan2_fixed` in favour of `psyqo::atan2` ([#79](https://github.com/Forceh91/psyqo-madnight-engine/issues/79)) ([3f68c7e](https://github.com/Forceh91/psyqo-madnight-engine/commit/3f68c7e288959adfa3c7d975242202bae3be51ed))
* **memorycard:** add ability to just get the state of a card ([#130](https://github.com/Forceh91/psyqo-madnight-engine/issues/130)) ([88f682c](https://github.com/Forceh91/psyqo-madnight-engine/commit/88f682c56ba7e608b36a626827c38df4a63c0712))
* **memorycard:** add read/write calls so you don't have to care what a file system is ([#88](https://github.com/Forceh91/psyqo-madnight-engine/issues/88)) ([5d0bc89](https://github.com/Forceh91/psyqo-madnight-engine/commit/5d0bc89e5905943721f18585d1c024bd63800838))
* **memorycard:** adds some basic helper functions for mc access ([#84](https://github.com/Forceh91/psyqo-madnight-engine/issues/84)) ([5ce7a75](https://github.com/Forceh91/psyqo-madnight-engine/commit/5ce7a753f97e54f857fdd3d631cc94088902bf30))
* **particles:** allow for velocity spread, and particles inside radius ([#128](https://github.com/Forceh91/psyqo-madnight-engine/issues/128)) ([5853e8c](https://github.com/Forceh91/psyqo-madnight-engine/commit/5853e8cda558fee2a4e263d412f3a00931922eac))
* **renderer:** stop subdividing nodes that are small on screen ([#133](https://github.com/Forceh91/psyqo-madnight-engine/issues/133)) ([f09fdf8](https://github.com/Forceh91/psyqo-madnight-engine/commit/f09fdf8b519ab845ebf0deba652bda5777baba83))
* **tech-demo:** add a tech demo scene ([#109](https://github.com/Forceh91/psyqo-madnight-engine/issues/109)) ([cbc9892](https://github.com/Forceh91/psyqo-madnight-engine/commit/cbc9892c43e570636a49dadd4ab850745fc5d923))


### Bug Fixes

* `ONE_METRE` being the wrong scale (hopefully?) ([#92](https://github.com/Forceh91/psyqo-madnight-engine/issues/92)) ([362eebc](https://github.com/Forceh91/psyqo-madnight-engine/commit/362eebcc48ef44a297ad2621fb327b0f8edb4625))
* **animation:** recomputing offsets per vertex instead of once per bone ([#135](https://github.com/Forceh91/psyqo-madnight-engine/issues/135)) ([a805331](https://github.com/Forceh91/psyqo-madnight-engine/commit/a80533176f96745a3fa2fe16d26d51c3d92578ed))
* **bone/anim:** reading 2 bytes into 4 after the `Quaternion` change ([#113](https://github.com/Forceh91/psyqo-madnight-engine/issues/113)) ([0963ee8](https://github.com/Forceh91/psyqo-madnight-engine/commit/0963ee8e45c67ce42cbd14bfb640b3cbc3edad5f))
* **camera:** building a mirrored view for `LookAt` mode ([#123](https://github.com/Forceh91/psyqo-madnight-engine/issues/123)) ([5459da9](https://github.com/Forceh91/psyqo-madnight-engine/commit/5459da9c5ea8557ce303ad2136d704e941f8ced3))
* **gameobject:** not resetting flags on destroy ([#97](https://github.com/Forceh91/psyqo-madnight-engine/issues/97)) ([f9475ae](https://github.com/Forceh91/psyqo-madnight-engine/commit/f9475ae0c190760f622767f74dd1508d72c94ba0))
* **menu:** disabled menu items responding to input ([#82](https://github.com/Forceh91/psyqo-madnight-engine/issues/82)) ([8c9c559](https://github.com/Forceh91/psyqo-madnight-engine/commit/8c9c559aeba7a1649ec4afddee13fe8718c008cf))
* **mesh:** not advancing the `ptr` after loading the bounding sphere ([#118](https://github.com/Forceh91/psyqo-madnight-engine/issues/118)) ([fe3efa6](https://github.com/Forceh91/psyqo-madnight-engine/commit/fe3efa62269e16ae38f0f49ffe63cdd645780fb6))
* **modsound:** freeing data that is being used by `Mod_Load` ([#115](https://github.com/Forceh91/psyqo-madnight-engine/issues/115)) ([682c4bf](https://github.com/Forceh91/psyqo-madnight-engine/commit/682c4bf3db0edfbdaa82b8e0de1c0f9e20f06324))
* **modsound:** leaking a timer, not having a default volume ([#117](https://github.com/Forceh91/psyqo-madnight-engine/issues/117)) ([b8bc67a](https://github.com/Forceh91/psyqo-madnight-engine/commit/b8bc67a872dee1cdaa457bfd2fba823252082a85))
* **particles:** doing heavy operations per-frame ([#134](https://github.com/Forceh91/psyqo-madnight-engine/issues/134)) ([991279c](https://github.com/Forceh91/psyqo-madnight-engine/commit/991279c4152b1d6c9c12fcb3a6448d0a081627a0))
* **particles:** emitter not setting rotation matrix on `Init` ([#98](https://github.com/Forceh91/psyqo-madnight-engine/issues/98)) ([9844830](https://github.com/Forceh91/psyqo-madnight-engine/commit/9844830e85ddfe019c0d53a8fae923b34ee16223))
* **particles:** overflow on emitter spawn rate ([#127](https://github.com/Forceh91/psyqo-madnight-engine/issues/127)) ([d5998c5](https://github.com/Forceh91/psyqo-madnight-engine/commit/d5998c5348a321df65ba99c6be3499dac197c424))
* **particles:** slow particles not moving at short frame times, scratchpad layout ([#140](https://github.com/Forceh91/psyqo-madnight-engine/issues/140)) ([88ac228](https://github.com/Forceh91/psyqo-madnight-engine/commit/88ac228dde61993eece97341cc74d622c5dc586e))
* **particles:** using a sprite instead of rectangle when textured ([#119](https://github.com/Forceh91/psyqo-madnight-engine/issues/119)) ([afb537b](https://github.com/Forceh91/psyqo-madnight-engine/commit/afb537bf6c69a2c3f41373f20a3c0c5683dc7812))
* **raycast:** not taking into account object pos for meshed game objects ([#99](https://github.com/Forceh91/psyqo-madnight-engine/issues/99)) ([eb3d0f8](https://github.com/Forceh91/psyqo-madnight-engine/commit/eb3d0f84175e12e56dee3b2cc19f93741693b53c))
* **renderer:** applying camera position twice when rendering ([#121](https://github.com/Forceh91/psyqo-madnight-engine/issues/121)) ([c66bd9f](https://github.com/Forceh91/psyqo-madnight-engine/commit/c66bd9f8f061a9509876d69a3f5918895d52f20c))
* **renderer:** clobbering skinned mesh rotation when doing bone/vert math ([#120](https://github.com/Forceh91/psyqo-madnight-engine/issues/120)) ([14e318c](https://github.com/Forceh91/psyqo-madnight-engine/commit/14e318c350c541441db15b8b3a14b9f83b2270b4))
* **renderer:** improve the IsGameObjectVisible logic ([c2c7217](https://github.com/Forceh91/psyqo-madnight-engine/commit/c2c7217ba398106e986156b346168db108769eba))
* **renderer:** run the face loop on a RAM stack at -Os and -O0 ([#146](https://github.com/Forceh91/psyqo-madnight-engine/issues/146)) ([926a20f](https://github.com/Forceh91/psyqo-madnight-engine/commit/926a20fd4edd455700a4b1674919aff92c1f0250))
* **renderer:** wrapping ambient colours around when they were bright ([#90](https://github.com/Forceh91/psyqo-madnight-engine/issues/90)) ([5309571](https://github.com/Forceh91/psyqo-madnight-engine/commit/5309571bd69882a2c43c274297c4be54b5f23e9c))
* slerp, animation playback and `LerpAngle` cases left failing after [#75](https://github.com/Forceh91/psyqo-madnight-engine/issues/75) ([#86](https://github.com/Forceh91/psyqo-madnight-engine/issues/86)) ([f86be40](https://github.com/Forceh91/psyqo-madnight-engine/commit/f86be40e3a266b4cb624748d453b30fc1971a53a))
* **sound:** loading vag files into address 0 initially ([#116](https://github.com/Forceh91/psyqo-madnight-engine/issues/116)) ([e76df2d](https://github.com/Forceh91/psyqo-madnight-engine/commit/e76df2deeaab906c1e23d141d04c597a98b0e3ab))
* **tech-demo:** build at ReleaseFast by default ([#144](https://github.com/Forceh91/psyqo-madnight-engine/issues/144)) ([3755082](https://github.com/Forceh91/psyqo-madnight-engine/commit/37550821f54fa932871f6b234c169d03fc8f6399))


### Miscellaneous Chores

* **renderer:** removed `NEAR_FOG_DISTANCE` and `FULL_FOG_DISTANCE` from `renderer.hh` ([c2c7217](https://github.com/Forceh91/psyqo-madnight-engine/commit/c2c7217ba398106e986156b346168db108769eba))
