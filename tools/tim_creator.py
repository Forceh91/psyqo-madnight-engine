from collections.abc import Generator, Mapping, Sequence
from dataclasses     import dataclass
from enum            import IntFlag
from struct          import Struct
from typing          import Any, Callable

import numpy
from numpy   import ndarray
from PIL     import Image

import argparse
import sys

# Your existing functions go here, including toIndexedImage, convertIndexedImage, etc.
## Simple image quantizer

# Pillow's built-in quantize() method will use different algorithms, some of
# which are broken, depending on whether the input image has an alpha channel.
# As a workaround, non-indexed-color images are "quantized" (with no dithering -
# images with more colors than allowed are rejected) manually instead.
def toIndexedImage(imageObj: Image.Image, numColors: int) -> Image.Image:
	if imageObj.mode == "P":
		return imageObj
	if imageObj.mode not in ( "RGB", "RGBA" ):
		imageObj = imageObj.convert("RGBA")

	image: ndarray = numpy.asarray(imageObj, "B")
	clut, image    = numpy.unique(
		image.reshape(( imageObj.width * imageObj.height, image.shape[2] )),
		return_inverse = True,
		axis           = 0
	)

	if clut.shape[0] > numColors:
		raise RuntimeError(
			f"source image contains {clut.shape[0]} unique colors (must be "
			f"{numColors} or less)"
		)

	image = image.astype("B").reshape(( imageObj.height, imageObj.width ))

	newImageObj: Image.Image = Image.fromarray(image, "P")

	newImageObj.putpalette(clut, imageObj.mode)
	return newImageObj

## .TIM image converter

class TIMHeaderFlag(IntFlag):
	COLOR_BITMASK = 3 << 0
	COLOR_4BPP    = 0 << 0
	COLOR_8BPP    = 1 << 0
	COLOR_16BPP   = 2 << 0
	HAS_PALETTE   = 1 << 3

_TIM_HEADER_STRUCT:  Struct = Struct("< 2I")
_TIM_SECTION_STRUCT: Struct = Struct("< I 4H")
_TIM_HEADER_VERSION: int    = 0x10

_LOWER_ALPHA_BOUND: int = 0x20
_UPPER_ALPHA_BOUND: int = 0xe0

# Color 0x0000 is interpreted by the PS1 GPU as fully transparent, so black
# pixels must be changed to dark gray to prevent them from becoming transparent.
_TRANSPARENT_COLOR: int = 0x0000
_BLACK_COLOR:       int = 0x0421

def _to16bpp(inputData: ndarray, forceSTP: bool) -> ndarray:
	source: ndarray = inputData.astype("<H")
	r:      ndarray = ((source[:, :, 0] * 249) + 1014) >> 11
	g:      ndarray = ((source[:, :, 1] * 249) + 1014) >> 11
	b:      ndarray = ((source[:, :, 2] * 249) + 1014) >> 11

	solid:           ndarray = r | (g << 5) | (b << 10)
	semitransparent: ndarray = solid | (1 << 15)

	data: ndarray = numpy.full_like(solid, _TRANSPARENT_COLOR)

	if source.shape[2] == 4:
		alpha: ndarray = source[:, :, 3]
	else:
		alpha: ndarray = numpy.full(source.shape[:-1], 0xff, "B")

	numpy.copyto(data, semitransparent, where = (alpha > _LOWER_ALPHA_BOUND))

	if not forceSTP:
		numpy.copyto(data, solid, where = (alpha > _UPPER_ALPHA_BOUND))
		numpy.copyto(
			data,
			_BLACK_COLOR,
			where = (alpha > _UPPER_ALPHA_BOUND) & (solid == _TRANSPARENT_COLOR)
		)

	return data

def convertIndexedImage(
	imageObj: Image.Image,
	forceSTP: bool = False
) -> tuple[ndarray, ndarray]:
	# Pillow has basically no support at all for palette manipulation, so nasty
	# hacks are required here.
	colorDepth: int = {
		"RGB":  3,
		"RGBA": 4
	}[imageObj.palette.mode]

	clut:      ndarray = numpy.frombuffer(imageObj.palette.palette, "B")
	numColors: int     = clut.shape[0] // colorDepth

	# Pad the palette to 16 or 256 colors after converting it to 16bpp.
	clut           = clut.reshape(( 1, numColors, colorDepth ))
	clut           = _to16bpp(clut, forceSTP)
	padAmount: int = (16 if (numColors <= 16) else 256) - numColors

	if padAmount:
		clut = numpy.c_[ clut, numpy.zeros(( 1, padAmount ), "<H") ]

	image: ndarray = numpy.asarray(imageObj, "B")

	if image.shape[1] % 2:
		image = numpy.c_[ image, numpy.zeros(( imageObj.height, 1 ), "B") ]

	# Pack two pixels into each byte for 4bpp images.
	if numColors <= 16:
		image = image[:, 0::2] | (image[:, 1::2] << 4)

		if image.shape[1] % 2:
			image = numpy.c_[ image, numpy.zeros(( imageObj.height, 1 ), "B") ]

	return image, clut

def generateRawTIM(
	imageObj: Image.Image,
	imageX:   int,
	imageY:   int,
	forceSTP: bool = False
) -> bytearray:
	if (imageX < 0) or (imageX > 1023) or (imageY < 0) or (imageY > 1023):
		raise ValueError("image X/Y coordinates must be in 0-1023 range")

	image: ndarray = numpy.asarray(imageObj, "B")
	image          = _to16bpp(image, forceSTP)

	data: bytearray = bytearray()
	data           += _TIM_HEADER_STRUCT.pack(
		_TIM_HEADER_VERSION,
		TIMHeaderFlag.COLOR_16BPP
	)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + image.size,
		imageX,
		imageY,
		image.shape[1] // 2,
		image.shape[0]
	)
	data.extend(image)

	return data

def generateIndexedTIM(
	imageObj: Image.Image,
	imageX:   int,
	imageY:   int,
	clutX:    int,
	clutY:    int,
	forceSTP: bool = False
) -> bytearray:
	if (imageX < 0) or (imageX > 1023) or (imageY < 0) or (imageY > 1023):
		raise ValueError("image X/Y coordinates must be in 0-1023 range")
	if (clutX < 0) or (clutX > 1023) or (clutY < 0) or (clutY > 1023):
		raise ValueError("palette X/Y coordinates must be in 0-1023 range")

	image, clut          = convertIndexedImage(imageObj, forceSTP)
	flags: TIMHeaderFlag = TIMHeaderFlag.HAS_PALETTE

	if clut.size <= 16:
		flags |= TIMHeaderFlag.COLOR_4BPP
	else:
		flags |= TIMHeaderFlag.COLOR_8BPP

	data: bytearray = bytearray()
	data           += _TIM_HEADER_STRUCT.pack(_TIM_HEADER_VERSION, flags)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + clut.size * 2,
		clutX,
		clutY,
		clut.shape[1],
		clut.shape[0]
	)
	data.extend(clut)

	data += _TIM_SECTION_STRUCT.pack(
		_TIM_SECTION_STRUCT.size + image.size,
		imageX,
		imageY,
		image.shape[1] // 2,
		image.shape[0]
	)
	data.extend(image)

	return data

# Argument parsing function
def parse_args():
    parser = argparse.ArgumentParser(description="Convert images to TIM format for PS1.")
    
    # Required arguments
    parser.add_argument("input_image", help="Path to the input image file")
    
    # Optional arguments
    parser.add_argument("-o", "--output", help="Output TIM file name (default: input_image.tim)", default=None)
    parser.add_argument("-x", "--xcoord", type=int, help="X coordinate for image placement (0-1023)", default=0)
    parser.add_argument("-y", "--ycoord", type=int, help="Y coordinate for image placement (0-1023)", default=0)
    parser.add_argument("-c", "--clut_x", type=int, help="X coordinate for CLUT placement (0-1023)", default=0)
    parser.add_argument("-d", "--clut_y", type=int, help="Y coordinate for CLUT placement (0-1023)", default=0)
    parser.add_argument("--force_stp", action="store_true", help="Force STP (semi-transparent) color processing")
    parser.add_argument("--quantize", type=int, help="Quantize image to a specified number of colors (e.g. 16, 256)", default=None)

    return parser.parse_args()

def main():
    args = parse_args()
    
    try:
        # Load the image
        image_obj = Image.open(args.input_image)
    except Exception as e:
        print(f"Error opening image: {e}")
        sys.exit(1)

    # Quantize the image if needed
    if args.quantize:
        print(f"Quantizing image to {args.quantize} colors...")
        image_obj = toIndexedImage(image_obj, args.quantize)
    
    # Generate TIM based on user input
    print("Generating TIM...")
    if image_obj.mode == "P":  # Indexed color image
        tim_data = generateIndexedTIM(image_obj, args.xcoord, args.ycoord, args.clut_x, args.clut_y, args.force_stp)
    else:  # RGB or RGBA image
        tim_data = generateRawTIM(image_obj, args.xcoord, args.ycoord, args.force_stp)

    # Define output file
    output_file = args.output if args.output else f"{args.input_image.rsplit('.', 1)[0]}.tim"
    
    # Write the TIM data to the output file
    try:
        with open(output_file, "wb") as f:
            f.write(tim_data)
        print(f"TIM file successfully saved to {output_file}")
    except Exception as e:
        print(f"Error saving TIM file: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
