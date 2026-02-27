# Using the tools


## Meshbins

For creating Meshbins you need to export out of Blender using the [./blender_obj_skeleton_exporter.py] script, and then you can use [./obj-to-meshbin.py] to convert that into an actual meshbin file format.

You need to provide the input obj file, the output destination file, and the texture size

```
python3 madnight_engine/tools/obj-to-meshbin.py ../assets/Lake\ Dock/map.obj cdrom/assets/map.meshbin 128
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

# Animations

Load an unskinned animation into Blender, and use the [./blender_animbin.py] script to export it to a very basic version of an ANIMBIN file. The script needs updating to allow for things like marker creation etc.