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


def parse_hex_color(hex_str: str) -> tuple[int, int, int]:
	"""Parse a hex color string (e.g. '#FF00FF' or 'FF00FF') into an (R, G, B) tuple."""
	hex_str = hex_str.lstrip("#")
	if len(hex_str) != 6:
		raise argparse.ArgumentTypeError(
			f"Invalid hex color '{hex_str}': must be a 6-digit hex value (e.g. FF00FF)"
		)
	try:
		r = int(hex_str[0:2], 16)
		g = int(hex_str[2:4], 16)
		b = int(hex_str[4:6], 16)
	except ValueError:
		raise argparse.ArgumentTypeError(
			f"Invalid hex color '{hex_str}': contains non-hex characters"
		)
	return (r, g, b)


def applyTransparentColor(imageObj: Image.Image, transparent_rgb: tuple[int, int, int]) -> Image.Image:
	"""
	Replace pixels matching transparent_rgb with fully transparent pixels (alpha=0).
	Converts the image to RGBA if it isn't already.
	"""
	if imageObj.mode != "RGBA":
		imageObj = imageObj.convert("RGBA")

	data: ndarray = numpy.asarray(imageObj, "B").copy()
	r, g, b = transparent_rgb

	# Build a mask of pixels that match the target color (ignoring existing alpha)
	mask = (data[:, :, 0] == r) & (data[:, :, 1] == g) & (data[:, :, 2] == b)

	# Set those pixels to fully transparent
	data[mask, 3] = 0

	return Image.fromarray(data, "RGBA")


def applyTransparentColorIndexed(imageObj: Image.Image, transparent_rgb: tuple[int, int, int]) -> Image.Image:
	"""
	For indexed (mode P) images, find palette entries matching transparent_rgb
	and set their alpha to 0 in the palette. Also ensures the image uses RGBA palette mode.
	"""
	palette_mode = imageObj.palette.mode
	color_depth  = {"RGB": 3, "RGBA": 4}[palette_mode]

	clut: ndarray = numpy.frombuffer(imageObj.palette.palette, "B").copy()
	num_colors    = clut.shape[0] // color_depth
	clut          = clut.reshape((num_colors, color_depth))

	r, g, b = transparent_rgb

	if color_depth == 3:
		# Expand palette to RGBA so we can set alpha
		clut_rgba         = numpy.ones((num_colors, 4), "B") * 0xff
		clut_rgba[:, :3]  = clut
		clut              = clut_rgba
		color_depth       = 4

	# Zero out alpha for any palette entry matching the target color
	mask = (clut[:, 0] == r) & (clut[:, 1] == g) & (clut[:, 2] == b)
	if not mask.any():
		print(
			f"Warning: transparent color #{r:02X}{g:02X}{b:02X} not found in palette — "
			"no changes made."
		)

	clut[mask, 3] = 0

	new_image             = imageObj.copy()
	new_image.palette.mode    = "RGBA"
	new_image.palette.palette = clut.tobytes()
	return new_image


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
	parser.add_argument(
		"--transparent",
		metavar="RRGGBB",
		help=(
			"Treat this hex color as fully transparent (e.g. FF00FF or #FF00FF). "
			"For indexed images the matching palette entry's alpha is zeroed; "
			"for RGB/RGBA images all matching pixels are set to alpha=0."
		),
		default=None
	)

	return parser.parse_args()

def main():
	args = parse_args()

	# Parse transparent color early so bad input fails before any file I/O
	transparent_rgb: tuple[int, int, int] | None = None
	if args.transparent:
		try:
			transparent_rgb = parse_hex_color(args.transparent)
			r, g, b = transparent_rgb
			print(f"Transparent color set to #{r:02X}{g:02X}{b:02X}")
		except argparse.ArgumentTypeError as e:
			print(f"Error: {e}")
			sys.exit(1)

	try:
		image_obj = Image.open(args.input_image)
	except Exception as e:
		print(f"Error opening image: {e}")
		sys.exit(1)

	# Quantize the image if needed
	if args.quantize:
		print(f"Quantizing image to {args.quantize} colors...")
		image_obj = toIndexedImage(image_obj, args.quantize)

	# Apply transparent color substitution
	if transparent_rgb is not None:
		print("Applying transparent color...")
		if image_obj.mode == "P":
			image_obj = applyTransparentColorIndexed(image_obj, transparent_rgb)
		else:
			image_obj = applyTransparentColor(image_obj, transparent_rgb)

	# Generate TIM based on image mode
	print("Generating TIM...")
	if image_obj.mode == "P":
		tim_data = generateIndexedTIM(image_obj, args.xcoord, args.ycoord, args.clut_x, args.clut_y, args.force_stp)
	else:
		tim_data = generateRawTIM(image_obj, args.xcoord, args.ycoord, args.force_stp)

	# Define output file
	output_file = args.output if args.output else f"{args.input_image.rsplit('.', 1)[0]}.tim"

	try:
		with open(output_file, "wb") as f:
			f.write(tim_data)
		print(f"TIM file successfully saved to {output_file}")
	except Exception as e:
		print(f"Error saving TIM file: {e}")
		sys.exit(1)

if __name__ == "__main__":
	main()
	