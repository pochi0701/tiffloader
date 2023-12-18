/*
  TIFF loader, by Malcolm McLean

  Meant to be a pretty comprehensive, one file, portable TIFF reader
  We just read everything in as 8 bit RGBs.

  Current limitations - no JPEG, no alpha, a few odd formats still
	unsupported
  No sophisticated colour handling

  Free for public use
  Acknowlegements, Lode Vandevenne for the Zlib decompressor
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "loadtiff.h"

/// <summary>
/// load a tiff, setting the background to white
/// </summary>
/// <returns>the raster data, 0 on error</returns>
BYTE* TIFF::floadtiffwhite ()
{
	BYTE* buff = 0;
	BYTE* rgb = 0;
	BYTE* grey = 0;
	BYTE* cmyk = 0;

	try {
		buff = load_tiff ();
		if (!buff) {
			throw general_exception ("error_exit");  // 例外をスロー
		}
		switch (format) {
		case FMT::FMT_RGBA:
			rgb = new BYTE[3 * width * height];
			for (auto i = 0;i < width * height;i++) {
				rgb[i * 3 + 0] = buff[i * 4 + 0] + 255 - buff[i * 4 + 3];
				rgb[i * 3 + 1] = buff[i * 4 + 1] + 255 - buff[i * 4 + 3];
				rgb[i * 3 + 2] = buff[i * 4 + 2] + 255 - buff[i * 4 + 3];
			}
			format = FMT::FMT_RGB;
			delete[] buff;
			return rgb;

		case FMT::FMT_CMYK:
			format = FMT::FMT_CMYK;
			return buff;

		case FMT::FMT_CMYKA:
			cmyk = new BYTE[4 * width * height];
			for (auto i = 0;i < width * height; i++) {
				cmyk[i * 4] = buff[i * 5];
				cmyk[i * 4 + 1] = buff[i * 5 + 1];
				cmyk[i * 4 + 2] = buff[i * 5 + 2];
				cmyk[i * 4 + 3] = buff[i * 5 + 3];
			}
			format = FMT::FMT_CMYK;
			delete[] buff;
			return cmyk;

		case FMT::FMT_GREYALPHA:
			grey = new BYTE[width * height];
			for (auto i = 0;i < width * height; i++) {
				grey[i] = buff[i * 2] + 255 - buff[i * 2 + 1];
			}
			format = FMT::FMT_GREY;
			delete[] buff;
			return grey;
		default:
			throw general_exception ("error_exit");  // 例外をスロー
		}
	}
	catch (general_exception) {
		delete[] buff;
		delete[] rgb;
		delete[] grey;
		delete[] cmyk;
		format = FMT::FMT_ERROR;
		width = -1;
		height = -1;
		return NULL;
	}
}

/// <summary>
/// load a tiff, setting the background to black
/// </summary>
/// <returns></returns>
BYTE* TIFF::load_tiff ()
{
	int magic;
	unsigned long offset;
	TAG* tags = NULL;
	int Ntags = 0;
	int err;
	BASICHEADER header = {};
	BYTE* answer;

	format = FMT::FMT_ERROR;
	fd->set_endian ();
	magic = fd->fget16 ();
	if (magic != 42) {
		killtags (tags, Ntags);
		throw general_exception ("parse_error");  // 例外をスロー
	}
	offset = fd->fget32 ();
	fd->buffer_ptr = offset;

	tags = load_header (fd, &Ntags);
	if (!tags) {
		killtags (tags, Ntags);
		throw general_exception ("out_of_memory");  // 例外をスロー
	}
	//getchar();
	//header_defaults (&header);
	header.endianness = fd->type;
	header.fill_header (tags, Ntags);
	err = header.header_fixupsections ();
	if (err) {
		killtags (tags, Ntags);
		throw general_exception ("parse_error");  // 例外をスロー
	}
	//printf("here %d %d\n", header.imagewidth, header.imageheight);
	//printf("Bitspersample%d %d %d\n", header.bitspersample[0], header.bitspersample[1], header.bitspersample[2]);
	err = header.header_not_ok ();
	if (err) {
		killtags (tags, Ntags);
		throw general_exception ("parse_error");  // 例外をスロー
	}
	answer = header.load_raster (fd, &format);
	//getchar();
	width = header.imagewidth;
	height = header.imageheight;
	//freeheader (&header);
	killtags (tags, Ntags);
	return answer;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
///BASICHEADER
/////////////////////////////////////////////////////////////////////////////////////////////////

/// <summary>
/// Some TIFF files have tiles in the strip byte counts and so on
/// fixc this up
/// </summary>
/// <param name="header"></param>
/// <returns></returns>
int BASICHEADER::header_fixupsections ()
{
	if (tilewidth == 0 && tileheight == 0) {
		if (Nstripbytecounts > 0 &&
			Nstripoffsets > 0 &&
			Nstripbytecounts == Nstripoffsets
			&& Nstripbytecounts < INT_MAX
			&& Ntileoffsets == 0 &&
			Ntilebytecounts == 0)
			return 0;
		else
			return -1;
	}
	else if (tilewidth > 0 && tileheight > 0) {
		if (Ntilebytecounts == 0) {
			Ntilebytecounts = Nstripbytecounts;
			Nstripbytecounts = 0;
			tilebytecounts = stripbytecounts;
			stripbytecounts = 0;
		}
		if (Ntileoffsets == 0) {
			Ntileoffsets = Nstripoffsets;
			Nstripoffsets = 0;
			tileoffsets = stripoffsets;
			stripoffsets = 0;
		}
		if (Ntilebytecounts > 0 &&
			Ntileoffsets > 0 &&
			Ntilebytecounts == Ntileoffsets
			&& Ntilebytecounts < INT_MAX
			&& Nstripoffsets == 0 &&
			Nstripbytecounts == 0)
			return 0;
		else
			return -1;
	}
	else {
		return -1;
	}
	return -1;
}

/// <summary>
/// check header
/// </summary>
/// <returns>return if success else throw exception.</returns>
int BASICHEADER::header_not_ok ()
{
	int i;
	//newsubfiletype = 0;
	//imagewidth = -1;
	//imageheight = -1;
	if (imagewidth <= 0 || imageheight <= 0 || (double)imagewidth * (double)imageheight > LONG_MAX / 4) {
		throw general_exception ("parse_error");  // 例外をスロー	
	}

	//bitspersample[0] = -1;
	if (samplesperpixel < 1 || samplesperpixel > 16) {
		throw general_exception ("parse_error");  // 例外をスロー
	}

	for (i = 0; i < samplesperpixel; i++) {
		if (bitspersample[i] < 1 || (bitspersample[i] > 32 && bitspersample[i] % 8)) {
			throw general_exception ("parse_error");  // 例外をスロー
		}
	}
	//compression = 1;
	//fillorder = 1;
	//photo_metric_interpretation = -1;
	//stripoffsets = 0;;
	//Nstripoffsets = 0;
	//samplesperpixel = 0;
	//rowsperstrip;
	//stripbytecounts;
	//Nstripbytecounts;
	//xresolution = 1.0;
	//yresolution = 1.0;
	//resolutionunit = 0;
	/*  Palette files */
	//colormap = 0;
	//Ncolormap = 0;
	if (Ncolormap < 0 || Ncolormap > INT_MAX / 4) {
		throw general_exception ("parse_error");  // 例外をスロー
	}
	/* RGB */
	//planarconfiguration = 1;
	if (planarconfiguration != 1 && planarconfiguration != 2) {
		throw general_exception ("parse_error");  // 例外をスロー
	}
	/* YCbCr*/

	//LumaRed = 0.299;
	//LumaGreen = 0.587;
	//LumaBlue = 0.114;
	if (LumaRed <= 0 || LumaGreen <= 0 || LumaBlue <= 0) {
		throw general_exception ("parse_error");  // 例外をスロー
	}
	//YCbCrCoefficients = 0;
	//YCbCrSubSampling_h = 2;
	//YCbCrSubSampling_v = 2;
	//YCbCrPositioning = 1;
	//ReferenceBlackWhite = 0;

	/* Class F */

	//int BadFaxLines;
	//int CleanFaxData;
	//int ConsecutiveBadFaxLines;

	//tilewidth = 0;
	//tileheight = 0;
	if (tilewidth < 0 || tileheight < 0) {
		throw general_exception ("parse_error");  // 例外をスロー
	}
	if ((double)tilewidth * (double)tileheight > LONG_MAX / 4) {
		throw general_exception ("parse_error");  // 例外をスロー
	}
	//tileoffsets = 0;
	//Ntileoffsets = 0;
	//tilebytecounts = 0;
	//Ntilebytecounts = 0;

	//sampleformat = 1;
	//smaxsamplevalue = 0;
	//Nsmaxsamplevalue = 0;
	//sminsamplevalue = 0;
	//Nsminsamplevalue = 0;
	//endianness = -1;
	return 0;
}

/// <summary>
/// fill the header.
/// </summary>
/// <param name="tags">tags</param>
/// <param name="Ntags">number of tags</param>
/// <returns></returns>
int BASICHEADER::fill_header (TAG* tags, int Ntags)
{
	unsigned long ii;
	int jj;

	for (auto i = 0; i < Ntags; i++) {
		if (tags[i].bad) {
			continue;
		}
		switch (tags[i].tagid) {
		case TID::TID_IMAGEWIDTH:
			imagewidth = (int)tags[i].scalar;
			break;
		case TID::TID_IMAGEHEIGHT:
			imageheight = (int)tags[i].scalar;
			break;
		case TID::TID_BITSPERSAMPLE:
			if (tags[i].datacount > 16) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				bitspersample[ii] = (int)tag_get_entry (&tags[i], ii);
			}
			break;
		case TID::TID_COMPRESSION:
			compression = static_cast<COMPRESSION>(tags[i].scalar);
			break;
		case TID::TID_PHOTOMETRICINTERPRETATION:
			photo_metric_interpretation = static_cast<photo_metric_interpretations>(tags[i].scalar);
			break;
		case TID::TID_FILLORDER:
			fillorder = (int)tags[i].scalar;
			break;
		case TID::TID_STRIPOFFSETS:
			stripoffsets = new unsigned long[tags[i].datacount];
			if (!stripoffsets) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				stripoffsets[ii] = (int)tag_get_entry (&tags[i], ii);
			}
			Nstripoffsets = tags[i].datacount;
			break;
		case TID::TID_SAMPLESPERPIXEL:
			samplesperpixel = (int)tags[i].scalar;
			break;
		case TID::TID_ROWSPERSTRIP:
			rowsperstrip = (int)tags[i].scalar;
			break;
		case TID::TID_STRIPBYTECOUNTS:
			stripbytecounts = new unsigned long[tags[i].datacount];
			if (!stripbytecounts) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				stripbytecounts[ii] = (unsigned long)tag_get_entry (&tags[i], ii);
			}
			Nstripbytecounts = tags[i].datacount;
			break;
		case TID::TID_PLANARCONFIGUATION:
			planarconfiguration = (int)tags[i].scalar;
			break;
		case TID::TID_T4OPTIONS:
			T4options = (unsigned long)tags[i].scalar;
			break;
		case TID::TID_PREDICTOR:
			predictor = (int)tags[i].scalar;
			break;
		case TID::TID_COLORMAP:
			if (tags[i].datacount > INT_MAX / 3) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			Ncolormap = tags[i].datacount / 3;
			colormap = new BYTE[Ncolormap * 3];
			if (!colormap) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (jj = 0; jj < Ncolormap; jj++) {
				colormap[jj * 3 + 0] = (int)tag_get_entry (&tags[i], jj) / 256;
			}
			for (jj = 0; jj < Ncolormap; jj++) {
				colormap[jj * 3 + 1] = (int)tag_get_entry (&tags[i], jj + Ncolormap) / 256;
			}
			for (jj = 0; jj < Ncolormap; jj++) {
				colormap[jj * 3 + 2] = (int)tag_get_entry (&tags[i], jj + Ncolormap * 2) / 256;
			}
			break;
		case TID::TID_TILEWIDTH:
			tilewidth = (int)tags[i].scalar;
			break;
		case TID::TID_TILELENGTH:
			tileheight = (int)tags[i].scalar;
			break;
		case TID::TID_TILEOFFSETS:
			tileoffsets = new unsigned long[tags[i].datacount];
			if (!tileoffsets) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				tileoffsets[ii] = (unsigned long)tag_get_entry (&tags[i], ii);
			}
			Ntileoffsets = tags[i].datacount;
			break;
		case TID::TID_TILEBYTECOUNTS:
			tilebytecounts = new unsigned long[tags[i].datacount];
			if (!tilebytecounts) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				tilebytecounts[ii] = (unsigned long)tag_get_entry (&tags[i], ii);
			}
			Ntilebytecounts = tags[i].datacount;
			break;
		case TID::TID_SAMPLEFORMAT:
			if (tags[i].datacount != samplesperpixel) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount;ii++) {
				sampleformat[ii] = static_cast<SAMPLE_FORMAT>(tag_get_entry (&tags[i], ii));
			}
			break;
		case TID::TID_SMINSAMPLEVALUE:
			sminsamplevalue = new double[tags[i].datacount];
			if (!sminsamplevalue) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				sminsamplevalue[ii] = (unsigned long)tag_get_entry (&tags[i], ii);
			}
			Nsminsamplevalue = tags[i].datacount;
			break;
		case TID::TID_SMAXSAMPLEVALUE:
			smaxsamplevalue = new double[tags[i].datacount];
			if (!smaxsamplevalue) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			for (ii = 0; ii < tags[i].datacount; ii++) {
				smaxsamplevalue[ii] = (unsigned long)tag_get_entry (&tags[i], ii);
			}
			Nsmaxsamplevalue = tags[i].datacount;
			break;
		case TID::TID_YCBCRCOEFFICIENTS:
			if (tags[i].datacount < 3) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			LumaRed = tag_get_entry (&tags[i], 0);
			LumaGreen = tag_get_entry (&tags[i], 1);
			LumaBlue = tag_get_entry (&tags[i], 2);
			break;
		case TID::TID_YCBCRSUBSAMPLING:
			if (tags[i].datacount < 2) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			YCbCrSubSampling_h = (int)tag_get_entry (&tags[i], 0);
			YCbCrSubSampling_v = (int)tag_get_entry (&tags[i], 1);
			break;
		case TID::TID_YCBCRPOSITIONING:
			YCbCrPositioning = (int)tags[i].scalar;
			break;
		case TID::TID_EXTRASAMPLES:
			extrasamples = (int)tags[i].scalar;
			break;
		default:
			//not supported.
			//エラーにしない
			//printf ("NOT SUPPORTED:%d\n", tags[i].tagid);
			break;
		}
	}

	return 0;
}
/// <summary>
/// 
/// </summary>
/// <returns></returns>
int BASICHEADER::header_Noutsamples ()
{

	switch (photo_metric_interpretation) {
	case photo_metric_interpretations::PI_BlackIsZero:
	case photo_metric_interpretations::PI_WhiteIsZero:
		return 2;
	case photo_metric_interpretations::PI_CMYK:
		if (extrasamples == 1) {
			return 5;
		}
		else {
			return 4;
		}
		break;
	case photo_metric_interpretations::PI_RGB:
		return 4;
	case photo_metric_interpretations::PI_RGB_Palette:
		return 4;
	case photo_metric_interpretations::PI_YCbCr:
		return 4;
	default:
		return 4;
	}

}
/// <summary>
/// 
/// </summary>
/// <returns></returns>
int BASICHEADER::header_Ninsamples ()
{

	switch (photo_metric_interpretation) {
	case photo_metric_interpretations::PI_BlackIsZero:
	case photo_metric_interpretations::PI_WhiteIsZero:
		return 1 + ((extrasamples == 1) ? 1 : 0);
	case photo_metric_interpretations::PI_CMYK:
		return 4 + ((extrasamples == 1) ? 1 : 0);
	case photo_metric_interpretations::PI_RGB:
		return 3 + ((extrasamples == 1) ? 1 : 0);
	case photo_metric_interpretations::PI_RGB_Palette:
		return 4;
	case photo_metric_interpretations::PI_YCbCr:
		return 4;
	default:
		return 4;
	}

}


FMT BASICHEADER::header_outputformat ()
{

	switch (photo_metric_interpretation) {
	case photo_metric_interpretations::PI_BlackIsZero:
	case photo_metric_interpretations::PI_WhiteIsZero:
		return FMT::FMT_GREYALPHA;
	case photo_metric_interpretations::PI_CMYK:
		if (extrasamples == 1) {
			return FMT::FMT_CMYKA;
		}
		else {
			return FMT::FMT_CMYK;
		}
		break;
	case photo_metric_interpretations::PI_RGB:
		return FMT::FMT_RGBA;
	case photo_metric_interpretations::PI_RGB_Palette:
		return FMT::FMT_RGBA;
	case photo_metric_interpretations::PI_YCbCr:
		return FMT::FMT_RGBA;
	default:
		return FMT::FMT_RGBA;
	}

}
/// <summary>
/// 
/// </summary>
BYTE* BASICHEADER::load_raster (FileData* fd, FMT* format)
{
	BYTE* answer = 0;
	BYTE* strip = 0;
	int i;
	//unsigned long ii;
	int row = 0;
	int swidth, sheight;
	int tilesacross = 0;
	int sample_index;
	int outsamples;
	int insamples;
	try {
		*format = header_outputformat ();
		outsamples = header_Noutsamples ();
		insamples = header_Ninsamples ();

		answer = new BYTE[imagewidth * imageheight * outsamples];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		for (auto ii = 0;ii < imagewidth * imageheight;ii++) {
			answer[ii * outsamples + outsamples - 1] = 255;
		}
		if (tilewidth) {
			tilesacross = (imagewidth + tilewidth - 1) / tilewidth;
		}

		if (planarconfiguration == 2) {
			if (photo_metric_interpretation == photo_metric_interpretations::PI_RGB) {
				sample_index = 0;
				for (i = 0; i < Nstripoffsets; i++) {
					if (sample_index >= insamples)
						continue;
					strip = read_channel (i, &swidth, &sheight, fd);
					if (!strip) {
						throw general_exception ("out_of_memory");  // 例外をスロー
					}
					for (auto ii = 0; ii < swidth * sheight; ii++) {
						answer[((row + ii / swidth) * imagewidth + (ii % swidth)) * outsamples + sample_index] = strip[ii];
					}

					row += sheight;
					if (row >= imageheight) {
						row = 0;
						sample_index++;
					}

					delete[] strip;
				}
				return answer;
			}
			else if (photo_metric_interpretation == photo_metric_interpretations::PI_CMYK) {
				sample_index = 0;
				for (i = 0; i < Nstripoffsets; i++) {
					if (sample_index >= insamples)
						continue;
					strip = read_channel (i, &swidth, &sheight, fd);
					if (!strip) {
						throw general_exception ("out_of_memory");  // 例外をスロー	
					}
					for (auto ii = 0; ii < swidth * sheight; ii++) {
						answer[((row + ii / swidth) * imagewidth + (ii % swidth)) * outsamples + sample_index] = strip[ii];
					}

					row += sheight;
					if (row >= imageheight) {
						row = 0;
						sample_index++;
					}

					delete[] strip;
				}
				return answer;
			}
			else {
				throw general_exception ("parse_error");  // 例外をスロー
			}
		}

		if (Nstripoffsets > 0 && tilesacross == 0) {
			for (i = 0; i < Nstripoffsets; i++) {
				strip = read_strip (i, &swidth, &sheight, fd, &insamples);
				if (!strip) {
					throw general_exception ("out_of_memory");  // 例外をスロー	
				}
				pasteflexible (answer, imagewidth, imageheight, outsamples,
							   strip, swidth, sheight, insamples, 0, row);
				row += sheight;
				delete[] strip;
			}
		}

		if (Nstripoffsets > 0 && tilesacross != 0) {
			Ntileoffsets = Nstripoffsets;
			tileoffsets = stripoffsets;
			stripoffsets = 0;
			Nstripoffsets = 0;
		}
		if (Ntilebytecounts == 0 && tilesacross != 0) {
			Ntilebytecounts = Nstripoffsets;
			tilebytecounts = stripbytecounts;
			stripbytecounts = 0;
			Nstripbytecounts = 0;
		}
		if (Ntileoffsets > 0) {
			for (i = 0; i < Ntileoffsets; i++) {
				strip = read_tile (i, &swidth, &sheight, fd, &insamples);
				if (!strip) {
					throw general_exception ("out_of_memory");  // 例外をスロー
				}

				pasteflexible (answer, imagewidth, imageheight, outsamples,
							   strip, swidth, sheight, insamples,
							   (i % tilesacross) * tilewidth,
							   (i / tilesacross) * tileheight);
				delete[] strip;
			}
		}

		return answer;
	}
	catch (general_exception) {
		//out_of_memory:
		//parse_error:
		delete[] answer;
		delete[] strip;
		*format = FMT::FMT_ERROR;
		return 0;
	}
}



/// <summary>
/// 
/// </summary>
/// <param name="index"></param>
/// <param name="tile_width"></param>
/// <param name="tile_height"></param>
/// <param name="fd"></param>
/// <param name="insamples"></param>
/// <returns></returns>
BYTE* BASICHEADER::read_tile (int index, int* tile_width, int* tile_height, FileData* fd, int* insamples)
{
	BYTE* data = 0;
	BYTE* answer = 0;
	unsigned long N;

	try {
		*insamples = header_Ninsamples ();

		//fseek(fp, tileoffsets[index], SEEK_SET);
		fd->buffer_ptr = tileoffsets[index];
		data = decompress (fd, tilebytecounts[index], compression, &N, tilewidth, tileheight, T4options);
		if (!data) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		answer = new BYTE[*insamples * tilewidth * tileheight];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		*tile_width = tilewidth;
		*tile_height = tileheight;

		switch (photo_metric_interpretation) {
		case photo_metric_interpretations::PI_WhiteIsZero:
		case photo_metric_interpretations::PI_BlackIsZero:
			grey_to_grey (answer, tilewidth, tileheight, data, N, *insamples);
			if (predictor == 2) {
				unpredict_samples (answer, tilewidth, tileheight, *insamples);
			}
			break;
		case photo_metric_interpretations::PI_RGB:
			bitstream_to_rgba (answer, tilewidth, tileheight, data, N, *insamples);
			if (predictor == 2) {
				unpredict_samples (answer, tilewidth, *insamples, tileheight);
			}
			break;
		case photo_metric_interpretations::PI_RGB_Palette:
			pal_to_rgba (answer, tilewidth, tileheight, data, N);
			break;
		case photo_metric_interpretations::PI_CMYK:
			cmyk_to_cmyk (answer, tilewidth, tileheight, data, N, *insamples);
			break;
		case photo_metric_interpretations::PI_YCbCr:
			ycbcr_to_rgba (answer, tilewidth, tileheight, data, N);
			break;
		default:
			//perror("photo_metric_interpretation not supported");
			break;
		}

		delete[] data;
		return answer;
	}
	catch (general_exception) {
		// out_of_memory:
		delete[] data;
		delete[] answer;
		return 0;
	}
}

/// <summary>
/// 
/// </summary>
/// <param name="index"></param>
/// <param name="strip_width"></param>
/// <param name="strip_height"></param>
/// <param name="fd"></param>
/// <param name="insamples"></param>
/// <returns></returns>
BYTE* BASICHEADER::read_strip (int index, int* strip_width, int* strip_height, FileData* fd, int* insamples)
{
	BYTE* data = 0;
	BYTE* answer = 0;
	unsigned long N;
	int stripheight;

	//fseek(fp, stripoffsets[index], SEEK_SET);
	try {
		fd->buffer_ptr = stripoffsets[index];
		if (index == Nstripoffsets - 1) {
			stripheight = imageheight - rowsperstrip * index;
		}
		else {
			stripheight = rowsperstrip;
		}
		data = decompress (fd, stripbytecounts[index], compression, &N, imagewidth, stripheight, T4options);
		if (!data) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}

		*insamples = header_Ninsamples ();
		answer = new BYTE[*insamples * imagewidth * stripheight];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}
		*strip_width = imagewidth;
		*strip_height = stripheight;
		switch (photo_metric_interpretation) {
		case photo_metric_interpretations::PI_WhiteIsZero:
		case photo_metric_interpretations::PI_BlackIsZero:
			grey_to_grey (answer, imagewidth, stripheight, data, N, *insamples);

			if (predictor == 2) {
				unpredict_samples (answer, imagewidth, stripheight, *insamples);
			}
			break;
		case photo_metric_interpretations::PI_RGB:
			bitstream_to_rgba (answer, imagewidth, stripheight, data, N, *insamples);
			if (predictor == 2) {
				unpredict_samples (answer, imagewidth, stripheight, *insamples);
			}
			break;
		case photo_metric_interpretations::PI_RGB_Palette:
			pal_to_rgba (answer, imagewidth, stripheight, data, N);
			break;
		case photo_metric_interpretations::PI_CMYK:
			cmyk_to_cmyk (answer, imagewidth, stripheight, data, N, *insamples);
			break;
		case photo_metric_interpretations::PI_YCbCr:
			ycbcr_to_rgba (answer, imagewidth, stripheight, data, N);
			break;
		default:
			perror ("photometric_interpretation not supported");
		}
		delete[] data;
		return answer;
	}
	catch (general_exception) {
		// out_of_memory:
		delete[] data;
		delete[] answer;
		return 0;
	}
}
/// <summary>
/// 
/// </summary>
/// <param name="index"></param>
/// <param name="channel_width"></param>
/// <param name="channel_height"></param>
/// <param name="fd"></param>
/// <returns></returns>
BYTE* BASICHEADER::read_channel (int index, int* channel_width, int* channel_height, FileData* fd)
{
	BYTE* data = 0;
	BYTE* out = 0;
	unsigned long N;
	int stripheight;
	int stripsperimage = (imageheight + rowsperstrip - 1) / rowsperstrip;
	int sample_index;

	sample_index = index / stripsperimage;
	if (sample_index < 0 || sample_index >= samplesperpixel) {
		return 0;
	}

	try {
		//fseek(fp, stripoffsets[index], SEEK_SET);
		fd->buffer_ptr = stripoffsets[index];
		if ((index % stripsperimage) == stripsperimage - 1) {
			stripheight = imageheight - rowsperstrip * (index % stripsperimage);
		}
		else
			stripheight = rowsperstrip;
		data = decompress (fd, stripbytecounts[index], compression, &N, imagewidth, stripheight, T4options);
		if (!data) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}

		out = new BYTE[imagewidth * stripheight];
		if (!out) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}
		*channel_width = imagewidth;
		*channel_height = stripheight;
		plane_to_channel (out, imagewidth, stripheight, data, N, index / stripsperimage);

		if (predictor == 2) {
			unpredict_samples (out, imagewidth, stripheight, 1);
		}
		delete[] data;
		return out;
	}
	catch (general_exception) {
		// out_of_memory:
		delete[] data;
		delete[] out;
		return 0;

	}
}
/// <summary>
/// 
/// </summary>
/// <param name="out"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="bits"></param>
/// <param name="Nbytes"></param>
/// <param name="sample_index"></param>
/// <returns></returns>
int BASICHEADER::plane_to_channel (BYTE* out, int width, int height, BYTE* bits, unsigned long Nbytes, int sample_index)
{
	int bitstreamflag = 0;
	int val;

	if ((bitspersample[sample_index] % 8) != 0) {
		bitstreamflag = 1;
	}
	if (bitstreamflag == 0) {
		unsigned long i = 0;
		unsigned long counter = 0;

		while (i < Nbytes) {
			out[0] = read_byte_sample (bits, sample_index);
			if (photo_metric_interpretation == photo_metric_interpretations::PI_WhiteIsZero) {
				out[0] = 255 - out[0];
			}
			bits += bitspersample[sample_index] / 8;
			i += bitspersample[sample_index] / 8;
			out++;
			if (counter++ > (unsigned long) (width * height)) {
				return -1;
			}
		}
	}
	else {
		BSTREAM* bs = new BSTREAM (bits, Nbytes, BIG_ENDIAN);
		for (auto i = 0; i < height; i++) {
			for (auto ii = 0; ii < width; ii++) {
				val = bs->getbits (bitspersample[sample_index]);
				val = (val * 255) / ((1 << bitspersample[sample_index]) - 1);

				*out++ = val;
			}
			bs->synch_to_byte ();
		}
		delete bs;
		return 0;
	}
	return 0;
}


/// <summary>
/// 
/// </summary>
/// <param name="grey"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="bits"></param>
/// <param name="Nbytes"></param>
/// <param name="header"></param>
/// <param name="insamples"></param>
/// <returns></returns>
int BASICHEADER::grey_to_grey (BYTE* grey, int width, int height, BYTE* bits, unsigned long Nbytes, int insamples)
{

	//unsigned long i;
	//int ii, iii;
	int totbits = 0;
	int bitstreamflag = 0;
	int counter = 0;

	for (auto i = 0; i < samplesperpixel; i++) {
		totbits += bitspersample[i];
	}
	for (auto i = 0; i < samplesperpixel; i++) {
		if ((bitspersample[i] % 8) != 0) {
			bitstreamflag = 1;
		}
	}

	if (bitstreamflag == 0) {
		unsigned long i = 0;

		while (i < Nbytes - totbits / 8 + 1) {
			grey[0] = read_byte_sample (bits, 0);
			if (photo_metric_interpretation == photo_metric_interpretations::PI_WhiteIsZero) {
				grey[0] = 255 - grey[0];
			}
			bits += bitspersample[0] / 8;
			i += bitspersample[0] / 8;

			if (insamples == 2 && samplesperpixel > 1) {
				grey[1] = read_byte_sample (bits, 0);
				bits += bitspersample[1] / 8;
				i += bitspersample[1] / 8;
			}

			for (auto ii = insamples; ii < samplesperpixel; ii++) {
				bits += bitspersample[ii] / 8;
				i += bitspersample[ii] / 8;
			}

			grey += insamples;

			if (counter++ > width * height) {
				break;
			}
		}

		return 0;
	}
	else {
		BSTREAM* bs = new BSTREAM (bits, Nbytes, BIG_ENDIAN);
		for (auto i = 0; i < height; i++) {
			for (auto ii = 0; ii < width; ii++) {
				int val = bs->getbits (bitspersample[0]);
				grey[0] = (val * 255) / ((1 << (bitspersample[0])) - 1);
				if (photo_metric_interpretation == photo_metric_interpretations::PI_WhiteIsZero) {
					grey[0] = 255 - grey[0];
				}
				if (insamples == 2 && samplesperpixel > 1) {
					val = bs->getbits (bitspersample[1]);
					grey[1] = (val * 255) / ((1 << (bitspersample[1])) - 1);
				}
				for (auto iii = insamples; iii < samplesperpixel; iii++) {
					bs->getbits (bitspersample[iii]);
				}
				grey += insamples;
			}
			bs->synch_to_byte ();
		}
		delete bs;
		return 0;
	}

}
/// <summary>
/// 
/// </summary>
/// <param name="rgba"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="bits"></param>
/// <param name="Nbytes"></param>
/// <returns></returns>
int BASICHEADER::pal_to_rgba (BYTE* rgba, int width, int height, BYTE* bits, unsigned long Nbytes)
{
	//unsigned long i;
	unsigned long counter = 0;
	int index;
	//int ii, iii;
	int totbits = 0;
	int bitstreamflag = 0;


	for (auto i = 0; i < samplesperpixel; i++) {
		totbits += bitspersample[i];
	}
	for (auto i = 0; i < samplesperpixel; i++) {
		if ((bitspersample[i] % 8) != 0) {
			bitstreamflag = 1;
		}
	}

	if (bitstreamflag == 0) {
		unsigned long i = 0;

		while (i < Nbytes - totbits / 8 + 1) {
			index = read_int_sample (bits, 0);
			bits += bitspersample[0] / 8;
			i += bitspersample[0] / 8;

			if (index >= 0 && index < Ncolormap) {
				rgba[0] = colormap[index * 3];
				rgba[1] = colormap[index * 3 + 1];
				rgba[2] = colormap[index * 3 + 2];
			}

			for (auto ii = 1; ii < samplesperpixel; ii++) {
				bits += bitspersample[ii] / 8;
				i += bitspersample[ii] / 8;
			}
			rgba += 4;
			counter++;
			if (counter > (unsigned long) (width * height)) {
				break;
			}
		}

		return 0;
	}
	else {
		BSTREAM* bs = new BSTREAM (bits, Nbytes, BIG_ENDIAN);
		for (auto i = 0; i < height; i++) {
			for (auto ii = 0; ii < width; ii++) {
				index = bs->getbits (bitspersample[0]);
				if (index >= 0 && index < Ncolormap) {
					rgba[0] = colormap[index * 3];
					rgba[1] = colormap[index * 3 + 1];
					rgba[2] = colormap[index * 3 + 2];
				}
				for (auto iii = 1; iii < samplesperpixel; iii++) {
					bs->getbits (bitspersample[iii]);
				}
				rgba += 4;
			}
			bs->synch_to_byte ();
		}
		delete bs;
		return 0;
	}



	return -1;
}
/// <summary>
/// 
/// </summary>
/// <param name="rgba"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="bits"></param>
/// <param name="Nbytes"></param>
/// <returns></returns>
int BASICHEADER::ycbcr_to_rgba (BYTE* rgba, int width, int height, BYTE* bits, unsigned long Nbytes)
{
	//unsigned long i;
	int ii;
	int totbits = 0;
	int bitstreamflag = 0;
	int Y[16] = {};
	int Cb, Cr;
	int x, y;
	int ix, iy;
	//unsigned long counter = 0;
	try {

		if (YCbCrSubSampling_h * YCbCrSubSampling_v > 16) {
			throw general_exception ("parse_error");  // 例外をスロー
		}
		if (LumaGreen == 0.0)
			LumaGreen = 1.0;

		for (auto i = 0; i < samplesperpixel; i++) {
			totbits += bitspersample[i];
		}
		for (auto i = 0; i < samplesperpixel; i++) {
			if ((bitspersample[i] % 8) != 0) {
				bitstreamflag = 1;
			}
		}

		x = 0; y = 0;
		// Need for some YcbCr images -
		//bits += 3;
		if (bitstreamflag == 0) {
			unsigned long i = 0;

			while (i < Nbytes - totbits / 8 + 1) {
				for (ii = 0; ii < YCbCrSubSampling_h * YCbCrSubSampling_v; ii++) {
					Y[ii] = read_byte_sample (bits, 0);
					bits += bitspersample[0] / 8;
					i += bitspersample[0] / 8;
				}
				Cb = read_byte_sample (bits, 1);
				bits += bitspersample[1] / 8;
				i += bitspersample[1] / 8;
				Cr = read_byte_sample (bits, 2);
				bits += bitspersample[2] / 8;
				i += bitspersample[2] / 8;


				for (ii = 0; ii < YCbCrSubSampling_h * YCbCrSubSampling_v; ii++) {
					int red, green, blue;
					red = (int)((Cr - 127) * (2 - 2 * LumaRed) + Y[ii]);
					blue = (int)((Cb - 127) * (2 - 2 * LumaBlue) + Y[ii]);
					green = (int)((Y[ii] - LumaBlue * blue - LumaRed * red) / LumaGreen);

					red = red < 0 ? 0 : red > 255 ? 255 : red;
					green = green < 0 ? 0 : green > 255 ? 255 : green;
					blue = blue < 0 ? 0 : blue > 255 ? 255 : blue;
					ix = x + (ii % YCbCrSubSampling_h);
					iy = y + (ii / YCbCrSubSampling_h);
					//YcbcrToRGB(Y[ii], Cb, Cr, &r, &g, &b);
					if (ix < width && iy < height) {
						rgba[(iy * width + ix) * 4] = red;
						rgba[(iy * width + ix) * 4 + 1] = green;
						rgba[(iy * width + ix) * 4 + 2] = blue;
					}

				}
				x += YCbCrSubSampling_h;
				if (x >= width) {
					x = 0;
					y += YCbCrSubSampling_v;
				}
				for (ii = 3; ii < samplesperpixel; ii++) {
					bits += bitspersample[ii] / 8;
					i += bitspersample[ii] / 8;
				}
			}
			return 0;
		}
		return 0;
	}
	catch (general_exception) {
		//parse_error:
		return -2;
	}

}
/// <summary>
/// 
/// </summary>
/// <param name="cmyk"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="bits"></param>
/// <param name="Nbytes"></param>
/// <param name="insamples"></param>
/// <returns></returns>
int BASICHEADER::cmyk_to_cmyk (BYTE* cmyk, int width, int height, BYTE* bits, unsigned long Nbytes, int insamples)
{
	//unsigned long i;
	int ii;
	int totbits = 0;
	int bitstreamflag = 0;

	//int red = 0;
	// green = 0;
	//int blue = 0;
	int x, y;
	int counter = 0;

	try {

		for (auto i = 0; i < samplesperpixel; i++) {
			totbits += bitspersample[i];
		}
		for (auto i = 0; i < samplesperpixel; i++) {
			if ((bitspersample[i] % 8) != 0) {
				bitstreamflag = 1;
			}
		}

		x = 0;
		y = 0;

		if (bitstreamflag == 0) {
			unsigned long i = 0;
			int C, M, Y, K, A{};
			int Cprev = 0, Yprev = 0, Mprev = 0, Kprev = 0, Aprev = 0;
			while (i < Nbytes - totbits / 8 + 1) {
				C = read_byte_sample (bits, 0);
				bits += bitspersample[0] / 8;
				i += bitspersample[0] / 8;
				M = read_byte_sample (bits, 1);
				bits += bitspersample[1] / 8;
				i += bitspersample[1] / 8;
				Y = read_byte_sample (bits, 2);
				bits += bitspersample[2] / 8;
				i += bitspersample[2] / 8;
				K = read_byte_sample (bits, 3);
				bits += bitspersample[3] / 8;
				i += bitspersample[3] / 8;

				if (insamples == 5) {
					A = read_byte_sample (bits, 4);
					bits += bitspersample[4] / 8;
					i += bitspersample[4] / 8;
				}

				if (predictor == 2) {
					C = (C + Cprev) & 0xFF;
					M = (M + Mprev) & 0xFF;
					Y = (Y + Yprev) & 0xFF;
					K = (K + Kprev) & 0xFF;
					A = (A + Aprev) & 0xFF;
					Cprev = C;
					Mprev = M;
					Yprev = Y;
					Kprev = K;
					Aprev = A;
				}

				//red = (255 * (255 - C) * (255 - K))/(255 * 255);
				//green = (255 * (255 - M) * (255 - K))/(255 * 255);
				//blue = (255 * (255 - Y) * (255 - K))/(255 * 255);

				//rgba[0] = red;
				//rgba[1] = green;
				//rgba[2] = blue;

				cmyk[0] = C;
				cmyk[1] = M;
				cmyk[2] = Y;
				cmyk[3] = K;
				if (insamples == 5)
					cmyk[4] = A;
				cmyk += insamples;

				for (ii = insamples; ii < samplesperpixel; ii++) {
					bits += bitspersample[ii] / 8;
					i += bitspersample[ii] / 8;
				}

				x++;
				if (x == width) {
					x = 0;
					y++;
					Cprev = 0;
					Mprev = 0;
					Yprev = 0;
					Kprev = 0;
					Aprev = 0;
				}
				if (counter++ > width * height) {
					throw general_exception ("parse_error");  // 例外をスロー
				}
			}
			return 0;
		}
		else {
			throw general_exception ("parse_error");  // 例外をスロー
		}
		return 0;
	}
	catch (general_exception) {
		//parse_error:
		return -2;
	}
	catch (...) {
		return -3;
	}
}


/// <summary>
/// 
/// </summary>
/// <param name="rgba"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="bits"></param>
/// <param name="Nbytes"></param>
/// <param name="insamples"></param>
/// <returns></returns>
int BASICHEADER::bitstream_to_rgba (BYTE* rgba, int width, int height, BYTE* bits, unsigned long Nbytes, int insamples)
{
	//unsigned long i;
	int ii, iii;
	int totbits = 0;
	int bitstreamflag = 0;
	int red, green, blue, alpha;
	//unsigned long counter = 0;
	int lastsample;

	for (auto i = 0; i < samplesperpixel; i++) {
		totbits += bitspersample[i];
	}
	for (auto i = 0; i < samplesperpixel; i++) {
		if ((bitspersample[i] % 8) != 0) {
			bitstreamflag = 1;
		}
	}

	if (bitstreamflag == 0) {
		unsigned long i = 0;

		while (i < Nbytes - totbits / 8 + 1) {
			rgba[0] = read_byte_sample (bits, 0);
			bits += bitspersample[0] / 8;
			i += bitspersample[0] / 8;
			rgba[1] = read_byte_sample (bits, 1);
			bits += bitspersample[1] / 8;
			i += bitspersample[1] / 8;
			rgba[2] = read_byte_sample (bits, 2);
			bits += bitspersample[2] / 8;
			i += bitspersample[2] / 8;

			if (insamples == 4) {
				rgba[3] = read_byte_sample (bits, 3);
				bits += bitspersample[3] / 8;
				i += bitspersample[3] / 8;
				lastsample = 4;
			}
			else {
				lastsample = 3;
			}


			for (ii = lastsample; ii < samplesperpixel; ii++) {
				bits += bitspersample[ii] / 8;
				i += bitspersample[ii] / 8;
			}
			rgba += insamples;
		}

		return 0;
	}
	else {
		BSTREAM* bs = new BSTREAM (bits, Nbytes, BIG_ENDIAN);
		for (auto i = 0; i < height; i++) {
			for (ii = 0; ii < width; ii++) {
				red = bs->getbits (bitspersample[0]);
				red = (red * 255) / ((1 << (bitspersample[0])) - 1);
				green = bs->getbits (bitspersample[1]);
				green = (green * 255) / ((1 << (bitspersample[1])) - 1);
				blue = bs->getbits (bitspersample[2]);
				blue = (blue * 255) / ((1 << (bitspersample[2])) - 1);
				rgba[0] = red;
				rgba[1] = green;
				rgba[2] = blue;
				if (insamples == 4) {
					alpha = bs->getbits (bitspersample[3]);
					alpha = (alpha * 255) / ((1 << (bitspersample[3])) - 1);
					rgba[3] = alpha;
				}
				for (iii = insamples; iii < samplesperpixel; iii++) {
					bs->getbits (bitspersample[iii]);
				}
				rgba += insamples;
			}
			bs->synch_to_byte ();
		}
		delete bs;
	}

	return 0;
}

/// <summary>
/// read a sample that is an intege
/// Called for colour-index modes
/// </summary>
/// <param name="bytes"></param>
/// <param name="sample_index"></param>
/// <returns></returns>
int BASICHEADER::read_int_sample (const BYTE* bytes, int sample_index)
{
	//int i;
	int answer = 0;
	if (sampleformat[sample_index] == SAMPLE_FORMAT::SAMPLEFORMAT_UINT) {
		if (endianness == BIG_ENDIAN) {
			for (auto i = 0; i < bitspersample[sample_index] / 8; i++) {
				answer <<= 8;
				answer |= bytes[i];
			}
		}
		else {
			for (auto i = 0; i < bitspersample[sample_index] / 8; i++) {
				answer |= bytes[i] << (i * 8);
			}
		}
	}
	else {
		// suppress errors by returning 0
		return 0;
	}

	return answer;
}

/*
  read a sample froma  byte stream ( as tream where all the filedsa re whole byte multiples)
  Returns: sample in range 0 - 255
*/

/// <summary>
/// 
/// </summary>
/// <param name="bytes"></param>
/// <param name="sample_index"></param>
/// <returns></returns>
int BASICHEADER::read_byte_sample (BYTE* bytes, int sample_index)
{
	int answer = -1;
	double real;
	double low, high;
	double highest = 0.0;

	if (sampleformat[sample_index] == SAMPLE_FORMAT::SAMPLEFORMAT_UINT) {
		if (endianness == BIG_ENDIAN) {
			answer = bytes[0];
		}
		else {
			answer = bytes[bitspersample[sample_index] / 8 - 1];
		}
	}
	else if (sampleformat[sample_index] == SAMPLE_FORMAT::SAMPLEFORMAT_IEEEFP) {
		if (bitspersample[sample_index] == 64) {
			real = memread_ieee754 (bytes, endianness == ENDIAN::BIG_ENDIAN ? 1 : 0);
		}
		else if (bitspersample[sample_index] == 32) {
			real = memread_ieee754f (bytes, endianness == ENDIAN::BIG_ENDIAN ? 1 : 0);
		}
		else {
			throw general_exception ("error");
		}
		if (sminsamplevalue) {
			low = smaxsamplevalue[sample_index];
		}
		else {
			low = 0;
		}

		if (smaxsamplevalue) {
			high = smaxsamplevalue[sample_index];
		}
		else {
			high = 512.0;
		}
		if (highest < real) {
			highest = real;
		}
		return (int)((real - low) * 255.0 / (high - low));
	}

	return answer;
}
/// <summary>
/// unpredict samples
/// </summary>
/// <param name="buff"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="depth"></param>
void BASICHEADER::unpredict_samples (BYTE* buff, int width, int height, int depth)
{
	for (auto i = 0; i < height; i++) {
		for (auto ii = 1; ii < width; ii++) {
			for (auto iii = 0;iii < depth;iii++) {
				buff[(i * width + ii) * depth + iii] += buff[(i * width + ii - 1) * depth + iii];
			}
		}
	}
}



/*///////////////////////////////////////////////////////////////////////////////////////*/
/* data decompression section */
/*///////////////////////////////////////////////////////////////////////////////////////*/
typedef struct
{
	unsigned ignore_adler32; /*if 1, continue and don't give an error message if the Adler32 checksum is corrupted*/
	unsigned custom_decoder; /*use custom decoder if LODEPNG_CUSTOM_ZLIB_DECODER and LODEPNG_COMPILE_ZLIB are enabled*/
} LodePNGDecompressSettings;

void invert (BYTE* bits, unsigned long N);
BYTE* unpackbits (FileData* fd, unsigned long count, unsigned long* Nret);
BYTE* ccittdecompress (BYTE* in, unsigned long count, unsigned long* Nret, int width, int height, bool eol);
BYTE* ccittgroup4decompress (BYTE* in, unsigned long count, unsigned long* Nret, int width, int height, int eol);
int loadlzw (BYTE* out, FileData* fd, unsigned long count, unsigned long* Nret);
unsigned lodepng_zlib_decompress (BYTE** out, size_t* outsize, const BYTE* in,
								  size_t insize, const LodePNGDecompressSettings* settings);

/*
  Master decompression function
  Params:
	fp - input file, pointing to start of data section
	count - number of bytes in stream to decompress
	Nret - return for number of decompressed bytes
	width, height - width and height of strip or tile
	T4option - T4 twiddle
  Returns: pointer to decompressed dta, 0 on fail

*/
BYTE* decompress (FileData* fd, unsigned long count, COMPRESSION compression, unsigned long* Nret, int width, int height, unsigned long T4options)
{
	BYTE* answer = 0;
	BYTE* buff = NULL;
	unsigned long pos = 0;
	size_t decompsize = 0;
	LodePNGDecompressSettings settings = {};
	try {
		switch (compression) {
		case COMPRESSION::COMPRESSION_NONE:
			answer = new BYTE[count];
			if (!answer) {
				throw general_exception ("out_of_memory");  // 例外をスロー	
			}
			//fread(answer, 1, count, fp);
			fd->memcpy (answer, count);
			//memcpy(answer,fd->buffer + fd->buffer_ptr, count);
			//fd->buffer_ptr += count;
			*Nret = count;
			return answer;
		case  COMPRESSION::COMPRESSION_CCITTRLE:
			buff = new BYTE[count];
			//unsigned long i = 0;
			if (!buff) {
				throw general_exception ("out_of_memory");  // 例外をスロー	
			}
			//fread(buff, 1, count, fp);
			fd->memcpy (buff, count);
			//memcpy(buff, fd->buffer + fd->buffer_ptr, count);
			//fd->buffer_ptr += count;
			answer = ccittdecompress (buff, count, Nret, width, height, false);
			if (answer) {
				invert (answer, *Nret);
			}
			delete[] buff;
			return answer;
		case COMPRESSION::COMPRESSION_CCITTFAX3:
			buff = new BYTE[count];
			if (!buff) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			//fread(buff, 1, count, fp);
			fd->memcpy (buff, count);
			//memcpy(buff, fd->buffer + fd->buffer_ptr, count);
			//fd->buffer_ptr += count;
			if ((T4options & 0x04) == 0) {
				answer = ccittdecompress (buff, count, Nret, width, height, true);
			}
			else {
				answer = 0; /* not handling for now */
			}
			if (answer) {
				invert (answer, *Nret);
			}
			delete[] buff;
			return answer;
		case COMPRESSION::COMPRESSION_CCITTFAX4:
			buff = new BYTE[count];
			if (!buff) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			//fread(buff, 1, count, fp);
			fd->memcpy (buff, count);
			//memcpy(buff, fd->buffer + fd->buffer_ptr, count);
			//fd->buffer_ptr += count;
			answer = ccittgroup4decompress (buff, count, Nret, width, height, 0);

			//if (answer) {
			//	invert (answer, *Nret);
			//}
			delete[] buff;
			return answer;
		case COMPRESSION::COMPRESSION_PACKBITS:
			answer = unpackbits (fd, count, Nret);
			return answer;
		case COMPRESSION::COMPRESSION_LZW:
			pos = fd->buffer_ptr;//ftell(fp);
			int err;
			err = loadlzw (0, fd, count, Nret);
			answer = new BYTE[*Nret];
			if (!answer) {
				throw general_exception ("out_of_memory");  // 例外をスロー	
			}
			//fseek(fp, pos, SEEK_SET);
			fd->buffer_ptr = pos;
			loadlzw (answer, fd, count, Nret);
			return answer;
		case COMPRESSION::COMPRESSION_ADOBE_DEFLATE:
		case COMPRESSION::COMPRESSION_DEFLATE:

			settings.custom_decoder = 0;
			settings.ignore_adler32 = 0;
			buff = new BYTE[count];
			if (!buff) {
				throw general_exception ("out_of_memory");  // 例外をスロー	
			}
			//fread(buff, 1, count, fp);
			fd->memcpy (buff, count);
			//memcpy(buff, fd->buffer + fd->buffer_ptr, count);
			//fd->buffer_ptr += count;
			*Nret = 0;
			answer = 0;
			decompsize = 0;
			lodepng_zlib_decompress (&answer, &decompsize, buff, count, &settings);
			*Nret = (unsigned long)decompsize;
			delete[] buff;
			return answer;
		default:
			//perror("compression not supprted");
			break;
			//char work[256];
			//const char* work2;
			//switch (compression) {
			//case COMPRESSION::COMPRESSION_OJPEG:
			//	work2 = "COMPRESSION_OJPEG";break;
			//case COMPRESSION::COMPRESSION_JPEG:
			//	work2 = "COMPRESSION_JPEG";break;
			//case COMPRESSION::COMPRESSION_NEXT:
			//	work2 = "COMPRESSION_NEXT";break;
			//case COMPRESSION::COMPRESSION_CCITTRLEW:
			//	work2 = "COMPRESSION_CCITTRLEW";break;
			//case COMPRESSION::COMPRESSION_THUNDERSCAN:
			//	work2 = "COMPRESSION_THUNDERSCAN";break;
			//case COMPRESSION::COMPRESSION_IT8CTPAD:
			//	work2 = "COMPRESSION_IT8CTPAD";break;
			//case COMPRESSION::COMPRESSION_IT8LW:
			//	work2 = "COMPRESSION_IT8LW";break;
			//case COMPRESSION::COMPRESSION_IT8MP:
			//	work2 = "COMPRESSION_IT8MP";break;
			//case COMPRESSION::COMPRESSION_IT8BL:
			//	work2 = "COMPRESSION_IT8BL";break;
			//case COMPRESSION::COMPRESSION_PIXARFILM:
			//	work2 = "COMPRESSION_PIXARFILM";break;
			//case COMPRESSION::COMPRESSION_PIXARLOG:
			//	work2 = "COMPRESSION_PIXARLOG";break;
			//case COMPRESSION::COMPRESSION_DCS:
			//	work2 = "COMPRESSION_DCS";break;
			//case COMPRESSION::COMPRESSION_JBIG:
			//	work2 = "COMPRESSION_JBIG";break;
			//case COMPRESSION::COMPRESSION_SGILOG:
			//	work2 = "COMPRESSION_SGILOG";break;
			//case COMPRESSION::COMPRESSION_SGILOG24:
			//	work2 = "COMPRESSION_SGILOG24";break;
			//case COMPRESSION::COMPRESSION_JP2000:
			//	work2 = "COMPRESSION_JP2000";break;
			//default:
			//	work2 = "COMPRESSION_UNKNOWN";break;
			//}
			//sprintf_s(work, "compression not supported:%s", work2);
			//throw general_exception(work);
		}
		return 0;
	}
	catch (general_exception) {
		//out_of_memory:
		throw;
	}
}

void invert (BYTE* bits, unsigned long N)
{
	//unsigned long i;

	for (auto i = 0UL; i < N; i++) {
		bits[i] ^= 0xFF;
	}
}

/*
  unpackbits decompressor.
  Nice and easy compression scheme
*/
BYTE* unpackbits (FileData* fd, unsigned long count, unsigned long* Nret)
{
	unsigned long pos = fd->buffer_ptr;//ftell(fp);
	unsigned long N = 0;
	//unsigned long i, j;
	long Nleft;
	signed char header;
	int ch;
	BYTE* answer = 0;

	try {
		Nleft = count;
		while (Nleft > 0) {
			if ((ch = fd->fgetcc ()) == EOF) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			Nleft--;
			header = (signed char)ch;
			if (header > 0) {
				N += header + 1;
				for (auto i = 0; i < header + 1; i++) {
					fd->fgetcc ();
					Nleft--;
				}
			}
			else if (header > -128) {
				N += 1 - header;
				fd->fgetcc ();
				Nleft--;
			}
			else {
			}
		}
		//fseek(fp, pos, SEEK_SET);
		fd->buffer_ptr = pos;
		Nleft = count;
		answer = new BYTE[N];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		auto j = 0;
		while (Nleft > 0) {
			if ((ch = fd->fgetcc ()) == EOF) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			Nleft--;
			header = (signed char)ch;
			if (header > 0) {
				for (auto i = 0; i < header + 1; i++) {
					answer[j++] = fd->fgetcc ();
					Nleft--;
				}
			}
			else if (header > -128) {
				ch = fd->fgetcc ();
				Nleft--;
				for (auto i = 0; i < 1 - header; i++) {
					answer[j++] = ch;
				}
			}
			else {
			}

		}

		*Nret = N;
		return answer;
	}
	catch (general_exception) {
		//parse_error:
		//out_of_memory:
		delete[] answer;
		*Nret = 0;
		return 0;
	}
	catch (...) {
		delete[] answer;
		*Nret = 0;
		return 0;
	}
}

typedef struct
{
	//int code;
	int prefix;
	int suffix;
	int len;
	//BYTE* ptr;
} ENTRY;

class HUFFNODE
{
public:
	HUFFNODE* zero;
	HUFFNODE* one;
	int symbol;
	HUFFNODE ()
	{
		zero = NULL;
		one = NULL;
		symbol = -1;
	}
	/// <summary>
	/// Huffman tree destructor
	/// </summary>
	/// <param name="root"></param>
	~HUFFNODE ()
	{
		delete zero;
		delete one;
	}
	/// <summary>
	/// get huffman symbol
	/// </summary>
	/// <param name="bs"></param>
	/// <returns>symbol</returns>
	int gethuffmansymbol (BSTREAM* bs)
	{

		int bit;

		if (zero == NULL && one == NULL) {

			return symbol;
		}
		bit = bs->getbit ();
		//printf("%d", bit);
		if (bit == 0 && zero) {
			return zero->gethuffmansymbol (bs);
		}
		else if (bit == 1 && one) {
			return one->gethuffmansymbol (bs);
		}
		else {
			return symbol;
		}
	}

	/// <summary>
	/// debug print
	/// </summary>
	/// <param name="N"></param>
	void debughufftree (int N)
	{
		int i;
		for (i = 0; i < N * 2; i++)
			printf (" ");
		printf ("%d %p %p\n", symbol, zero, one);
		if (zero)
			zero->debughufftree (N + 1);
		if (one)
			one->debughufftree (N + 1);

	}

	/// <summary>
	/// add a symbol to the tree
	/// </summary>
	/// <param name="root">original tree (start off with null)</param>
	/// <param name="str">the code, to add, encoded in ascii</param>
	/// <param name="symbol">associated symbol</param>
	/// <param name="err">error return sticky, 0 = success</param>
	/// <returns>new tree (meaningful internally, after the first external call will always return the root)</returns>
	static HUFFNODE* addhuffmansymbol (HUFFNODE* root, const char* str, int symbol, int* err)
	{
		HUFFNODE* answer = NULL;
		if (root) {
			if (str[0] == '0') {
				root->zero = addhuffmansymbol (root->zero, str + 1, symbol, err);
			}
			else if (str[0] == '1') {
				root->one = addhuffmansymbol (root->one, str + 1, symbol, err);
			}
			else if (str[0] == 0) {
				root->symbol = symbol;
			}

			return root;
		}
		else {
			answer = new HUFFNODE ();
			if (!answer) {
				*err = -1;
				return 0;
			}
			return addhuffmansymbol (answer, str, symbol, err);
		}
		return 0;
	}
};


/*///////////////////////////////////////////////////////////////////////////////////////////////////*/
/*   CCITT decoding section*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////*/
struct ccitt2dcode { CCITT symbol; const char* code; };

struct ccitt2dcode ccitt2dtable[11] =
{
	{ CCITT::CCITT_PASS, "0001" },
	{ CCITT::CCITT_HORIZONTAL, "001" },
	{ CCITT::CCITT_VERTICAL_0, "1" },
	{ CCITT::CCITT_VERTICAL_R1, "011" },
	{ CCITT::CCITT_VERTICAL_R2, "000011" },
	{ CCITT::CCITT_VERTICAL_R3, "0000011" },
	{ CCITT::CCITT_VERTICAL_L1, "010" },
	{ CCITT::CCITT_VERTICAL_L2, "000010" },
	{ CCITT::CCITT_VERTICAL_L3, "0000010" },
	{ CCITT::CCITT_EXTENSION, "0000001" },
	{ CCITT::CCITT_ENDOFFAXBLOCK, "000000000001"}
};

struct ccittcode { int whitelen; const char* whitecode; int blacklen; const char* blackcode; };
const int EOL = -2;
struct ccittcode ccitttable[105] =
{
	{ 0, "00110101", 0, "0000110111" },
	{ 1, "000111", 1, "010" },
	{ 2, "0111", 2, "11" },
	{ 3, "1000", 3, "10" },
	{ 4, "1011", 4, "011" },
	{ 5, "1100", 5, "0011" },
	{ 6, "1110", 6, "0010" },
	{ 7, "1111", 7, "00011" },
	{ 8, "10011", 8, "000101" },
	{ 9, "10100", 9, "000100" },
	{ 10, "00111", 10, "0000100" },
	{ 11, "01000", 11, "0000101" },
	{ 12, "001000", 12, "0000111" },
	{ 13, "000011", 13, "00000100" },
	{ 14, "110100", 14, "00000111" },
	{ 15, "110101", 15, "000011000" },
	{ 16, "101010", 16, "0000010111" },
	{ 17, "101011", 17, "0000011000" },
	{ 18, "0100111", 18, "0000001000" },
	{ 19, "0001100", 19, "00001100111" },
	{ 20, "0001000", 20, "00001101000" },
	{ 21, "0010111", 21, "00001101100" },
	{ 22, "0000011", 22, "00000110111" },
	{ 23, "0000100", 23, "00000101000" },
	{ 24, "0101000", 24, "00000010111" },
	{ 25, "0101011", 25, "00000011000" },
	{ 26, "0010011", 26, "000011001010" },
	{ 27, "0100100", 27, "000011001011" },
	{ 28, "0011000", 28, "000011001100" },
	{ 29, "00000010", 29, "000011001101" },
	{ 30, "00000011", 30, "000001101000" },
	{ 31, "00011010", 31, "000001101001" },
	{ 32, "00011011", 32, "000001101010" },
	{ 33, "00010010", 33, "000001101011" },
	{ 34, "00010011", 34, "000011010010" },
	{ 35, "00010100", 35, "000011010011" },
	{ 36, "00010101", 36, "000011010100" },
	{ 37, "00010110", 37, "000011010101" },
	{ 38, "00010111", 38, "000011010110" },
	{ 39, "00101000", 39, "000011010111" },
	{ 40, "00101001", 40, "000001101100" },
	{ 41, "00101010", 41, "000001101101" },
	{ 42, "00101011", 42, "000011011010" },
	{ 43, "00101100", 43, "000011011011" },
	{ 44, "00101101", 44, "000001010100" },
	{ 45, "00000100", 45, "000001010101" },
	{ 46, "00000101", 46, "000001010110" },
	{ 47, "00001010", 47, "000001010111" },
	{ 48, "00001011", 48, "000001100100" },
	{ 49, "01010010", 49, "000001100101" },
	{ 50, "01010011", 50, "000001010010" },
	{ 51, "01010100", 51, "000001010011" },
	{ 52, "01010101", 52, "000000100100" },
	{ 53, "00100100", 53, "000000110111" },
	{ 54, "00100101", 54, "000000111000" },
	{ 55, "01011000", 55, "000000100111" },
	{ 56, "01011001", 56, "000000101000" },
	{ 57, "01011010", 57, "000001011000" },
	{ 58, "01011011", 58, "000001011001" },
	{ 59, "01001010", 59, "000000101011" },
	{ 60, "01001011", 60, "000000101100" },
	{ 61, "00110010", 61, "000001011010" },
	{ 62, "00110011", 62, "000001100110" },
	{ 63, "00110100", 63, "000001100111" },
	{ 64, "11011", 64, "0000001111" },
	{ 128, "10010", 128, "000011001000" },
	{ 192, "010111", 192, "000011001001" },
	{ 256, "0110111", 256, "000001011011" },
	{ 320, "00110110", 320, "000000110011" },
	{ 384, "00110111", 384, "000000110100" },
	{ 448, "01100100", 448, "000000110101" },
	{ 512, "01100101", 512, "0000001101100" },
	{ 576, "01101000", 576, "0000001101101" },
	{ 640, "01100111", 640, "0000001001010" },
	{ 704, "011001100", 704, "0000001001011" },
	{ 768, "011001101", 768, "0000001001100" },
	{ 832, "011010010", 832, "0000001001101" },
	{ 896, "011010011", 896, "0000001110010" },
	{ 960, "011010100", 960, "0000001110011" },
	{ 1024, "011010101", 1024, "0000001110100" },
	{ 1088, "011010110", 1088, "0000001110101" },
	{ 1152, "011010111", 1152, "0000001110110" },
	{ 1216, "011011000", 1216, "0000001110111" },
	{ 1280, "011011001", 1280, "0000001010010" },
	{ 1344, "011011010", 1344, "0000001010011" },
	{ 1408, "011011011", 1408, "0000001010100" },
	{ 1472, "010011000", 1472, "0000001010101" },
	{ 1536, "010011001", 1536, "0000001011010" },
	{ 1600, "010011010", 1600, "0000001011011" },
	{ 1664, "011000", 1664, "0000001100100" },
	{ 1728, "010011011", 1728, "0000001100101" },
	{ EOL, "000000000001", EOL, "00000000000" },
	{ 1792, "00000001000", 1792, "00000001000" },
	{ 1856, "00000001100", 1856, "00000001100" },
	{ 1920, "00000001101", 1920, "00000001101" },
	{ 1984, "000000010010", 1984, "000000010010" },
	{ 2048, "000000010011", 2048, "000000010011" },
	{ 2112, "000000010100", 2112, "000000010100" },
	{ 2176, "000000010101", 2176, "000000010101" },
	{ 2240, "00000001011", 2240, "000000010110" },
	{ 2304, "000000010111", 2304, "000000010111" },
	{ 2368, "000000011100", 2368, "000000011100" },
	{ 2432, "000000011101", 2432, "000000011101" },
	{ 2496, "000000011110", 2496, "000000011110" },
	{ 2560, "000000011111", 2560, "000000011111" },
};

BYTE* ccittdecompress (BYTE* in, unsigned long count, unsigned long* Nret, int width, int height, bool eol)
{
	HUFFNODE* whitetree = NULL;
	HUFFNODE* blacktree = NULL;
	BSTREAM* bs = 0;
	BSTREAM* bout = 0;
	int i, ii;
	int err = 0;
	int totlen;
	int len;
	int whitelen, blacklen;
	BYTE* answer = 0;
	int Nout;

	try {
		Nout = (width + 7) / 8 * height;
		answer = new BYTE[Nout];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		bout = new BSTREAM (answer, Nout, BIG_ENDIAN);
		if (!bout) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}
		bs = new BSTREAM (in, count, LITTLE_ENDIAN);
		if (!bs) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		for (i = 0; i < 105; i++) {
			whitetree = HUFFNODE::addhuffmansymbol (whitetree, ccitttable[i].whitecode, ccitttable[i].whitelen, &err);
		}
		for (i = 0; i < 105; i++) {
			blacktree = HUFFNODE::addhuffmansymbol (blacktree, ccitttable[i].blackcode, ccitttable[i].blacklen, &err);
		}
		if (err) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}

		//debug(whitetree, 0);
		if (eol) {
			len = whitetree->gethuffmansymbol (bs);
		}
		for (i = 0; i < height; i++) {
			totlen = 0;
			while (totlen < width) {
				len = whitetree->gethuffmansymbol (bs);
				if (len == -1) {
					throw general_exception ("parse_error");  // 例外をスロー
				}
				if (len == -2) {
					whitelen = width - totlen;
				}
				else {
					whitelen = len;
				}
				while (len >= 64) {
					len = whitetree->gethuffmansymbol (bs);
					if (len == EOL || len == -1 || totlen + len + whitelen > width) {
						throw general_exception ("parse_error");  // 例外をスロー
					}
					whitelen += len;
				}
				for (ii = 0; ii < whitelen; ii++) {
					bout->writebit (0);
				}
				totlen += whitelen;
				if (totlen >= width) {
					break;
				}
				len = blacktree->gethuffmansymbol (bs);
				if (len < 0) {
					throw general_exception ("parse_error");  // 例外をスロー
				}
				blacklen = len;
				while (len >= 64) {
					len = blacktree->gethuffmansymbol (bs);
					if (len == EOL || len == -1 || totlen + len + blacklen > width) {
						throw general_exception ("parse_error");  // 例外をスロー
					}
					blacklen += len;
				}
				for (ii = 0; ii < blacklen; ii++) {
					bout->writebit (1);
				}
				totlen += blacklen;

			}
			//synch_to_byte(bs);
			if (width & 0x07) {
				for (ii = 0; ii < 8 - (width & 0x07); ii++) {
					bout->writebit (0);
				}
			}
			if (eol) {
				whitetree->gethuffmansymbol (bs);
			}
			///synch_to_byte(bs);
		}
		*Nret = Nout;
		delete whitetree;
		delete blacktree;
		delete bs;
		delete bout;
		return answer;
	}
	catch (general_exception) {
		// parse_error:
		// out_of_memory:
		delete whitetree;
		delete blacktree;
		delete bs;
		delete bout;
		delete[] answer;
		return 0;
	}
}

BYTE* ccittgroup4decompress (BYTE* in, unsigned long count, unsigned long* Nret, int width, int height, int eol)
{
	BYTE* reference = NULL;
	BYTE* current = NULL;
	BYTE* temp;
	int i;
	int a0;
	int a1;
	int a2;
	int a1span, a2span;
	int seg;
	int b1, b2;
	HUFFNODE* twodtree = 0;
	HUFFNODE* whitetree = 0;
	HUFFNODE* blacktree = 0;
	BSTREAM* bs = 0;
	BSTREAM* bout = 0;
	unsigned long Nout;
	BYTE* answer = NULL;
	int err = 0;
	CCITT mode;
	int colour;

	try {
		Nout = (width + 7) / 8 * height;
		answer = new BYTE[Nout];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}
		//debug memset
		memset (answer, 0, Nout);
		bout = new BSTREAM (answer, Nout, BIG_ENDIAN);
		if (!bout) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		bs = new BSTREAM (in, count, eol ? LITTLE_ENDIAN : BIG_ENDIAN);
		if (!bs) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}
		for (i = 0; i < 105; i++) {
			whitetree = HUFFNODE::addhuffmansymbol (whitetree, ccitttable[i].whitecode, ccitttable[i].whitelen, &err);
		}
		for (i = 0; i < 105; i++) {
			blacktree = HUFFNODE::addhuffmansymbol (blacktree, ccitttable[i].blackcode, ccitttable[i].blacklen, &err);
		}
		for (i = 0; i < 11; i++) {
			twodtree = HUFFNODE::addhuffmansymbol (twodtree, ccitt2dtable[i].code, static_cast<int>(ccitt2dtable[i].symbol), &err);
		}
		if (err) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}

		reference = new BYTE[width];
		for (i = 0; i < width; i++) {
			reference[i] = 0;
		}
		current = new BYTE[width];
		if (!current || !reference) {
			throw general_exception ("out_of_memory");  // 例外をスロー
		}


		if (eol) {
			//int len = 0;

			while (bs->getbit () == 0) {
				continue;
			}
		}

		for (i = 0; i < height; i++) {
			a0 = -1;
			colour = 0;
			while (a0 < width) {
				b1 = a0 + 1;
				while (b1 < width) {
					if (reference[b1] != colour && (b1 == 0 || reference[b1] != reference[b1 - 1])) {
						break;
					}
					b1++;
				}
				for (b2 = b1; b2 < width && reference[b2] == reference[b1]; b2++) {
				}

				if (a0 == -1) {
					a0 = 0;
				}

				mode = static_cast<CCITT>(twodtree->gethuffmansymbol (bs));

				a2 = -1;
				switch (mode) {
				case CCITT::CCITT_PASS:
					a1 = b2;
					break;
				case CCITT::CCITT_HORIZONTAL:
					a1span = 0;
					a2span = 0;
					if (colour == 0) {
						do {
							seg = whitetree->gethuffmansymbol (bs);
							a1span += seg;
						} while (seg >= 64 && a0 + a1span <= width);

						do {
							seg = blacktree->gethuffmansymbol (bs);
							a2span += seg;
						} while (seg >= 64 && a0 + a1span <= width);
					}
					else {
						do {
							seg = blacktree->gethuffmansymbol (bs);
							a1span += seg;
						} while (seg >= 64 && a0 + a1span <= width);

						do {
							seg = whitetree->gethuffmansymbol (bs);
							a2span += seg;
						} while (seg >= 64 && a0 + a1span + a2span <= width);
					}
					a1 = a0 + a1span;
					a2 = a1 + a2span;
					if (a1 > width) {
						//printf("Bad span1 %d span2 %d here\n", a1span, a2span);
						throw general_exception ("parse_error");  // 例外をスロー
					}
					break;
				case CCITT::CCITT_VERTICAL_0:
					a1 = b1;
					break;
				case CCITT::CCITT_VERTICAL_R1:
					a1 = b1 + 1;
					break;
				case CCITT::CCITT_VERTICAL_R2:
					a1 = b1 + 2;
					break;
				case CCITT::CCITT_VERTICAL_R3:
					a1 = b1 + 3;
					break;
				case CCITT::CCITT_VERTICAL_L1:
					a1 = b1 - 1;
					break;
				case CCITT::CCITT_VERTICAL_L2:
					a1 = b1 - 2;
					break;
				case CCITT::CCITT_VERTICAL_L3:
					a1 = b1 - 3;
					break;
				case CCITT::CCITT_ENDOFFAXBLOCK:
					*Nret = Nout;
					delete twodtree;
					delete whitetree;
					delete blacktree;
					delete[] reference;
					delete[] current;
					return answer;
				default:
					throw general_exception ("parse_error");  // 例外をスロー
				}

				if (a1 <= a0 && a0 != 0) {
					throw general_exception ("parse_error");  // 例外をスロー
				}
				if (a0 < 0 || a1 < 0) {
					throw general_exception ("parse_error");  // 例外をスロー
				}
				while (a0 < a1 && a0 < width) {
					current[a0++] = colour;
					bout->writebit (colour);
				}
				if (mode != CCITT::CCITT_PASS) {
					colour ^= 1;
				}
				if (a1 >= 0) {
					a0 = a1;
				}
				if (a2 != -1) {
					while (a0 < a2 && a0 < width) {
						current[a0++] = colour;
						bout->writebit (colour);
					}
					colour ^= 1;
				}
			}
			if (eol) {
				while (bs->getbit () == 0) {
					continue;
				}
			}
			temp = reference;
			reference = current;
			current = temp;
			if (width % 8) {
				while (a0 % 8) {
					bout->writebit (0);
					a0++;
				}
			}

		}
		*Nret = Nout;
		delete twodtree;
		delete whitetree;
		delete blacktree;
		delete[] reference;
		delete[] current;
		return answer;
	}
	catch (general_exception e) {
		if (strcmp (e.what (), "parse_error") == 0) {
			*Nret = Nout;
			return answer;
		}
		else if (strcmp (e.what (), "out_of_memory") == 0) {
			return NULL;
		}
	}
	return NULL;
}

/*
load the raster data
Params: out - return pointer for raster data, 0 for size run
fp - pointer to an open file
Nret - number of bytes read
Returns: 0 on success, -1 on fail.
*/
int loadlzw (BYTE* out, FileData* fd, unsigned long count, unsigned long* Nret)
{
	int codesize;
	//int block;
	int clear;
	int end;
	int nextcode;
	int codelen;
	BYTE* stream = 0;
	//int blen = 0;
	BSTREAM* bs;
	ENTRY* table = NULL;
	int pos = 0;
	int len;
	int second;
	int first;
	int tempcode;
	int ch;


	codesize = 8;

	clear = 1 << codesize;
	end = clear + 1;
	nextcode = end + 1;
	codelen = codesize + 1;
	try {
		stream = new BYTE[count];
		if (!stream) {
			return -1;
		}
		//fread(stream, count, 1, fp);
		fd->memcpy (stream, count);
		//memcpy(stream, fd->buffer + fd->buffer_ptr, count);
		//fd->buffer_ptr += count;

		table = new ENTRY[(1 << 12)];

		for (auto ii = 0; ii < nextcode; ii++) {
			table[ii].prefix = 0;
			table[ii].len = 1;
			table[ii].suffix = ii;
		}
		bs = new BSTREAM (stream, count, BIG_ENDIAN);

		first = bs->getbits (codelen);
		if (first != clear) {
			delete bs;
			bs = new BSTREAM (stream, count, LITTLE_ENDIAN);
			first = bs->getbits (codelen);
			if (first != clear) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
		}
		while (first == clear) {
			first = bs->getbits (codelen);
		}
		ch = first;
		if (out) {
			out[0] = ch;
		}
		pos = 1;

		while (1) {
			second = bs->getbits (codelen);
			if (second < 0) {
				throw general_exception ("parse_error");  // 例外をスロー
			}
			if (second == clear) {
				nextcode = end + 1;
				codelen = codesize + 1;
				first = bs->getbits (codelen);

				while (first == clear)
					first = bs->getbits (codelen);
				if (first == end)
					break;
				ch = first;
				if (out) {
					out[pos++] = first;
				}
				else {
					pos++;
				}
				continue;
			}
			if (second == end) {
				break;
			}

			if (second >= nextcode) {
				len = table[first].len;
				// if (len + pos >= width * height)
				// break;
				tempcode = first;
				for (auto ii = 0; ii < len; ii++) {
					if (out) {
						out[pos + len - ii - 1] = (BYTE)table[tempcode].suffix;
					}
					tempcode = table[tempcode].prefix;
				}
				if (out) {
					out[pos + len] = (BYTE)ch;
				}
				pos += len + 1;
			}
			else {
				len = table[second].len;
				// if (pos + len > width * height)
				// break;
				tempcode = second;

				for (auto ii = 0; ii < len; ii++) {
					ch = table[tempcode].suffix;
					if (out) {
						out[pos + len - ii - 1] = (BYTE)table[tempcode].suffix;
					}
					tempcode = table[tempcode].prefix;
				}
				pos += len;
			}

			if (nextcode < 4096) {
				table[nextcode].prefix = first;
				table[nextcode].len = table[first].len + 1;
				table[nextcode].suffix = ch;

				nextcode++;
				if (nextcode == (1 << codelen) - ((bs->endianness == BIG_ENDIAN) ? 1 : 0)) {
					codelen++;

					if (codelen == 13) {
						codelen = 12;
					}
				}
			}

			first = second;
		}


		delete[] table;
		delete[] stream;
		*Nret = pos;
		return 0;
	}
	catch (general_exception) {
		//parse_error:
		delete[] table;
		delete[] stream;
		return -1;
	}
}

/// <summary>
/// read a bit from the bitstream:
/// </summary>
/// <returns>the bit read</returns>
int BSTREAM::getbit ()
{
	int answer;

	if (pos >= N) {
		return -1;
	}
	answer = (data[pos] & bit) ? 1 : 0;

	if (endianness == ENDIAN::BIG_ENDIAN) {
		bit >>= 1;
		if (bit == 0) {
			bit = 128;
			pos++;
		}
	}
	else {
		bit <<= 1;
		if (bit == 0x100) {
			bit = 1;
			pos++;
		}
	}
	return answer;
}

/// <summary>
/// read several bits from a bitstream:
/// </summary>
/// <param name="nbits">number of bits to read</param>
/// <returns>the bits read (little-endian)</returns>
int BSTREAM::getbits (int nbits)
{
	int i;
	int answer = 0;

	if (endianness == BIG_ENDIAN) {
		for (i = 0; i < nbits; i++) {
			answer |= (getbit () << (nbits - i - 1));
		}
	}
	else {
		for (i = 0; i < nbits; i++) {
			answer |= (getbit () << i);
		}
	}

	return answer;
}
/// <summary>
/// 
/// </summary>
/// <returns></returns>
int BSTREAM::synch_to_byte ()
{
	if (endianness == BIG_ENDIAN) {
		while (bit != 0x80 && pos < N) {
			getbit ();
		}
	}
	else {
		while (bit != 0x01 && pos < N) {
			getbit ();
		}
	}
	return 0;
}
/// <summary>
/// 
/// </summary>
/// <param name="bit"></param>
/// <returns></returns>
int BSTREAM::writebit (int bit)
{
	if (pos >= N) {
		return -1;
	}
	if (bit) {
		data[pos] |= this->bit;
	}
	else {
		data[pos] &= (BYTE)~(this->bit);
	}
	getbit ();
	return 0;
}

/*
  sizeof() for a TIFF data type
  we default to 1
*/
int tiffsizeof (TAG_TYPE datatype)
{
	switch (datatype) {
	case TAG_TYPE::TAG_BYTE: return 1;
	case TAG_TYPE::TAG_ASCII: return 1;
	case TAG_TYPE::TAG_SHORT: return 2;
	case TAG_TYPE::TAG_LONG: return 4;
	case TAG_TYPE::TAG_RATIONAL: return 8;
	default:
		return 1;
	}
}

/*
  load tag header
  Params: type - big endian or litle endian
		  fp - file pointer
		  Ntags - return for number of tags
  Returns: the tags, 0 on error
*/
TAG* load_header (FileData* fd, int* Ntags)
{
	TAG* answer = NULL;
	int N = 0;
	int i;
	int err;

	try {
		N = fd->fget16u ();
		//printf("%d tags\n", N);
		answer = new TAG[N];//(TAG*)new char[N * sizeof(TAG)];
		if (!answer) {
			throw general_exception ("out_of_memory");  // 例外をスロー	
		}
		for (i = 0; i < N;i++) {
			answer[i].vector = 0;
			answer[i].ascii = 0;
		}

		for (i = 0; i < N; i++) {
			err = load_tags (&answer[i], fd);
			if (err) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			/*
			if (answer[i].datacount == 1)
				printf("tag %d %f\n", answer[i].tagid, answer[i].scalar);
			else if (answer[i].datatype == TAG_ASCII)
				printf("tag %d %s\n", answer[i].tagid, answer[i].ascii);
			else
				printf("\n");
				*/

		}
		*Ntags = N;
		return answer;
	}

	catch (general_exception) {
		//out_of_memory:
		killtags (answer, N);
		return 0;
	}
	catch (...) {
		killtags (answer, N);
		return 0;
	}
}

/*
  tags destructor
	Params: tags - items to destroy
			N - number of tags
*/
void killtags (TAG* tags, int N)
{
	int i;

	if (tags) {
		for (i = 0; i < N; i++) {
			delete[] tags[i].vector;
			delete[] tags[i].ascii;
		}
		delete[] tags;
	}
}

/*
  Load a tag froma file
	tag - the tag
	type - big endian or little endia
	fp - pointer to file
  Returns: 0 on success -1 on out of memory, -2 on parse error
*/
int load_tags (TAG* tag, FileData* fd)
{
	unsigned long offset;
	unsigned long pos;
	unsigned long num, denom;

	try {

		tag->tagid = static_cast<TID>(fd->fget16 ());
		tag->datatype = static_cast<TAG_TYPE>(fd->fget16 ());
		tag->datacount = fd->fget32 ();
		tag->vector = 0;
		tag->ascii = 0;
		tag->bad = 0;

		//printf("tag %d type %d N %ld ", tag->tagid, tag->datatype, tag->datacount);
		const unsigned long datasize = tag->datacount * tiffsizeof (tag->datatype);
		if (tag->datacount == 1) {
			switch (tag->datatype) {
			case TAG_TYPE::TAG_BYTE:
				tag->scalar = (double)fd->fgetcc ();
				fd->fgetcc ();
				fd->fgetcc ();
				fd->fgetcc ();
				break;
			case TAG_TYPE::TAG_ASCII:
				if (datasize > 4) {
					offset = fd->fget32 ();
					//pos = ftell(fp);
					pos = fd->buffer_ptr;
					//fseek(fp, offset, SEEK_SET);
					fd->buffer_ptr = offset;
				}
				tag->ascii = fread_asciiz (fd);
				if (datasize > 4) {
					//fseek(fp, pos, SEEK_SET);
					fd->buffer_ptr = pos;
				}
				break;
			case TAG_TYPE::TAG_SHORT:
				tag->scalar = (double)fd->fget16u ();
				fd->fgetcc ();
				fd->fgetcc ();
				break;
			case TAG_TYPE::TAG_LONG:
				tag->scalar = (double)fd->fget32u ();
				break;
			case TAG_TYPE::TAG_RATIONAL:
				offset = fd->fget32u ();
				//pos = ftell(fp);
				pos = fd->buffer_ptr;
				//fseek(fp, offset, SEEK_SET);
				fd->buffer_ptr = offset;
				num = fd->fget32u ();
				denom = fd->fget32u ();
				if (denom) {
					tag->scalar = ((double)num) / denom;
				}
				//fseek(fp, pos, SEEK_SET);
				fd->buffer_ptr = pos;
				break;
			default:
				tag->bad = -1;
				break;
			}
		}
		else {
			if (datasize > 4) {
				offset = fd->fget32 ();
				//pos = ftell(fp);
				pos = fd->buffer_ptr;
				//fseek(fp, offset, SEEK_SET);
				fd->buffer_ptr = offset;
			}
			switch (tag->datatype) {
			case TAG_TYPE::TAG_BYTE:
				tag->vector = new char[datasize];
				if (!tag->vector) {
					throw general_exception ("out_of_memory");  // 例外をスロー
				}
				//fread(tag->vector, 1, datasize, fp);
				fd->memcpy (tag->vector, datasize);
				//memcpy(tag->vector, fd->buffer + fd->buffer_ptr, datasize);
				//fd->buffer_ptr += datasize;
				break;
			case TAG_TYPE::TAG_ASCII:
				tag->ascii = fread_asciiz (fd);
				if (!tag->ascii) {
					throw general_exception ("out_of_memory");  // 例外をスロー	
				}
				break;
			case TAG_TYPE::TAG_SHORT:
				tag->vector = new char[tag->datacount * sizeof (short)];
				if (!tag->vector) {
					throw general_exception ("out_of_memory");  // 例外をスロー
				}
				for (auto i = 0LU; i < tag->datacount; i++)
					(static_cast<unsigned short*>(tag->vector))[i] = fd->fget16u ();
				break;
			case TAG_TYPE::TAG_LONG:
				tag->vector = new char[tag->datacount * sizeof (long)];
				if (!tag->vector) {
					throw general_exception ("out_of_memory");  // 例外をスロー	
				}
				for (auto i = 0LU; i < tag->datacount; i++)
					(static_cast<unsigned long*>(tag->vector))[i] = fd->fget32u ();
				break;
			case TAG_TYPE::TAG_RATIONAL:
				tag->vector = new char[tag->datacount * sizeof (double)];
				if (!tag->vector) {
					throw general_exception ("out_of_memory");  // 例外をスロー	
				}
				for (auto i = 0LU; i < tag->datacount; i++) {
					num = fd->fget32u ();
					denom = fd->fget32u ();
					if (denom)
						(static_cast<double*>(tag->vector))[i] = ((double)num) / denom;
				}
				break;
			default:
				fd->fgetcc ();
				fd->fgetcc ();
				fd->fgetcc ();
				fd->fgetcc ();
				tag->bad = -1;
			}
			if (datasize > 4) {
				//fseek(fp, pos, SEEK_SET);
				fd->buffer_ptr = pos;
			}
		}

		//if (feof(fp))
		//	return -2;
		if (fd->buffer_ptr >= fd->size) {
			return -2;
		}
		return 0;
	}
	catch (general_exception) {
		//out_of_memory:
		tag->bad = -1;
		return -1;

	}
}

/// <summary>
/// read an entry for a tag
/// </summary>
/// <param name="tag">the tag read in</param>
/// <param name="index">index of data item in tag</param>
/// <returns>value (as double)</returns>
double tag_get_entry (TAG* tag, unsigned long index)
{
	if (index >= tag->datacount) {
		return -1.0;
	}
	if (tag->datacount == 1) {
		return tag->scalar;
	}
	if (tag->bad) {
		return -1;
	}
	switch (tag->datatype) {
	case TAG_TYPE::TAG_BYTE:
		return (double)(static_cast<BYTE*>(tag->vector))[index];
	case TAG_TYPE::TAG_ASCII:
		return (double)(static_cast<char*>(tag->vector))[index];
	case TAG_TYPE::TAG_SHORT:
		return (double)(static_cast<unsigned short*>(tag->vector))[index];
	case TAG_TYPE::TAG_LONG:
		return (double)(static_cast<unsigned long*>(tag->vector))[index];
	case TAG_TYPE::TAG_RATIONAL:
		return static_cast<double*>(tag->vector)[index];
	default:
		return -1;
	}
}

/// <summary>
/// safe paste function
/// </summary>
/// <param name="rgba">destination buffer</param>
/// <param name="width">buffer dimensions. width</param>
/// <param name="height">buffer dimensions. height</param>
/// <param name="tile">the tile to paste</param>
/// <param name="twidth">tile width</param>
/// <param name="theight">tile height</param>
/// <param name="x"> x co-ordinates to paste</param>
/// <param name="y"> y co-ordinates to paste</param>
void rgbapaste (BYTE* rgba, int width, int height, const BYTE* tile, int twidth, int theight, int x, int y)
{
	for (auto ty = 0; ty < theight; ty++) {
		auto iy = y + ty;
		if (iy < 0) {
			continue;
		}
		if (iy >= height) {
			break;
		}
		for (auto tx = 0; tx < twidth; tx++) {
			auto ix = x + tx;
			if (ix < 0) {
				continue;
			}
			if (ix >= width) {
				break;
			}

			rgba[(iy * width + ix) * 4 + 0] = tile[(ty * twidth + tx) * 4 + 0];
			rgba[(iy * width + ix) * 4 + 1] = tile[(ty * twidth + tx) * 4 + 1];
			rgba[(iy * width + ix) * 4 + 2] = tile[(ty * twidth + tx) * 4 + 2];
			rgba[(iy * width + ix) * 4 + 3] = tile[(ty * twidth + tx) * 4 + 3];
		}

	}

}

/// <summary>
/// safe paste function
/// </summary>
/// <param name="grey">destination buffer</param>
/// <param name="width">buffer dimensions</param>
/// <param name="height">buffer dimensions</param>
/// <param name="tile">the tile to paste</param>
/// <param name="twidth">tile width</param>
/// <param name="theight">tile height</param>
/// <param name="x">x co-ordinates to paste</param>
/// <param name="y">y co-ordinates to paste</param>
void greypaste (BYTE* grey, int width, int height, const BYTE* tile, int twidth, int theight, int x, int y)
{
	for (auto ty = 0; ty < theight; ty++) {
		auto iy = y + ty;
		if (iy < 0) {
			continue;
		}
		if (iy >= height) {
			break;
		}
		for (auto tx = 0; tx < twidth; tx++) {
			auto ix = x + tx;
			if (ix < 0) {
				continue;
			}
			if (ix >= width) {
				break;
			}

			grey[(iy * width + ix)] = tile[(ty * twidth + tx)];
		}

	}

}

/// <summary>
/// 
/// </summary>
/// <param name="buff"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="depth"></param>
/// <param name="tile"></param>
/// <param name="twidth"></param>
/// <param name="theight"></param>
/// <param name="tdepth"></param>
/// <param name="x"></param>
/// <param name="y"></param>
void pasteflexible (BYTE* buff, int width, int height, int depth, const BYTE* tile, int twidth, int theight, int tdepth, int x, int y)
{
	auto mindepth = depth < tdepth ? depth : tdepth;
	for (auto ty = 0;ty < theight;ty++) {
		auto iy = y + ty;
		if (iy < 0 || iy >= height) {
			continue;
		}
		for (auto tx = 0; tx < twidth; tx++) {
			auto ix = x + tx;
			if (ix < 0) {
				continue;
			}
			if (ix >= width) {
				break;
			}

			for (auto isample = 0;isample < mindepth;isample++) {
				buff[(iy * width + ix) * depth + isample] = tile[(ty * twidth + tx) * tdepth + isample];
			}
		}
	}
}

/// <summary>
/// read asciiz
/// </summary>
/// <param name="fd"></param>
/// <returns></returns>
char* fread_asciiz (FileData* fd)
{
	char* buff;
	char* temp;
	size_t bufflen = 64;
	size_t N = 0;
	int ch;

	buff = new char[bufflen];//(char*)malloc(bufflen);
	while ((ch = fd->fgetcc ()) != EOF) {
		buff[N++] = ch;
		if (N == bufflen) {
			if ((bufflen + bufflen / 2) < bufflen) {
				throw general_exception ("out_of_memory");  // 例外をスロー	
			}
			temp = new char[bufflen + bufflen / 2];
			//temp = (char*)realloc(buff, bufflen + bufflen / 2);
			if (!temp) {
				throw general_exception ("out_of_memory");  // 例外をスロー
			}
			// 旧領域から転送
			memcpy (temp, buff, bufflen);
			// 旧領域を削除
			delete[] buff;
			buff = temp;
			bufflen = bufflen + bufflen / 2;
		}
		if (ch == 0) {
			temp = new char[N];
			if (temp) {
				memcpy (temp, buff, N);
				delete[] buff;
				return temp;
			}
			//return (char*)realloc(buff, N);
		}
	}
	delete[] buff;
	return 0;
}

/// <summary>
/// read a double from a stream in ieee754 format regardless of host encoding.
/// </summary>
/// <param name="buff"></param>
/// <param name="bigendian">set to if big bytes first, clear for little bytes first</param>
/// <returns></returns>
double memread_ieee754 (BYTE* buff, int bigendian)
{
	int i;
	double fnorm = 0.0;
	BYTE temp;
	int sign;
	int exponent;
	double bitval;
	int maski, mask;
	int expbits = 11;
	int significandbits = 52;
	int shift;
	double answer;

	/* just reverse if not big-endian*/
	if (!bigendian) {
		for (i = 0; i < 4; i++) {
			temp = buff[i];
			buff[i] = buff[8 - i - 1];
			buff[8 - i - 1] = temp;
		}
	}
	sign = buff[0] & 0x80 ? -1 : 1;
	/* exponet in raw format*/
	exponent = ((buff[0] & 0x7F) << 4) | ((buff[1] & 0xF0) >> 4);

	/* read inthe mantissa. Top bit is 0.5, the successive bits half*/
	bitval = 0.5;
	maski = 1;
	mask = 0x08;
	for (i = 0; i < significandbits; i++) {
		if (buff[maski] & mask)
			fnorm += bitval;

		bitval /= 2.0;
		mask >>= 1;
		if (mask == 0) {
			mask = 0x80;
			maski++;
		}
	}
	/* handle zero specially */
	if (exponent == 0 && fnorm == 0)
		return 0.0;

	shift = exponent - ((1 << (expbits - 1)) - 1); /* exponent = shift + bias */
	/* nans have exp 1024 and non-zero mantissa */
	if (shift == 1024 && fnorm != 0) {
		return sqrt (-1.0);
	}
	/*infinity*/
	if (shift == 1024 && fnorm == 0) {

#ifdef INFINITY
		return sign == 1 ? INFINITY : -INFINITY;
#else
		return	(sign * 1.0) / 0.0;
#endif
	}
	if (shift > -1023) {
		answer = ldexp (fnorm + 1.0, shift);
		return answer * sign;
	}
	else {
		/* denormalised numbers */
		if (fnorm == 0.0)
			return 0.0;
		shift = -1022;
		while (fnorm < 1.0) {
			fnorm *= 2;
			shift--;
		}
		answer = ldexp (fnorm, shift);
		return answer * sign;
	}
}

/// <summary>
/// 
/// </summary>
/// <param name="mem"></param>
/// <param name="bigendian"></param>
/// <returns></returns>
float memread_ieee754f (const BYTE* mem, int bigendian)
{
	unsigned long buff = 0;
	//unsigned long buff2 = 0;
	unsigned long mask;
	int sign;
	int exponent;
	int shift;
	int i;
	int significandbits = 23;
	int expbits = 8;
	double fnorm = 0.0;
	double bitval;
	double answer;

	if (bigendian) {
		buff = (mem[0] << 24) | (mem[1] << 16) | (mem[2] << 8) | mem[3];
	}
	else {
		buff = (mem[3] << 24) | (mem[2] << 16) | (mem[1] << 8) | mem[0];
	}

	sign = (buff & 0x80000000) ? -1 : 1;
	mask = 0x00400000;
	exponent = (buff & 0x7F800000) >> 23;
	bitval = 0.5;
	for (i = 0;i < significandbits;i++) {
		if (buff & mask)
			fnorm += bitval;
		bitval /= 2;
		mask >>= 1;
	}
	if (exponent == 0 && fnorm == 0.0)
		return 0.0f;
	shift = exponent - ((1 << (expbits - 1)) - 1); /* exponent = shift + bias */

	if (shift == 128 && fnorm != 0.0)
		return (float)sqrt (-1.0);
	if (shift == 128 && fnorm == 0.0) {
#ifdef INFINITY
		return sign == 1 ? INFINITY : -INFINITY;
#else
		return (sign * 1.0f) / 0.0f;
#endif
	}
	if (shift > -127) {
		answer = ldexp (fnorm + 1.0, shift);
		return (float)answer * sign;
	}
	else {
		if (fnorm == 0.0) {
			return 0.0f;
		}
		shift = -126;
		while (fnorm < 1.0) {
			fnorm *= 2;
			shift--;
		}
		answer = ldexp (fnorm, shift);
		return (float)answer * sign;
	}
}

int YcbcrToRGB (int Y, int cb, int cr, BYTE* red, BYTE* green, BYTE* blue)
{
	double r, g, b;

	r = (Y - 16) * 1.164 + (cr - 128) * 1.596;
	g = (Y - 16) * 1.164 + (cb - 128) * -0.392 + (cr - 128) * -0.813;
	b = (Y - 16) * 1.164 + (cb - 128) * 2.017;

	*red = r < 0.0 ? 0 : r > 255 ? 255 : (BYTE)r;
	*green = g < 0.0 ? 0 : g > 255 ? 255 : (BYTE)g;
	*blue = b < 0.0 ? 0 : b > 255 ? 255 : (BYTE)b;

	return 0;
}



/*

This section is a zlib decompressor written by Lode Vandevenne as part of his
PNG loader. Lode's asked for the following notice to be retained in derived
version of his software, so please don't remove.

The only change I've made is to separate out the fucntions and mark the
public ones as (Malcolm)

LodePNG version 20120729

Copyright (c) 2005-2012 Lode Vandevenne

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

/// Stream
//#define READBIT(bitpointer, bitstream) ((bitstream[bitpointer >> 3] >> (bitpointer & 0x7)) & (BYTE)1)
int READBIT (size_t bitpointer, const BYTE* bitstream)
{
	return ((bitstream[bitpointer >> 3] >> (bitpointer & 0x7)) & (BYTE)1);
}

BYTE readBitFromStream (size_t* bitpointer, const BYTE* bitstream)
{
	BYTE result = (BYTE)(READBIT (*bitpointer, bitstream));
	(*bitpointer)++;
	return result;
}

unsigned readBitsFromStream (size_t* bitpointer, const BYTE* bitstream, size_t nbits)
{
	unsigned result = 0, i;
	for (i = 0; i < nbits; i++) {
		result += ((unsigned)READBIT (*bitpointer, bitstream)) << i;
		(*bitpointer)++;
	}
	return result;
}


/* ////////////////////////////////////////////////////////////////////////// */
/* / Adler32                                                                  */
/* ////////////////////////////////////////////////////////////////////////// */

/* ////////////////////////////////////////////////////////////////////////// */

unsigned lodepng_read32bitInt (const BYTE* buffer)
{
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}
unsigned update_adler32 (unsigned adler, const BYTE* data, unsigned len)
{
	unsigned s1 = adler & 0xffff;
	unsigned s2 = (adler >> 16) & 0xffff;

	while (len > 0) {
		/*at least 5550 sums can be done before the sums overflow, saving a lot of module divisions*/
		unsigned amount = len > 5550 ? 5550 : len;
		len -= amount;
		while (amount > 0) {
			s1 = (s1 + *data++);
			s2 = (s2 + s1);
			amount--;
		}
		s1 %= 65521;
		s2 %= 65521;
	}

	return (s2 << 16) | s1;
}

/*Return the adler32 of the bytes data[0..len-1]*/
unsigned adler32 (const BYTE* data, unsigned len)
{
	return update_adler32 (1L, data, len);
}

/* ////////////////////////////////////////////////////////////////////////// */
/* / Deflate - Huffman                                                      / */
/* ////////////////////////////////////////////////////////////////////////// */
#if 0
typedef struct LodePNGDecompressSettings
{
	unsigned ignore_adler32; /*if 1, continue and don't give an error message if the Adler32 checksum is corrupted*/
	unsigned custom_decoder; /*use custom decoder if LODEPNG_CUSTOM_ZLIB_DECODER and LODEPNG_COMPILE_ZLIB are enabled*/
} LodePNGDecompressSettings;
#endif

#define ERROR_BREAK(c) exit(0)

//#define FIRST_LENGTH_CODE_INDEX 257
//#define LAST_LENGTH_CODE_INDEX 285
//#define NUM_DEFLATE_CODE_SYMBOLS 288
//#define NUM_DISTANCE_SYMBOLS 32
//#define NUM_CODE_LENGTH_CODES 19
const int FIRST_LENGTH_CODE_INDEX = 257;
const int LAST_LENGTH_CODE_INDEX = 285;

/*256 literals, the end code, some length codes, and 2 unused codes*/
const int NUM_DEFLATE_CODE_SYMBOLS = 288;
/*the distance codes have their own symbols, 30 used, 2 unused*/
const int NUM_DISTANCE_SYMBOLS = 32;
/*the code length codes. 0-15: code lengths, 16: copy previous 3-6 times, 17: 3-10 zeros, 18: 11-138 zeros*/
const int NUM_CODE_LENGTH_CODES = 19;

/*the base lengths represented by codes 257-285*/
const unsigned LENGTHBASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,67, 83, 99, 115, 131, 163, 195, 227, 258 };

/*the extra bits used by codes 257-285 (added to base length)*/
const unsigned LENGTHEXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,4, 4, 4, 4, 5, 5, 5, 5, 0 };

/*the base backwards distances (the bits of distance codes appear after length codes and use their own huffman tree)*/
const unsigned DISTANCEBASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };

/*the extra bits of backwards distances (added to base)*/
const unsigned DISTANCEEXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

/*the order in which "code length alphabet code lengths" are stored, out of this
the huffman tree of the dynamic huffman tree lengths is generated*/
const unsigned CLCL_ORDER[NUM_CODE_LENGTH_CODES] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

/* ////////////////////////////////////////////////////////////////////////// */
#include <iostream>
/// <summary>
/// Huffman tree struct, containing multiple representations of the tree
/// </summary>
class HuffmanTree
{
public:
	unsigned* tree2d;
	unsigned* tree1d;
	unsigned* lengths; /*the lengths of the codes of the 1d-tree*/
	unsigned maxbitlen; /*maximum number of bits a single code can get*/
	unsigned numcodes; /*number of symbols in the alphabet = number of codes*/
	//void HuffmanTree_init (HuffmanTree* tree);
	//void HuffmanTree_cleanup (HuffmanTree* tree)
	/// <summary>
	/// constructor
	/// </summary>
	HuffmanTree ()
	{
		this->tree2d = NULL;
		this->tree1d = NULL;
		this->lengths = NULL;
		maxbitlen = 0;
		numcodes = 0;
	}

	/// <summary>
	/// destructor
	/// </summary>
	~HuffmanTree ()
	{
		delete[] this->tree2d;
		delete[] this->tree1d;
		delete[] this->lengths;
	}

	///// <summary>
	///// function used for debug purposes to draw the tree in ascii art with C++
	///// </summary>
	//void HuffmanTree_draw ()
	//{
	//	std::cout << "tree. length: " << this->numcodes << " maxbitlen: " << this->maxbitlen << std::endl;
	//	for (size_t i = 0; i < this->tree1d->size; i++) {
	//		if (this->lengths->data[i])
	//			std::cout << i << " " << this->tree1d->data[i] << " " << this->lengths->data[i] << std::endl;
	//	}
	//	std::cout << std::endl;
	//}

};


unsigned getTreeInflateDynamic (HuffmanTree* tree_ll, HuffmanTree* tree_d, const BYTE* in, size_t* bp, size_t inlength);
unsigned generateFixedLitLenTree (HuffmanTree* tree);
unsigned generateFixedDistanceTree (HuffmanTree* tree);

;
void getTreeInflateFixed (HuffmanTree* tree_ll, HuffmanTree* tree_d);
unsigned huffmanTree_make2DTree (HuffmanTree* tree);
unsigned huffmanTree_make_from_lengths (HuffmanTree* tree, const unsigned* bitlen, size_t numcodes, unsigned maxbitlen);
unsigned huffman_decode_symbol (const BYTE* in, size_t* bp, const HuffmanTree* codetree, size_t inbitlength);

/* /////////////////////////////////////////////////////////////////////////// */

/*dynamic vector of unsigned chars*/
class ucvector
{
public:
	BYTE* data;
	size_t size; /*used size*/
	size_t allocsize; /*allocated size*/
	//unsigned inflateNoCompression (ucvector* out, const BYTE* in, size_t* bp, size_t* pos, size_t inlength);
	//unsigned inflateHuffmanBlock (ucvector* out, const BYTE* in, size_t* bp,size_t* pos, size_t inlength, unsigned btype);

	/// <summary>
	/// constructor
	/// </summary>
	ucvector ()
	{
		this->data = NULL;
		this->size = this->allocsize = 0;
	}

	/// <summary>
	/// destructor
	/// </summary>
	~ucvector ()
	{
		this->size = 0;
		this->allocsize = 0;
		delete[] this->data;
		this->data = NULL;
	}

	/// <summary>
	/// resize
	/// </summary>
	/// <param name="size">new size</param>
	/// <returns>returns 1 if success, 0 if failure ==> nothing done</returns>
	unsigned ucvector_resize (size_t size)
	{
		if (size * sizeof (BYTE) > this->allocsize) {
			size_t new_size = size * sizeof (BYTE) * 2;
			// 領域確保
			void* new_data = new char[new_size] {};
			if (new_data) {
				// 旧領域からコピー
				memcpy (data, this->data, this->allocsize);
				// 旧領域削除
				delete[] this->data;
				this->allocsize = new_size;
				this->data = static_cast<BYTE*>(new_data);
				this->size = size;
			}
			else {
				return 0; /*error: not enough memory*/
			}
		}
		else {
			this->size = size;
		}
		return 1;
	}

	/// <summary>
	/// you can both convert from vector to buffer&size and vica versa.
	/// If you use init_buffer to take over a buffer and size, it is not needed to use cleanup
	/// </summary>
	/// <param name="buffer">set buffer data(tobe deleted)</param>
	/// <param name="size">initial size</param>
	void ucvector_init_buffer (BYTE* buffer, size_t size)
	{
		this->data = buffer;
		this->allocsize = size;
		this->size = size;
	}

	/// <summary>
	/// inflate no compression
	/// </summary>
	/// <param name="in"></param>
	/// <param name="bp"></param>
	/// <param name="pos"></param>
	/// <param name="inlength"></param>
	/// <returns></returns>
	unsigned inflateNoCompression (const BYTE* in, size_t* bp, size_t* pos, size_t inlength)
	{
		/*go to first boundary of byte*/
		size_t p;
		unsigned LEN, NLEN, n, error = 0;
		while (((*bp) & 0x7) != 0) (*bp)++;
		p = (*bp) / 8; /*byte position*/

		/*read LEN (2 bytes) and NLEN (2 bytes)*/
		if (p >= inlength - 4) return 52; /*error, bit pointer will jump past memory*/
		LEN = in[p] + 256 * in[p + 1]; p += 2;
		NLEN = in[p] + 256 * in[p + 1]; p += 2;

		/*check if 16-bit NLEN is really the one's complement of LEN*/
		if (LEN + NLEN != 65535) return 21; /*error: NLEN is not one's complement of LEN*/

		if ((*pos) + LEN >= this->size) {
			if (!ucvector_resize ((*pos) + LEN)) {
				return 83; /*alloc fail*/
			}
		}

		/*read the literal data: LEN bytes are now stored in the out buffer*/
		if (p + LEN > inlength) return 23; /*error: reading outside of in buffer*/
		for (n = 0; n < LEN; n++) this->data[(*pos)++] = in[p++];

		(*bp) = p * 8;

		return error;
	}

	/// <summary>
	/// inflate a block with dynamic of fixed Huffman tree
	/// </summary>
	/// <param name="in"></param>
	/// <param name="bp"></param>
	/// <param name="pos"></param>
	/// <param name="inlength"></param>
	/// <param name="btype"></param>
	/// <returns></returns>
	unsigned inflateHuffmanBlock (const BYTE* in, size_t* bp, size_t* pos, size_t inlength, unsigned btype)
	{
		unsigned error = 0;
		HuffmanTree* tree_ll = new HuffmanTree (); /*the huffman tree for literal and length codes*/
		HuffmanTree* tree_d = new HuffmanTree (); /*the huffman tree for distance codes*/
		size_t inbitlength = inlength * 8;


		if (btype == 1) getTreeInflateFixed (tree_ll, tree_d);
		else if (btype == 2) {
			error = getTreeInflateDynamic (tree_ll, tree_d, in, bp, inlength);
		}

		while (!error) /*decode all symbols until end reached, breaks at end code*/
		{
			/*code_ll is literal, length or end code*/
			unsigned code_ll = huffman_decode_symbol (in, bp, tree_ll, inbitlength);
			if (code_ll <= 255) /*literal symbol*/
			{
				if ((*pos) >= this->size) {
					/*reserve more room at once*/
					if (!this->ucvector_resize (((*pos) + 1) * 2)) {
						ERROR_BREAK (83 /*alloc fail*/);
					}
				}
				this->data[(*pos)] = (BYTE)(code_ll);
				(*pos)++;
			}
			else if (code_ll >= FIRST_LENGTH_CODE_INDEX && code_ll <= LAST_LENGTH_CODE_INDEX) /*length code*/
			{
				unsigned code_d, distance;
				unsigned numextrabits_l, numextrabits_d; /*extra bits for length and distance*/
				size_t start, forward, backward, length;

				/*part 1: get length base*/
				length = LENGTHBASE[code_ll - FIRST_LENGTH_CODE_INDEX];

				/*part 2: get extra bits and add the value of that to length*/
				numextrabits_l = LENGTHEXTRA[code_ll - FIRST_LENGTH_CODE_INDEX];
				if (*bp >= inbitlength) {
					ERROR_BREAK (51); /*error, bit pointer will jump past memory*/
				}
				length += readBitsFromStream (bp, in, numextrabits_l);

				/*part 3: get distance code*/
				code_d = huffman_decode_symbol (in, bp, tree_d, inbitlength);
				if (code_d > 29) {
					if (code_ll == (unsigned)(-1)) /*huffman_decode_symbol returns (unsigned)(-1) in case of error*/
					{
						/*return error code 10 or 11 depending on the situation that happened in huffman_decode_symbol
						(10=no endcode, 11=wrong jump outside of tree)*/
						error = (*bp) > inlength * 8 ? 10 : 11;
					}
					else error = 18; /*error: invalid distance code (30-31 are never used)*/
					break;
				}
				distance = DISTANCEBASE[code_d];

				/*part 4: get extra bits from distance*/
				numextrabits_d = DISTANCEEXTRA[code_d];
				if (*bp >= inbitlength) {
					ERROR_BREAK (51); /*error, bit pointer will jump past memory*/
				}

				distance += readBitsFromStream (bp, in, numextrabits_d);

				/*part 5: fill in all the out[n] values based on the length and dist*/
				start = (*pos);
				if (distance > start) {
					ERROR_BREAK (52); /*too long backward distance*/
				}
				backward = start - distance;
				if ((*pos) + length >= this->size) {
					/*reserve more room at once*/
					if (!this->ucvector_resize (((*pos) + length) * 2)) {
						ERROR_BREAK (83 /*alloc fail*/);
					}
				}

				for (forward = 0; forward < length; forward++) {
					this->data[(*pos)] = this->data[backward];
					(*pos)++;
					backward++;
					if (backward >= start) backward = start - distance;
				}
			}
			else if (code_ll == 256) {
				break; /*end code, break the loop*/
			}
			else /*if(code == (unsigned)(-1))*/ /*huffman_decode_symbol returns (unsigned)(-1) in case of error*/
			{
				/*return error code 10 or 11 depending on the situation that happened in huffman_decode_symbol
				(10=no endcode, 11=wrong jump outside of tree)*/
				error = (*bp) > inlength * 8 ? 10 : 11;
				break;
			}
		}

		delete tree_ll;
		delete tree_d;

		return error;
	}
};




unsigned lodepng_inflate (BYTE** out, size_t* outsize, const BYTE* in, size_t insize, const LodePNGDecompressSettings* settings);

unsigned lodepng_inflatev (ucvector* out, const BYTE* in, size_t insize, const LodePNGDecompressSettings* settings);


/// <summary>
/// dynamic vector of unsigned ints
/// </summary>
class uivector
{
public:
	unsigned* data;
	size_t size; /*size in number of unsigned longs*/
	size_t allocsize; /*allocated size in bytes*/
	/// <summary>
	/// constructor
	/// </summary>
	uivector ()
	{
		this->data = NULL;
		this->size = 0;
		this->allocsize = 0;
	}

	/// <summary>
	/// destructor
	/// </summary>
	~uivector ()
	{
		this->size = 0;
		this->allocsize = 0;
		delete[] this->data;
		this->data = NULL;
	}

	/// <summary>
	/// resize and give all new elements the value
	/// </summary>
	/// <param name="size">new size</param>
	/// <param name="value">new default value</param>
	/// <returns>1 if success.</returns>
	unsigned uivector_resizev (size_t size, unsigned value)
	{
		size_t oldsize = this->size;
		if (!uivector_resize (size)) {
			return 0;
		}
		for (auto i = oldsize; i < size; i++) {
			this->data[i] = value;
		}
		return 1;
	}


	/// <summary>
	/// resize
	/// </summary>
	/// <param name="size">new size</param>
	/// <returns>returns 1 if success, 0 if failure ==> nothing done</returns>
	unsigned uivector_resize (size_t size)
	{
		if (size * sizeof (unsigned) > this->allocsize) {
			size_t new_size = size * sizeof (unsigned) * 2;
			// 領域取得
			void* new_data = new char[new_size];
			if (new_data) {
				// 旧領域からコピー
				memcpy (data, this->data, this->allocsize);
				// 削除
				delete[] this->data;
				this->allocsize = new_size;
				this->data = static_cast<unsigned*>(new_data);
				this->size = size;
			}
			else {
				return 0;
			}
		}
		else {
			this->size = size;
		}
		return 1;
	}


};


/*get the tree of a deflated block with fixed tree, as specified in the deflate specification*/
void getTreeInflateFixed (HuffmanTree* tree_ll, HuffmanTree* tree_d)
{
	/*TODO: check for out of memory errors*/
	generateFixedLitLenTree (tree_ll);
	generateFixedDistanceTree (tree_d);
}

/*get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree*/
unsigned getTreeInflateDynamic (HuffmanTree* tree_ll, HuffmanTree* tree_d,
								const BYTE* in, size_t* bp, size_t inlength)
{
	/*make sure that length values that aren't filled in will be 0, or a wrong tree will be generated*/
	unsigned error = 0;
	unsigned n, HLIT, HDIST, HCLEN, i;
	size_t inbitlength = inlength * 8;

	/*see comments in deflateDynamic for explanation of the context and these variables, it is analogous*/
	unsigned* bitlen_ll = 0; /*lit,len code lengths*/
	unsigned* bitlen_d = 0; /*dist code lengths*/
	/*code length code lengths ("clcl"), the bit lengths of the huffman tree used to compress bitlen_ll and bitlen_d*/
	unsigned* bitlen_cl = 0;
	HuffmanTree* tree_cl = new HuffmanTree (); /*the code tree for code length codes (the huffman tree for compressed huffman trees)*/

	if ((*bp) >> 3 >= inlength - 2) return 49; /*error: the bit pointer is or will go past the memory*/

	/*number of literal/length codes + 257. Unlike the spec, the value 257 is added to it here already*/
	HLIT = readBitsFromStream (bp, in, 5) + 257;
	/*number of distance codes. Unlike the spec, the value 1 is added to it here already*/
	HDIST = readBitsFromStream (bp, in, 5) + 1;
	/*number of code length codes. Unlike the spec, the value 4 is added to it here already*/
	HCLEN = readBitsFromStream (bp, in, 4) + 4;

	while (!error) {
		/*read the code length codes out of 3 * (amount of code length codes) bits*/

		bitlen_cl = new unsigned[NUM_CODE_LENGTH_CODES];
		if (!bitlen_cl) {
			ERROR_BREAK (83 /*alloc fail*/);
		}

		for (i = 0; i < NUM_CODE_LENGTH_CODES; i++) {
			if (i < HCLEN) bitlen_cl[CLCL_ORDER[i]] = readBitsFromStream (bp, in, 3);
			else bitlen_cl[CLCL_ORDER[i]] = 0; /*if not, it must stay 0*/
		}

		error = huffmanTree_make_from_lengths (tree_cl, bitlen_cl, NUM_CODE_LENGTH_CODES, 7);
		if (error) break;

		/*now we can use this tree to read the lengths for the tree that this function will return*/
		bitlen_ll = new unsigned[NUM_DEFLATE_CODE_SYMBOLS];
		bitlen_d = new unsigned[NUM_DISTANCE_SYMBOLS];
		if (!bitlen_ll || !bitlen_d) {
			ERROR_BREAK (83 /*alloc fail*/);
		}
		for (i = 0; i < NUM_DEFLATE_CODE_SYMBOLS; i++) {
			bitlen_ll[i] = 0;
		}
		for (i = 0; i < NUM_DISTANCE_SYMBOLS; i++) {
			bitlen_d[i] = 0;
		}

		/*i is the current symbol we're reading in the part that contains the code lengths of lit/len and dist codes*/
		i = 0;
		while (i < HLIT + HDIST) {
			unsigned code = huffman_decode_symbol (in, bp, tree_cl, inbitlength);
			if (code <= 15) /*a length code*/
			{
				if (i < HLIT) bitlen_ll[i] = code;
				else bitlen_d[i - HLIT] = code;
				i++;
			}
			else if (code == 16) /*repeat previous*/
			{
				unsigned replength = 3; /*read in the 2 bits that indicate repeat length (3-6)*/
				unsigned value; /*set value to the previous code*/

				if (*bp >= inbitlength) {
					ERROR_BREAK (50); /*error, bit pointer jumps past memory*/
				}
				if (i == 0) {
					ERROR_BREAK (54); /*can't repeat previous if i is 0*/
				}

				replength += readBitsFromStream (bp, in, 2);

				if (i < HLIT + 1) value = bitlen_ll[i - 1];
				else value = bitlen_d[i - HLIT - 1];
				/*repeat this value in the next lengths*/
				for (n = 0; n < replength; n++) {
					if (i >= HLIT + HDIST) {
						ERROR_BREAK (13); /*error: i is larger than the amount of codes*/
					}
					if (i < HLIT) bitlen_ll[i] = value;
					else bitlen_d[i - HLIT] = value;
					i++;
				}
			}
			else if (code == 17) /*repeat "0" 3-10 times*/
			{
				unsigned replength = 3; /*read in the bits that indicate repeat length*/
				if (*bp >= inbitlength) {
					ERROR_BREAK (50); /*error, bit pointer jumps past memory*/
				}

				replength += readBitsFromStream (bp, in, 3);

				/*repeat this value in the next lengths*/
				for (n = 0; n < replength; n++) {
					if (i >= HLIT + HDIST) {
						ERROR_BREAK (14); /*error: i is larger than the amount of codes*/
					}

					if (i < HLIT) {
						bitlen_ll[i] = 0;
					}
					else {
						bitlen_d[i - HLIT] = 0;
					}
					i++;
				}
			}
			else if (code == 18) /*repeat "0" 11-138 times*/
			{
				unsigned replength = 11; /*read in the bits that indicate repeat length*/
				if (*bp >= inbitlength) {
					ERROR_BREAK (50); /*error, bit pointer jumps past memory*/
				}

				replength += readBitsFromStream (bp, in, 7);

				/*repeat this value in the next lengths*/
				for (n = 0; n < replength; n++) {
					if (i >= HLIT + HDIST) {
						ERROR_BREAK (15); /*error: i is larger than the amount of codes*/
					}

					if (i < HLIT) {
						bitlen_ll[i] = 0;
					}
					else {
						bitlen_d[i - HLIT] = 0;
					}
					i++;
				}
			}
			else /*if(code == (unsigned)(-1))*/ /*huffman_decode_symbol returns (unsigned)(-1) in case of error*/
			{
				if (code == (unsigned)(-1)) {
					/*return error code 10 or 11 depending on the situation that happened in huffman_decode_symbol
					(10=no endcode, 11=wrong jump outside of tree)*/
					error = (*bp) > inbitlength ? 10 : 11;
				}
				else error = 16; /*unexisting code, this can never happen*/
				break;
			}
		}
		if (error) {
			break;
		}

		if (bitlen_ll[256] == 0) {
			ERROR_BREAK (64); /*the length of the end code 256 must be larger than 0*/
		}

		/*now we've finally got HLIT and HDIST, so generate the code trees, and the function is done*/
		error = huffmanTree_make_from_lengths (tree_ll, bitlen_ll, NUM_DEFLATE_CODE_SYMBOLS, 15);
		if (error) {
			break;
		}
		error = huffmanTree_make_from_lengths (tree_d, bitlen_d, NUM_DISTANCE_SYMBOLS, 15);

		break; /*end of error-while*/
	}

	delete[] bitlen_cl;
	delete[] bitlen_ll;
	delete[] bitlen_d;
	delete tree_cl;

	return error;
}

/*get the literal and length code tree of a deflated block with fixed tree, as per the deflate specification*/
unsigned generateFixedLitLenTree (HuffmanTree* tree)
{
	unsigned i, error = 0;
	unsigned* bitlen = new unsigned[NUM_DEFLATE_CODE_SYMBOLS];
	if (!bitlen) return 83; /*alloc fail*/

	/*288 possible codes: 0-255=literals, 256=endcode, 257-285=lengthcodes, 286-287=unused*/
	for (i = 0; i <= 143; i++) bitlen[i] = 8;
	for (i = 144; i <= 255; i++) bitlen[i] = 9;
	for (i = 256; i <= 279; i++) bitlen[i] = 7;
	for (i = 280; i <= 287; i++) bitlen[i] = 8;

	error = huffmanTree_make_from_lengths (tree, bitlen, NUM_DEFLATE_CODE_SYMBOLS, 15);

	delete[] bitlen;
	return error;
}

/// <summary>
/// get the distance code tree of a deflated block with fixed tree, as specified in the deflate specification
/// </summary>
/// <param name="tree"></param>
/// <returns></returns>
unsigned generateFixedDistanceTree (HuffmanTree* tree)
{
	unsigned i, error = 0;
	unsigned* bitlen = new unsigned[NUM_DISTANCE_SYMBOLS];
	if (!bitlen) {
		return 83; /*alloc fail*/
	}

	/*there are 32 distance codes, but 30-31 are unused*/
	for (i = 0; i < NUM_DISTANCE_SYMBOLS; i++) {
		bitlen[i] = 5;
	}
	error = huffmanTree_make_from_lengths (tree, bitlen, NUM_DISTANCE_SYMBOLS, 15);

	delete[] bitlen;
	return error;
}


/*the tree representation used by the decoder. return value is error*/
unsigned huffmanTree_make2DTree (HuffmanTree* tree)
{
	unsigned nodefilled = 0; /*up to which node it is filled*/
	unsigned treepos = 0; /*position in the tree (1 of the numcodes columns)*/
	unsigned n, i;

	tree->tree2d = new unsigned int[tree->numcodes * 2];
	if (!tree->tree2d) {
		return 83; /*alloc fail*/
	}

	/*
	convert tree1d[] to tree2d[][]. In the 2D array, a value of 32767 means
	uninited, a value >= numcodes is an address to another bit, a value < numcodes
	is a code. The 2 rows are the 2 possible bit values (0 or 1), there are as
	many columns as codes - 1.
	A good huffmann tree has N * 2 - 1 nodes, of which N - 1 are internal nodes.
	Here, the internal nodes are stored (what their 0 and 1 option point to).
	There is only memory for such good tree currently, if there are more nodes
	(due to too long length codes), error 55 will happen
	*/
	for (n = 0; n < tree->numcodes * 2; n++) {
		tree->tree2d[n] = 32767; /*32767 here means the tree2d isn't filled there yet*/
	}

	for (n = 0; n < tree->numcodes; n++) {/*the codes*/
		for (i = 0; i < tree->lengths[n]; i++) { /*the bits for this code*/
			BYTE bit = (BYTE)((tree->tree1d[n] >> (tree->lengths[n] - i - 1)) & 1);
			if (treepos > tree->numcodes - 2) {
				return 55; /*oversubscribed, see comment in lodepng_error_text*/
			}
			if (tree->tree2d[2 * treepos + bit] == 32767) { /*not yet filled in*/
				if (i + 1 == tree->lengths[n]) { /*last bit*/
					tree->tree2d[2 * treepos + bit] = n; /*put the current code in it*/
					treepos = 0;
				}
				else {
					/*put address of the next step in here, first that address has to be found of course
					(it's just nodefilled + 1)...*/
					nodefilled++;
					/*addresses encoded with numcodes added to it*/
					tree->tree2d[2 * treepos + bit] = nodefilled + tree->numcodes;
					treepos = nodefilled;
				}
			}
			else treepos = tree->tree2d[2 * treepos + bit] - tree->numcodes;
		}
	}

	for (n = 0; n < tree->numcodes * 2; n++) {
		if (tree->tree2d[n] == 32767) {
			tree->tree2d[n] = 0; /*remove possible remaining 32767's*/
		}
	}

	return 0;
}

/// <summary>
/// Second step for the ...makeFromLengths and ...makeFromFrequencies functions.
/// numcodes, lengths and maxbitlen must already be filled in correctly. return
/// value is error.
/// </summary>
/// <param name="tree"></param>
/// <returns></returns>
unsigned huffmanTree_make_from_lengths2 (HuffmanTree* tree)
{
	uivector* blcount = new uivector ();
	uivector* nextcode = new uivector ();;
	unsigned error = 0;

	//uivector_init (&blcount);
	//uivector_init (&nextcode);

	tree->tree1d = new unsigned[tree->numcodes];
	if (!tree->tree1d) {
		error = 83; /*alloc fail*/
	}

	if (!blcount->uivector_resizev (tree->maxbitlen + 1, 0) || !nextcode->uivector_resizev (tree->maxbitlen + 1, 0)) {
		error = 83; /*alloc fail*/
	}

	if (!error) {
		/*step 1: count number of instances of each code length*/
		for (auto bits = 0U; bits < tree->numcodes; bits++) {
			blcount->data[tree->lengths[bits]]++;
		}
		/*step 2: generate the nextcode values*/
		for (auto bits = 1U; bits <= tree->maxbitlen; bits++) {
			nextcode->data[bits] = (nextcode->data[bits - 1] + blcount->data[bits - 1]) << 1;
		}
		/*step 3: generate all the codes*/
		for (auto n = 0U; n < tree->numcodes; n++) {
			if (tree->lengths[n] != 0) {
				tree->tree1d[n] = nextcode->data[tree->lengths[n]]++;
			}
		}
	}

	delete blcount;
	delete nextcode;

	if (!error) {
		return huffmanTree_make2DTree (tree);
	}
	else {
		return error;
	}
}

/// <summary>
/// given the code lengths (as stored in the PNG file), generate the tree as defined
/// by Deflate.maxbitlen is the maximum bits that a code in the tree can have.
/// return value is error.
/// </summary>
/// <param name="tree"></param>
/// <param name="bitlen"></param>
/// <param name="numcodes"></param>
/// <param name="maxbitlen"></param>
/// <returns></returns>
unsigned huffmanTree_make_from_lengths (HuffmanTree* tree, const unsigned* bitlen,
										size_t numcodes, unsigned maxbitlen)
{
	unsigned i;
	tree->lengths = new unsigned[numcodes];
	if (!tree->lengths) {
		return 83; /*alloc fail*/
	}
	for (i = 0; i < numcodes; i++) {
		tree->lengths[i] = bitlen[i];
	}
	tree->numcodes = (unsigned)numcodes; /*number of symbols*/
	tree->maxbitlen = maxbitlen;
	return huffmanTree_make_from_lengths2 (tree);
}


/*
returns the code, or (unsigned)(-1) if error happened
inbitlength is the length of the complete buffer, in bits (so its byte length times 8)
*/
unsigned huffman_decode_symbol (const BYTE* in, size_t* bp,
								const HuffmanTree* codetree, size_t inbitlength)
{
	unsigned treepos = 0;
	for (;;) {
		if (*bp >= inbitlength) {
			return (unsigned)(-1); /*error: end of input memory reached without endcode*/
		}
		/*
		decode the symbol from the tree. The "readBitFromStream" code is inlined in
		the expression below because this is the biggest bottleneck while decoding
		*/
		auto ct = codetree->tree2d[(treepos << 1) + READBIT (*bp, in)];
		(*bp)++;
		if (ct < codetree->numcodes) {
			return ct; /*the symbol is decoded, return it*/
		}
		else {
			treepos = ct - codetree->numcodes; /*symbol not yet decoded, instead move tree position*/
		}

		if (treepos >= codetree->numcodes) {
			return (unsigned)(-1); /*error: it appeared outside the codetree*/
		}
	}
}


/// <summary>
/// 
/// </summary>
/// <param name="out"></param>
/// <param name="outsize"></param>
/// <param name="in"></param>
/// <param name="insize"></param>
/// <param name="settings"></param>
/// <returns></returns>
unsigned lodepng_inflate (BYTE** out, size_t* outsize, const BYTE* in, size_t insize, const LodePNGDecompressSettings* settings)
{
#if LODEPNG_CUSTOM_ZLIB_DECODER == 2
	if (settings->custom_decoder) {
		return lodepng_custom_inflate (out, outsize, in, insize, settings);
	}
	else {
#endif /*LODEPNG_CUSTOM_ZLIB_DECODER == 2*/
		unsigned error;
		ucvector* v = new ucvector ();
		v->ucvector_init_buffer (*out, *outsize);
		error = lodepng_inflatev (v, in, insize, settings);
		*out = v->data;
		*outsize = v->size;
		return error;
#if LODEPNG_CUSTOM_ZLIB_DECODER == 2
	}
#endif /*LODEPNG_CUSTOM_ZLIB_DECODER == 2*/
}


/// <summary>
/// 
/// </summary>
/// <param name="out"></param>
/// <param name="in"></param>
/// <param name="insize"></param>
/// <param name="settings"></param>
/// <returns></returns>
unsigned lodepng_inflatev (ucvector* out, const BYTE* in, size_t insize, const LodePNGDecompressSettings* settings)
{
	/*bit pointer in the "in" data, current byte is bp >> 3, current bit is bp & 0x7 (from lsb to msb of the byte)*/
	size_t bp = 0;
	unsigned BFINAL = 0;
	size_t pos = 0; /*byte position in the out buffer*/

	unsigned error = 0;

	//(void)settings;

	while (!BFINAL) {
		unsigned BTYPE;
		if (bp + 2 >= insize * 8) {
			return 52; /*error, bit pointer will jump past memory*/
		}
		BFINAL = readBitFromStream (&bp, in);
		BTYPE = 1 * readBitFromStream (&bp, in);
		BTYPE += 2 * readBitFromStream (&bp, in);

		if (BTYPE == 3) {
			return 20; /*error: invalid BTYPE*/
		}
		else if (BTYPE == 0) {
			error = out->inflateNoCompression (in, &bp, &pos, insize); /*no compression*/
		}
		else {
			error = out->inflateHuffmanBlock (in, &bp, &pos, insize, BTYPE); /*compression, BTYPE 01 or 10*/
		}

		if (error) {
			return error;
		}
	}

	/*Only now we know the true size of out, resize it to that*/
	if (!out->ucvector_resize (pos)) {
		error = 83; /*alloc fail*/
	}

	return error;
}
/*///////////////////////////////////////////////////////////////////////////////////////////////*/
/* Main deflate section
/*///////////////////////////////////////////////////////////////////////////////////////////////*/

/// <summary>
/// Lode's main function, call this
/// </summary>
/// <param name="out">malloced return for decompressed output</param>
/// <param name="outsize">return for output size</param>
/// <param name="in">the zlib stream</param>
/// <param name="insize">number of bytes in input stream</param>
/// <param name="settings">
/// typedef struct LodePNGDecompressSettings
/// {
/// unsigned ignore_adler32; // 1, continue without warning if Adler checksum corrupted
/// unsigned custom_decoder; //use custom decoder if LODEPNG_CUSTOM_ZLIB_DECODER and LODEPNG_COMPILE_ZLIB are enabled
/// } LodePNGDecompressSettings;
/// Pass 1, 0 if unsure what to do.
/// </param>
/// <returns>an error code, 0 on success</returns>
unsigned lodepng_zlib_decompress (BYTE** out, size_t* outsize, const BYTE* in, size_t insize, const LodePNGDecompressSettings* settings)
{
	unsigned error = 0;
	unsigned CM, CINFO, FDICT;

	if (insize < 2) {
		return 53; /*error, size of zlib data too small*/
	}
	/*read information from zlib header*/
	if ((in[0] * 256 + in[1]) % 31 != 0) {
		/*error: 256 * in[0] + in[1] must be a multiple of 31, the FCHECK value is supposed to be made that way*/
		return 24;
	}

	CM = in[0] & 15;
	CINFO = (in[0] >> 4) & 15;
	/*FCHECK = in[1] & 31;*/ /*FCHECK is already tested above*/
	FDICT = (in[1] >> 5) & 1;
	/*FLEVEL = (in[1] >> 6) & 3;*/ /*FLEVEL is not used here*/

	if (CM != 8 || CINFO > 7) {
		/*error: only compression method 8: inflate with sliding window of 32k is supported by the PNG spec*/
		return 25;
	}
	if (FDICT != 0) {
		/*error: the specification of PNG says about the zlib stream:
		"The additional flags shall not specify a preset dictionary."*/
		return 26;
	}
	error = lodepng_inflate (out, outsize, in + 2, insize - 2, settings);
	if (error) {
		return error;
	}

	if (!settings->ignore_adler32) {
		unsigned ADLER32 = lodepng_read32bitInt (&in[insize - 4]);
		unsigned checksum = adler32 (*out, (unsigned)(*outsize));
		if (checksum != ADLER32) {
			return 58; /*error, adler checksum not correct, data must be corrupted*/
		}
	}

	return 0; /*no error*/
#if LODEPNG_CUSTOM_ZLIB_DECODER == 1
}
#endif /*LODEPNG_CUSTOM_ZLIB_DECODER == 1*/
}