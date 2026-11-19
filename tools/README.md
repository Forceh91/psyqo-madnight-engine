# Using the tools

## Meshbins

For creating Meshbins you need to export out of Blender using the [./blender_obj_skeleton_exporter.py] script, and then you can use [./obj-to-meshbin.py] to convert that into an actual meshbin file format.

You need to provide the input obj file, the output destination file, and the texture size

```
python3 madnight_engine/tools/obj-to-meshbin.py ../assets/Lake\ Dock/map.obj cdrom/assets/map.meshbin 128
```

### Splitting level meshes

The engine lights each object once, from the middle of its bounding sphere, so one big level mesh is lit the same all over. [./blender_split_mesh.py] splits meshes into square chunks on the ground plane, and each chunk into its floor, its ceiling and its walls, so each piece is lit from its own side. It also keeps every piece under the mesh manager's face limit, so it doubles as level chunking.

Install it as an add-on, select the meshes to split, or select nothing to split the whole scene, then run Object > Split Meshes for Madnight. A chunk size of 0 picks one from the size of the meshes. The pieces go into a `<object>_split` collection and the original is hidden; running it again replaces them. It prints how many mesh slots the pieces leave free, and warns when that drops under 10.

A game object takes one texture, so faces with different materials always go in different pieces. For the fewest pieces give each material one texture, and keep each area of the level on as few materials as you can, ideally one atlas per area. It also runs headless:

```
blender -b level.blend --python madnight_engine/tools/blender_split_mesh.py -- --cell 4 --save level_split.blend
```

## Textures

Create a texture in GIMP and export as jpg/png (256x256 max), make sure its a square. After exporting use a tool like Imagemagick to conmvert it into something the PSX will understand.

You need to provide the input png file, the output png file, the depth, and the number of colours

```
convert ../assets/Lake\ Dock/atlas.png -depth 8 -colors 256 ../assets/Lake\ Dock/atlas_tim.png
```

Once its been converted you can then create a TIM file using [./tim_creator.py]. You should specify the input png file, the output TIM file, and number of colours (256 max)

```
python3 ./madnight_engine/tools/tim_creator.py -o cdrom/assets/map.tim --quantize 256 "../assets/Lake Dock/atlas_tim.png"
```

### Alternative to Python

If you don't have Python and/or ImageMagick installed then there is an alternative that you can use which is the **web version** of [TIM Tool](https://tools.psx.dev/timweb/). It allows you to import images, lay them out where you desire, and then export them all as .TIM files.

When using this version you can apply complete transparency to your image and it will automatically apply the right settings to the image to make it transparent in your game. Anything that is pure black will be replaced to a dark grey (0x0421), or colour of your choosing.

# Animations

Load an unskinned animation into Blender, and use the [./blender_animbin.py] script to export it to a very basic version of an ANIMBIN file. The script needs updating to allow for things like marker creation etc.

# World scale

The engine's convention is that 1 Blender unit is 2 m. So `1.0_ws` is 2 m, `0.5_ws` is 1 m, and `ONE_METRE` (`src/core/world_defs.hh`) is defined as `0.5_ws`.

Blender will show a 25 unit wall as 25 m, while the engine treats it as 50 m. To make Blender's displayed metres match, set the scene's Unit Scale to 2.0, or model at half size.
