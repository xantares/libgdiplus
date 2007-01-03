/*
 * pngcodec.c : Contains function definitions for encoding decoding png images
 *
 * Copyright (C) Novell, Inc. 2003-2004.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
 * and associated documentation files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial 
 * portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *  	Sanjay Gupta (gsanjay@novell.com)
 *	Vladimir Vukicevic <vladimir@pobox.com>
 *      Jonathan Gilbert (logic@deltaq.org)
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBPNG

#include <png.h>
#include "general.h"
#include "pngcodec.h"


#include <setjmp.h>


/* Codecinfo related data*/
static ImageCodecInfo png_codec;
static const WCHAR png_codecname[] = {'B', 'u', 'i','l', 't', '-','i', 'n', ' ', 'P', 'N', 'G', 0}; /* Built-in PNG */
static const WCHAR png_extension[] = {'*', '.', 'P', 'N', 'G', 0}; /* *.PNG */
static const WCHAR png_mimetype[] = {'i', 'm', 'a','g', 'e', '/', 'p', 'n', 'g', 0}; /* image/png */
static const WCHAR png_format[] = {'P', 'N', 'G', 0}; /* PNG */


ImageCodecInfo *
gdip_getcodecinfo_png ()
{
        png_codec.Clsid = (CLSID) { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x0, 0x0, 0xf8, 0x1e, 0xf3, 0x2e } };
        png_codec.FormatID = gdip_png_image_format_guid;
        png_codec.CodecName = (const WCHAR*) png_codecname;
        png_codec.DllName = NULL;
        png_codec.FormatDescription = (const WCHAR*) png_format;
        png_codec.FilenameExtension = (const WCHAR*) png_extension;
        png_codec.MimeType = (const WCHAR*) png_mimetype;
        png_codec.Flags = Encoder | Decoder | SupportBitmap | Builtin;
        png_codec.Version = 1;
        png_codec.SigCount = 0;
        png_codec.SigSize = 0;
        png_codec.SigPattern = 0;
        png_codec.SigMask = 0;

        return &png_codec;
}

#if !defined(HAVE_SIGSETJMP) && !defined(sigsetjmp)
#define sigjmp_buf jmp_buf
#define sigsetjmp(jb, x) setjmp(jb)
#define siglongjmp longjmp
#endif

void
_gdip_png_stream_read_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	GetBytesDelegate	getBytesFunc;
	int			bytesRead;
	int			res;

	getBytesFunc = (GetBytesDelegate) png_get_io_ptr (png_ptr);

	/* In png parlance, it is an error to read less than length */
	bytesRead = 0;
	while (bytesRead != length) {
		res = getBytesFunc (data + bytesRead, length - bytesRead, 0);
		if (res <= 0) {
			png_error(png_ptr, "Read failed");
		}
		bytesRead += res;
	}
}

void
_gdip_png_stream_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
    PutBytesDelegate putBytesFunc = (PutBytesDelegate) png_get_io_ptr (png_ptr);
    putBytesFunc (data, length);
}

void
_gdip_png_stream_flush_data (png_structp png_ptr)
{
    /* nothing */
}

GpStatus
gdip_load_png_properties(png_structp png_ptr, png_infop info_ptr, png_infop end_ptr, BitmapData *bitmap_data)
{

#if defined(PNG_INCH_CONVERSIONS) && defined(PNG_FLOATING_POINT_SUPPORTED)
	bitmap_data->image_flags |= ImageFlagsHasRealDPI;
	bitmap_data->dpi_horz = png_get_x_pixels_per_inch(png_ptr, info_ptr);
	bitmap_data->dpi_vert = png_get_y_pixels_per_inch(png_ptr, info_ptr);
#endif

#if defined(PNG_iCCP_SUPPORTED)
	{
		png_charp	name;
		png_charp	profile;
		png_uint_32	proflen;
		int		compression_type;

		if (png_get_iCCP(png_ptr, info_ptr, &name, &compression_type, &profile, &proflen)) {
			gdip_bitmapdata_property_add_ASCII(bitmap_data, ICCProfileDescriptor, (unsigned char *)name);
			gdip_bitmapdata_property_add_byte(bitmap_data, ICCProfile, (byte)compression_type);
		}
	}
#endif

#if defined(PNG_gAMA_SUPPORTED)
	{
		double	gamma;

		if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
			gdip_bitmapdata_property_add_rational(bitmap_data, Gamma, 100000, gamma * 100000);
		}
	}
#endif

#if defined(PNG_cHRM_SUPPORTED)
	{
		double	white_x;
		double	white_y;
		double	red_x;
		double	red_y;
		double	green_x;
		double	green_y;
		double	blue_x;
		double	blue_y;

		if (png_get_cHRM(png_ptr, info_ptr, &white_x, &white_y, &red_x, &red_y, &green_x, &green_y, &blue_x, &blue_y)) {
			unsigned char	*buffer;
			guint32		*ptr;

			buffer = GdipAlloc(6 * (sizeof(png_uint_32) + sizeof(png_uint_32)));
			if (buffer != NULL)  {
				ptr = (guint32 *)buffer;

				ptr[0] = (guint32)(red_x * 100000);
				ptr[1] = 1000000;
				ptr[2] = (guint32)(red_y * 100000);
				ptr[3] = 100000;

				ptr[4] = (guint32)(green_x * 100000);
				ptr[5] = 1000000;
				ptr[6] = (guint32)(green_y * 100000);
				ptr[7] = 100000;

				ptr[8] = (guint32)(blue_x * 100000);
				ptr[9] = 100000;
				ptr[10] = (guint32)(blue_y * 100000);
				ptr[11] = 100000;

				gdip_bitmapdata_property_add(bitmap_data, PrimaryChromaticities, 6 * (sizeof(guint32) + sizeof(guint32)), TypeRational, buffer);

				ptr[0] = (guint32)(white_x * 100000);
				ptr[1] = 1000000;
				ptr[2] = (guint32)(white_y * 100000);
				ptr[3] = 100000;
				gdip_bitmapdata_property_add(bitmap_data, WhitePoint, 2 * (sizeof(guint32) + sizeof(guint32)), TypeRational, buffer);


				GdipFree(buffer);
			}
		}
	}
#endif

#if defined(PNG_pHYs_SUPPORTED)
	{
		int		unit_type;
		png_uint_32	res_x;
		png_uint_32	res_y;

		if (png_get_pHYs(png_ptr, info_ptr, &res_x, &res_y, &unit_type)) {
			gdip_bitmapdata_property_add_byte(bitmap_data, PixelUnit, (byte)unit_type);
			gdip_bitmapdata_property_add_long(bitmap_data, PixelPerUnitX, res_x);
			gdip_bitmapdata_property_add_long(bitmap_data, PixelPerUnitY, res_y);
		}
	}
#endif

#if defined(PNG_TEXT_SUPPORTED)
	{
		int		num_text;
		png_textp	text_ptr;

		if (png_get_text(png_ptr, info_ptr, &text_ptr, &num_text)) {
			if (num_text > 0) {
				gdip_bitmapdata_property_add_ASCII(bitmap_data, ExifUserComment, (unsigned char *)text_ptr[0].text);
			}
		}
	}
#endif

	return Ok;
}

GpStatus 
gdip_load_png_image_from_file_or_stream (FILE *fp, GetBytesDelegate getBytesFunc, GpImage **image)
{
	png_structp	png_ptr = NULL;
	png_infop	info_ptr = NULL;
	png_infop	end_info_ptr = NULL;
	guchar		*rawdata = NULL;
	GpImage		*result = NULL;

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) {
		goto error;
	}


	if (setjmp(png_jmpbuf(png_ptr))) {
		/* png detected error occured */
		goto error;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
		goto error;
	}

	end_info_ptr = png_create_info_struct (png_ptr);
	if (end_info_ptr == NULL) {
		goto error;
	}

	if (fp != NULL) {
		png_init_io (png_ptr, fp);
	} else {
		png_set_read_fn (png_ptr, getBytesFunc, _gdip_png_stream_read_data);
	}

	png_read_png(png_ptr, info_ptr, 0, NULL);

	if ((png_get_bit_depth (png_ptr, info_ptr) <= 8)
	 && (png_get_channels (png_ptr, info_ptr) == 1)
	 && ((png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE)
	 || (png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY))) {
		int		width;
		int		height;
		int		bit_depth;
		int		source_stride;
		int		dest_stride;
		png_bytep	*row_pointers;
		guchar		*rawptr;
		int		num_colours;
		int		palette_entries;
		ColorPalette	*palette;
		ImageFlags	colourspace_flag;
		int		i;
		int		j;

		width = png_get_image_width (png_ptr, info_ptr);
		height = png_get_image_height (png_ptr, info_ptr);
		bit_depth = png_get_bit_depth (png_ptr, info_ptr);

		source_stride = (width * bit_depth + 7) / 8;
		dest_stride = (source_stride + sizeof(pixman_bits_t) - 1) & ~(sizeof(pixman_bits_t) - 1);

		/* Copy image data. */
		row_pointers = png_get_rows (png_ptr, info_ptr);

		if (bit_depth == 2) { /* upsample to 4bpp */
			dest_stride = ((width + 1) / 2 + sizeof(pixman_bits_t) - 1) & ~(sizeof(pixman_bits_t) - 1);

			rawdata = GdipAlloc(dest_stride * height);
			for (i=0; i < height; i++) {
				png_bytep row = row_pointers[i];
				rawptr = rawdata + i * dest_stride;

				for (j=0; j < source_stride; j++) {
					int four_pixels = row[j];

					int first_two = 0x0F & (four_pixels >> 4);
					int second_two = 0x0F & four_pixels;

					first_two = (first_two & 0x03) | ((first_two & 0x0C) << 2);
					second_two = (second_two & 0x03) | ((second_two & 0x0C) << 2);

					rawptr[j * 2 + 0] = first_two;
					rawptr[j * 2 + 1] = second_two;
				}
			}
		} else {
			rawdata = GdipAlloc(dest_stride * height);
			for (i=0; i < height; i++) {
				memcpy(rawdata + i * dest_stride, row_pointers[i], source_stride);
			}
		}

		/* Copy palette. */
		num_colours = 1 << bit_depth;
		if (bit_depth == 4) {
			num_colours = 256;
		}

		palette = GdipAlloc (sizeof(ColorPalette) + num_colours * sizeof(ARGB));

		palette->Flags = 0;
		palette->Count = num_colours;

		if (png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {
			/* A gray-scale image; generate a palette fading from black to white. */
			colourspace_flag = ImageFlagsColorSpaceGRAY;
			palette->Flags = PaletteFlagsGrayScale;

			for (i=0; i < num_colours; i++) {
				int intensity = i * 255 / (num_colours - 1);

				set_pixel_bgra (&palette->Entries[i], 0, intensity, intensity, intensity, 0xFF); /* alpha */
			}
		} else {
			/* Copy the palette data into the GDI+ structure. */
			colourspace_flag = ImageFlagsColorSpaceRGB;

			palette_entries = num_colours;
			if (palette_entries > info_ptr->num_palette) {
				palette_entries = info_ptr->num_palette;
			}

			for (i=0; i < palette_entries; i++) {
				set_pixel_bgra (&palette->Entries[i], 0,
						info_ptr->palette[i].blue,
						info_ptr->palette[i].green,
						info_ptr->palette[i].red,
						0xFF); /* alpha */
			}
		}

		/* Make sure transparency is respected. */
		if (info_ptr->num_trans > 0) {
			palette->Flags |= PaletteFlagsHasAlpha;
			colourspace_flag |= ImageFlagsHasAlpha;

			if (info_ptr->num_trans > info_ptr->num_palette) {
				info_ptr->num_trans = info_ptr->num_palette;
			}

			for (i=0; i < info_ptr->num_trans; i++) {
				set_pixel_bgra(&palette->Entries[i], 0,
						info_ptr->palette[i].blue,
						info_ptr->palette[i].green,
						info_ptr->palette[i].red,
						info_ptr->trans[i]); /* alpha */
			}
		}


		result = gdip_bitmap_new_with_frame (&gdip_image_frameDimension_page_guid, TRUE);
		result->type = imageBitmap;
		result->cairo_format = CAIRO_FORMAT_ARGB32;
		result->active_bitmap->stride = dest_stride;
		result->active_bitmap->width = width;
		result->active_bitmap->height = height;
		result->active_bitmap->scan0 = rawdata;
		result->active_bitmap->reserved = GBD_OWN_SCAN0;

		switch (bit_depth) {
			case 1: result->active_bitmap->pixel_format = Format1bppIndexed; result->cairo_format = CAIRO_FORMAT_A1; break;
			case 4: result->active_bitmap->pixel_format = Format4bppIndexed; result->cairo_format = CAIRO_FORMAT_A8; break;
			case 8: result->active_bitmap->pixel_format = Format8bppIndexed; result->cairo_format = CAIRO_FORMAT_A8; break;
		}

		result->active_bitmap->image_flags = ImageFlagsReadOnly | ImageFlagsHasRealPixelSize | colourspace_flag; /* assigned when the palette is loaded */
		result->active_bitmap->dpi_horz = 0;
		result->active_bitmap->dpi_vert = 0;
		result->active_bitmap->palette = palette;
	} else {
		int		width;
		int		height;
		guchar		bit_depth;
		guchar		color_type;
		int		channels;
		int		stride;
		int		interlace;
		png_bytep *row_pointers;
		guchar *rawptr;
		int i, j;

		width = png_get_image_width (png_ptr, info_ptr);
		height = png_get_image_height (png_ptr, info_ptr);
		bit_depth = png_get_bit_depth (png_ptr, info_ptr);
		color_type = png_get_color_type (png_ptr, info_ptr);
		channels = png_get_channels (png_ptr, info_ptr);
		interlace = png_get_interlace_type (png_ptr, info_ptr);

		/* According to the libpng manual, this sequence is equivalent to
		* using the PNG_TRANSFORM_EXPAND flag in png_read_png. */
		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_palette_to_rgb (png_ptr);
		}

		if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
			png_set_gray_1_2_4_to_8(png_ptr);
		}

		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
			png_set_tRNS_to_alpha(png_ptr);
		}

		stride = (width * 4) + (sizeof(pixman_bits_t)-1);
		stride &= ~(sizeof(pixman_bits_t)-1);

		row_pointers = png_get_rows (png_ptr, info_ptr);
		rawdata = GdipAlloc (stride * height);
		rawptr = rawdata;

		switch (channels) {
			case 4: {
				for (i = 0; i < height; i++) {
					png_bytep rowp = row_pointers[i];
					for (j = 0; j < width; j++) {
						byte a = rowp[3];
						if (a == 0) {
							set_pixel_bgra (rawptr, 0, 0, 0, 0, 0);
						} else {
							byte b = rowp[2];
							byte g = rowp[1];
							byte r = rowp[0];

							if (a < 0xff) {
								r = pre_multiplied_table [r][a];
								g = pre_multiplied_table [g][a];
								b = pre_multiplied_table [b][a];
							}

							set_pixel_bgra (rawptr, 0, b, g, r, a);
						}
						rowp += 4;
						rawptr += 4;
					}
				}
				break;
			}

			case 3: {
				for (i = 0; i < height; i++) {
					png_bytep rowp = row_pointers[i];
					for (j = 0; j < width; j++) {
						set_pixel_bgra (rawptr, 0, rowp[2], rowp[1], rowp[0], 0xff);
						rowp += 3;
						rawptr += 4;
					}
				}
				break;
			}

			case 1:
				for (i = 0; i < height; i++) {
					png_bytep rowp = row_pointers[i];
					for (j = 0; j < width; j++) {
						png_byte pix = *rowp++;
						set_pixel_bgra (rawptr, 0, pix, pix, pix, 0xff);
						rawptr += 4;
					}
				}
				break;
		}

		result = gdip_bitmap_new_with_frame (&gdip_image_frameDimension_page_guid, TRUE);
		result->type = imageBitmap;

		result->cairo_format = CAIRO_FORMAT_ARGB32;
		result->active_bitmap->stride = stride;
		result->active_bitmap->pixel_format = Format32bppArgb;
		result->active_bitmap->width = width;
		result->active_bitmap->height = height;
		result->active_bitmap->scan0 = rawdata;
		result->active_bitmap->reserved = GBD_OWN_SCAN0;

		result->surface = cairo_image_surface_create_for_data ((unsigned char *)rawdata,
			result->cairo_format,
			result->active_bitmap->width,
			result->active_bitmap->height,
			result->active_bitmap->stride);
		if (channels == 3) {
			result->active_bitmap->pixel_format = Format24bppRgb;
			result->active_bitmap->image_flags = ImageFlagsColorSpaceRGB;
		} else if (channels == 4) {
			result->active_bitmap->pixel_format = Format32bppArgb;
			result->active_bitmap->image_flags = ImageFlagsColorSpaceRGB;
		} else if (channels == 1) {
			result->active_bitmap->pixel_format = Format8bppIndexed;
			result->active_bitmap->image_flags = ImageFlagsColorSpaceGRAY;
		}

		if (color_type & PNG_COLOR_MASK_ALPHA)
			 result->active_bitmap->image_flags |= ImageFlagsHasAlpha;

		result->active_bitmap->image_flags |= ImageFlagsReadOnly | ImageFlagsHasRealPixelSize;
		result->active_bitmap->dpi_horz = 0;
		result->active_bitmap->dpi_vert = 0;
	}

	gdip_load_png_properties(png_ptr, info_ptr, end_info_ptr, result->active_bitmap);
	png_destroy_read_struct (&png_ptr, &info_ptr, &end_info_ptr);

	*image = result;

	return Ok;

error:
	/* coverity[dead_error_line] */
	if (rawdata) {
		GdipFree (rawdata);
	}

	if (png_ptr) {
		png_destroy_read_struct (&png_ptr, info_ptr ? &info_ptr : (png_infopp) NULL, end_info_ptr ? &end_info_ptr : (png_infopp) NULL);
	}

	*image = NULL;
	return InvalidParameter;
}


GpStatus 
gdip_load_png_image_from_file (FILE *fp, GpImage **image)
{
	return gdip_load_png_image_from_file_or_stream (fp, NULL, image);
}

GpStatus
gdip_load_png_image_from_stream_delegate (GetBytesDelegate getBytesFunc, SeekDelegate seeknFunc, GpImage **image)
{
	return gdip_load_png_image_from_file_or_stream (NULL, getBytesFunc, image);
}

GpStatus 
gdip_save_png_image_to_file_or_stream (FILE *fp, PutBytesDelegate putBytesFunc, GpImage *image, GDIPCONST EncoderParameters *params)
{
	png_structp	png_ptr = NULL;
	png_infop	info_ptr = NULL;
	int		i;
	int		bit_depth;
	int		color_type;

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		goto error;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		/* png detected error occured */
		goto error;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		goto error;
	}

	if (fp != NULL) {
		png_init_io (png_ptr, fp);
	} else {
		png_set_write_fn (png_ptr, putBytesFunc, _gdip_png_stream_write_data, _gdip_png_stream_flush_data);
	}

	switch (image->active_bitmap->pixel_format) {
		case Format32bppArgb:
		case Format32bppPArgb:
		case Format32bppRgb:
			color_type = PNG_COLOR_TYPE_RGB_ALPHA;
			bit_depth = 8;
			break;

		case Format24bppRgb:
			color_type = PNG_COLOR_TYPE_RGB; /* FIXME - we should be able to write grayscale PNGs */
			bit_depth = 8;
			break;

		case Format8bppIndexed:
			color_type = PNG_COLOR_TYPE_PALETTE;
			bit_depth = 8;
			break;

		case Format4bppIndexed:
			color_type = PNG_COLOR_TYPE_PALETTE;
			bit_depth = 4;
			break;

		case Format1bppIndexed:
			color_type = PNG_COLOR_TYPE_PALETTE;
			bit_depth = 1;
			break;

		/* We're not going to even try to save these images, for now */
		case Format64bppArgb:
		case Format64bppPArgb:
		case Format48bppRgb:
		case Format16bppArgb1555:
		case Format16bppGrayScale:
		case Format16bppRgb555:
		case Format16bppRgb565:
		default:
			color_type = -1;
			bit_depth = -1;
			break;
	}

	if (bit_depth == -1) {
		goto error;
	}

	png_set_IHDR (png_ptr, info_ptr, image->active_bitmap->width, image->active_bitmap->height, bit_depth, color_type,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	if (gdip_is_an_indexed_pixelformat (image->active_bitmap->pixel_format)) {
		png_color palette[256];

		int palette_entries = image->active_bitmap->palette->Count;
		if (image->active_bitmap->pixel_format == Format4bppIndexed) {
			palette_entries = 16;
		}

		for (i=0; i < palette_entries; i++) {
			ARGB entry = image->active_bitmap->palette->Entries[i];

			int dummy;

			get_pixel_bgra(entry, palette[i].blue, palette[i].green, palette[i].red, dummy);
		}

		png_set_PLTE (png_ptr, info_ptr, palette, palette_entries);
	}

	png_set_filter (png_ptr, 0, PNG_NO_FILTERS);
	png_set_sRGB_gAMA_and_cHRM (png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);
	png_write_info (png_ptr, info_ptr);

#if 0
	if ((image->active_bitmap->pixel_format == Format24bppRgb) || (image->active_bitmap->pixel_format == Format32bppRgb)) {
		png_set_filler (png_ptr, 0, PNG_FILLER_AFTER);
	} else if (image->active_bitmap->pixel_format == Format8bppIndexed) {
		png_set_filler (png_ptr, 0, PNG_FILLER_AFTER);
	}
#endif

	png_set_bgr(png_ptr);

	if (gdip_is_an_indexed_pixelformat (image->active_bitmap->pixel_format)) {
		for (i = 0; i < image->active_bitmap->height; i++) {
			png_write_row (png_ptr, image->active_bitmap->scan0 + i * image->active_bitmap->stride);
		}
	} else if (image->active_bitmap->pixel_format == Format24bppRgb) {
		int j;
		guchar *row_pointer = GdipAlloc (image->active_bitmap->width * 3);
		for (i = 0; i < image->active_bitmap->height; i++) {
			for (j = 0; j < image->active_bitmap->width; j++) {
#ifdef WORDS_BIGENDIAN
				row_pointer[j*3  ] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 3);
				row_pointer[j*3+1] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 2);
				row_pointer[j*3+2] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 1);
#else
				row_pointer[j*3  ] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 0);
				row_pointer[j*3+1] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 1);
				row_pointer[j*3+2] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 2);
#endif /* WORDS_BIGENDIAN */
			}
			png_write_row (png_ptr, row_pointer);
		}
		GdipFree (row_pointer);
	} else {
#ifdef WORDS_BIGENDIAN
		int j;
		guchar *row_pointer = GdipAlloc (image->active_bitmap->width * 4);

		for (i = 0; i < image->active_bitmap->height; i++) {
			for (j = 0; j < image->active_bitmap->width; j++) {
				row_pointer[j*4] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 3);
				row_pointer[j*4+1] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 2);
				row_pointer[j*4+2] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 1);
				row_pointer[j*4+3] = *((guchar *)image->active_bitmap->scan0 + (image->active_bitmap->stride * i) + (j*4) + 0);
			}
			png_write_row (png_ptr, row_pointer);
		}
		GdipFree (row_pointer);
#else
		for (i = 0; i < image->active_bitmap->height; i++) {
			png_write_row (png_ptr, image->active_bitmap->scan0 + (image->active_bitmap->stride * i));
		}
#endif
	}

	png_write_end (png_ptr, NULL);

	png_destroy_write_struct (&png_ptr, &info_ptr);

	return Ok;

error:
	if (png_ptr) {
		png_destroy_write_struct (&png_ptr, info_ptr ? &info_ptr : NULL);
	}
	return GenericError;
}

GpStatus 
gdip_save_png_image_to_file (FILE *fp, GpImage *image, GDIPCONST EncoderParameters *params)
{
	return gdip_save_png_image_to_file_or_stream (fp, NULL, image, params);
}

GpStatus
gdip_save_png_image_to_stream_delegate (PutBytesDelegate putBytesFunc, GpImage *image, GDIPCONST EncoderParameters *params)
{
	return gdip_save_png_image_to_file_or_stream (NULL, putBytesFunc, image, params);
}

#else

#include "pngcodec.h"

GpStatus 
gdip_load_png_image_from_file (FILE *fp, GpImage **image)
{
	*image = NULL;
	return UnknownImageFormat;
}

GpStatus
gdip_load_png_image_from_stream_delegate (GetBytesDelegate getBytesFunc, SeekDelegate seeknFunc, GpImage **image)
{
	*image = NULL;
	return UnknownImageFormat;
}


GpStatus 
gdip_save_png_image_to_file (FILE *fp, GpImage *image, GDIPCONST EncoderParameters *params)
{
	return UnknownImageFormat;
}


GpStatus
gdip_save_png_image_to_stream_delegate (PutBytesDelegate putBytesFunc, GpImage *image, GDIPCONST EncoderParameters *params)
{

	return UnknownImageFormat;
}


ImageCodecInfo *
gdip_getcodecinfo_png ()
{
	return NULL;
}
#endif	/* HAVE_LIBPNG */

