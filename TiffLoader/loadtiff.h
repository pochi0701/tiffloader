#ifndef loadtiff_h
#define loadtiff_h


#include <stdio.h>


/*
  By Malcolm McLean


  To use
  int width, height;
  BYTE *data;
  int format;

  FILE *fp = fopen("tiffile.tiff", "rb");
  if(!fp)
	 errorhandling();
  data = load_tiff(fp, &width, &height, &format);
  fclose(fp);

  if(data == 0)
	 printf("TIFF file unreadable\n");

	 data format given by format - it's 8 bit channels
	   with alpha (if any) last.
	 alpha is premultiplied = composted on black. To get
		the image composted on white, call floadtiffwhite()
	 width is image width, height is image height in pixels
  */
#define LODEPNG_CUSTOM_ZLIB_DECODER 0
typedef unsigned char BYTE;
enum class FMT
{
	FMT_ERROR = 0,
	FMT_RGBA = 1,
	FMT_CMYK = 2,
	FMT_CMYKA = 3,
	FMT_GREYALPHA = 4,
	FMT_RGB = 5,
	FMT_GREY = 6,
};

const enum ENDIAN
{
	NOT_DEFINED = -1,
	BIG_ENDIAN = 1,
	LITTLE_ENDIAN = 2,
};
// 例外を表すクラス
class some_exception
{
private:
	const char* msg;   // 例外を説明するメッセージ
public:
	some_exception (const char* msg) : msg (msg) {}  // コンストラクタ
	const char* what () { return msg; }  // メッセージを返す
};


class FileData
{
public:
	char* buffer;
	long size;
	int buffer_ptr;
	ENDIAN type;
	FileData ()
	{
		buffer = NULL;
		size = 0;
		buffer_ptr = 0;
		type = LITTLE_ENDIAN;
	}
	~FileData ()
	{
		if (buffer != NULL) {
			delete[] buffer;
			buffer = NULL;
		}
	}
	bool FileRead (const char* path)
	{
		FILE* fp = NULL;
		fopen_s (&fp, path, "rb");
		if (!fp) {
			perror ("error");
			return false;
		}
		fseek (fp, 0, SEEK_END);
		size = ftell (fp);
		fseek (fp, 0, SEEK_SET);
		buffer = new char[size];
		fread (buffer, size, 1, fp);
		fclose (fp);
		buffer_ptr = 0;
		return true;
	}
	/// <summary>
	/// ENDIANの決定
	/// </summary>
	/// <returns></returns>
	void set_endian ()
	{
		auto enda = fgetcc ();
		auto endb = fgetcc ();
		if (enda == 'I' && endb == 'I') {
			type = ENDIAN::LITTLE_ENDIAN;
		}
		else if (enda == 'M' && endb == 'M') {
			type = ENDIAN::BIG_ENDIAN;
		}
		else {
			throw some_exception ("parse_error");  // 例外をスロー
		}
	}
	/// <summary>
	/// バッファから1バイト取得
	/// </summary>
	/// <returns>符号なし読み込み１バイト</returns>
	int fgetcc ()
	{
		if (buffer_ptr >= size) {
			throw some_exception ("memory_error");  // 例外をスロー
		}
		return (BYTE)buffer[buffer_ptr++];
	}
	/// <summary>
	/// 符号なし３２ビット取得
	/// </summary>
	/// <returns></returns>
	unsigned long fget32u ()
	{
		int a, b, c, d;

		a = fgetcc ();
		b = fgetcc ();
		c = fgetcc ();
		d = fgetcc ();

		if (type == ENDIAN::BIG_ENDIAN) {
			return (a << 24) | (b << 16) | (c << 8) | d;
		}
		else {
			return (d << 24) | (c << 16) | (b << 8) | a;
		}
	}
	/// <summary>
	/// 符号なし16ビット取得
	/// </summary>
	/// <returns></returns>
	unsigned int fget16u ()
	{
		int a, b;
		a = fgetcc ();
		b = fgetcc ();
		if (type == ENDIAN::BIG_ENDIAN) {
			return (a << 8) | b;
		}
		else {
			return (b << 8) | a;
		}
	}

	/// <summary>
	/// 16ビット整数取得
	/// </summary>
	/// <returns></returns>
	int fget16 ()
	{
		if (type == ENDIAN::BIG_ENDIAN) {
			return fget16be ();
		}
		else {
			return fget16le ();
		}
	}

	/// <summary>
	/// ３２ビット整数取得
	/// </summary>
	/// <returns></returns>
	long fget32 ()
	{
		if (type == ENDIAN::BIG_ENDIAN) {
			return fget32be ();

		}
		else {
			return fget32le ();
		}
	}
	/// <summary>
	/// １６ビットBIG Endian取得
	/// </summary>
	/// <returns></returns>
	int fget16be ()
	{
		int c1, c2;

		c2 = fgetcc ();
		c1 = fgetcc ();

		return ((c2 ^ 128) - 128) * 256 + c1;
	}

	/// <summary>
	/// 32ビットBIG Endian取得
	/// </summary>
	/// <returns></returns>
	long fget32be ()
	{
		int c1, c2, c3, c4;

		c4 = fgetcc ();
		c3 = fgetcc ();
		c2 = fgetcc ();
		c1 = fgetcc ();
		return ((c4 ^ 128) - 128) * 256 * 256 * 256 + c3 * 256 * 256 + c2 * 256 + c1;
	}

	/// <summary>
	/// 16ビットLITTLE Endian取得
	/// </summary>
	/// <returns></returns>
	int fget16le ()
	{
		int c1, c2;

		c1 = fgetcc ();
		c2 = fgetcc ();

		return ((c2 ^ 128) - 128) * 256 + c1;
	}

	/// <summary>
	/// 32ビットLITTLE Endian取得
	/// </summary>
	/// <returns></returns>
	long fget32le ()
	{
		int c1, c2, c3, c4;

		c1 = fgetcc ();
		c2 = fgetcc ();
		c3 = fgetcc ();
		c4 = fgetcc ();
		return ((c4 ^ 128) - 128) * 256 * 256 * 256 + c3 * 256 * 256 + c2 * 256 + c1;
	}

	void memcpy (void* dest, unsigned long datasize)
	{
		::memcpy (dest, buffer + buffer_ptr, datasize);
		buffer_ptr += datasize;
	}
};
enum class TAG_TYPE
{
	TAG_NONE = 0,
	TAG_BYTE = 1,
	TAG_ASCII = 2,
	TAG_SHORT = 3,
	TAG_LONG = 4,
	TAG_RATIONAL = 5,
};

/* data types
1 BYTE 8 - bit unsigned integer

2 ASCII 8 - bit, NULL - terminated string

3 SHORT 16 - bit unsigned integer

4 LONG 32 - bit unsigned integer

5 RATIONAL Two 32 - bit unsigned integers


6 SBYTE 8 - bit signed integer

7 UNDEFINE 8 - bit byte

8 SSHORT 16 - bit signed integer
9 SLONG 32 - bit signed integer
10 SRATIONAL Two 32 - bit signed integers

11 FLOAT 4 - byte single - precision IEEE floating - point value

12 DOUBLE 8 - byte double - precision IEEE floating - point value

*/
enum class COMPRESSION
{
	COMPRESSION_NONE = 1,
	COMPRESSION_CCITTRLE = 2,
	COMPRESSION_CCITTFAX3 = 3,
	COMPRESSION_CCITT_T4 = 3,
	COMPRESSION_CCITTFAX4 = 4,
	COMPRESSION_CCITT_T6 = 4,
	COMPRESSION_LZW = 5,
	COMPRESSION_OJPEG = 6,
	COMPRESSION_JPEG = 7,
	COMPRESSION_NEXT = 32766,
	COMPRESSION_CCITTRLEW = 32771,
	COMPRESSION_PACKBITS = 32773,
	COMPRESSION_THUNDERSCAN = 32809,
	COMPRESSION_IT8CTPAD = 32895,
	COMPRESSION_IT8LW = 32896,
	COMPRESSION_IT8MP = 32897,
	COMPRESSION_IT8BL = 32898,
	COMPRESSION_PIXARFILM = 32908,
	COMPRESSION_PIXARLOG = 32909,
	COMPRESSION_DEFLATE = 32946,
	COMPRESSION_ADOBE_DEFLATE = 8,
	COMPRESSION_DCS = 32947,
	COMPRESSION_JBIG = 34661,
	COMPRESSION_SGILOG = 34676,
	COMPRESSION_SGILOG24 = 34677,
	COMPRESSION_JP2000 = 34712,
};

enum class TID
{
	TID_NONE = 0,
	TID_IMAGEWIDTH = 256,
	TID_IMAGEHEIGHT = 257,
	TID_BITSPERSAMPLE = 258,
	TID_COMPRESSION = 259,
	TID_PHOTOMETRICINTERPRETATION = 262,
	TID_FILLORDER = 266,
	TID_STRIPOFFSETS = 273,
	TID_SAMPLESPERPIXEL = 277,
	TID_ROWSPERSTRIP = 278,
	TID_STRIPBYTECOUNTS = 279,
	TID_PLANARCONFIGUATION = 284,
	TID_T4OPTIONS = 292,
	TID_PREDICTOR = 317,
	TID_COLORMAP = 320,
	TID_TILEWIDTH = 322,
	TID_TILELENGTH = 323,
	TID_TILEOFFSETS = 324,
	TID_TILEBYTECOUNTS = 325,
	TID_EXTRASAMPLES = 338,
	TID_SAMPLEFORMAT = 339,
	TID_SMINSAMPLEVALUE = 340,
	TID_SMAXSAMPLEVALUE = 341,
	TID_YCBCRCOEFFICIENTS = 529,
	TID_YCBCRSUBSAMPLING = 530,
	TID_YCBCRPOSITIONING = 531,
	// Artist 315 ASCII - * *
	// BadFaxLines[1] 326 SHORT or LONG - - -
	// BitsPerSample 258 SHORT * * *
	// CellLength 265 SHORT * * *
	// CellWidth 264 SHORT * * *
	// CleanFaxData[1] 327 SHORT - - -
	// ColorMap 320 SHORT - * *
	// ColorResponseCurve 301 SHORT * * x
	// ColorResponseUnit 300 SHORT * x x
	// Compression 259 SHORT * * *
	// Uncompressed 1 * * *
	// CCITT 1D 2 * * *
	// CCITT Group 3 3 * * *
	// CCITT Group 4 4 * * *
	// LZW 5 - * *
	// JPEG 6 - - *
	// Uncompressed 32771 * x x
	// Packbits 32773 * * *
	// ConsecutiveBadFaxLines[1] 328 LONG or SHORT - - -
	// Copyright 33432 ASCII - - *
	// DateTime 306 ASCII - * *
	// DocumentName 269 ASCII * * *
	// DotRange 336 BYTE or SHORT - - *
	// ExtraSamples 338 BYTE - - *
	// FillOrder 266 SHORT * * *
	// FreeByteCounts 289 LONG * * *
	// FreeOffsets 288 LONG * * *
	// GrayResponseCurve 291 SHORT * * *
	// GrayResponseUnit 290 SHORT * * *
	// HalftoneHints 321 SHORT - - *
	// HostComputer 316 ASCII - * *
	// ImageDescription 270 ASCII * * *
	// ImageHeight 257 SHORT or LONG * * *
	// ImageWidth 256 SHORT or LONG * * *
	// InkNames 333 ASCII - - *
	// InkSet 332 SHORT - - *
	// JPEGACTTables 521 LONG - - *
	// JPEGDCTTables 520 LONG - - *
	// JPEGInterchangeFormat 513 LONG - - *
	// JPEGInterchangeFormatLength 514 LONG - - *
	// JPEGLosslessPredictors 517 SHORT - - *
	// JPEGPointTransforms 518 SHORT - - *
	// JPEGProc 512 SHORT - - *
	// JPEGRestartInterval 515 SHORT - - *
	// JPEGQTables 519 LONG - - *
	// Make 271 ASCII * * *
	// MaxSampleValue 281 SHORT * * *
	// MinSampleValue 280 SHORT * * *
	// Model 272 ASCII * * *
	// NewSubFileType 254 LONG - * *
	// NumberOfInks 334 SHORT - - *
	// Orientation 274 SHORT * * *
	// PageName 285 ASCII * * *
	// PageNumber 297 SHORT * * *
	// PhotometricInterpretation 262 SHORT * * *
	// WhiteIsZero 0 * * *
	// BlackIsZero 1 * * * RGB 2 * * *
	// RGB Palette 3 - * *
	// Tranparency Mask 4 - - *
	// CMYK 5 - - *
	// YCbCr 6 - - *
	// CIELab 8 - - *
	// PlanarConfiguration 284 SHORT * * *
	// Predictor 317 SHORT - * *
	// PrimaryChromaticities 319 RATIONAL - * *
	// ReferenceBlackWhite 532 LONG - - *
	// ResolutionUnit 296 SHORT * * *
	// RowsPerStrip 278 SHORT or LONG * * *
	// SampleFormat 339 SHORT - - *
	// SamplesPerPixel 277 SHORT * * *
	// SMaxSampleValue 341 Any - - *
	// SMinSampleValue 340 Any - - *
	// Software 305 ASCII - * *
	// StripByteCounts 279 LONG or SHORT * * *
	// StripOffsets 273 SHORT or LONG * * *
	// SubFileType 255 SHORT * x x
	// T4Options[2] 292 LONG * * *
	// T6Options[3] 293 LONG * * *
	// TargetPrinter 337 ASCII - - *
	// Thresholding 263 SHORT * * *
	// TileByteCounts 325 SHORT or LONG - - *
	// TileLength 323 SHORT or LONG - - *
	// TileOffsets 324 LONG - - *
	// TileWidth 322 SHORT or LONG - - *
	// TransferFunction[4] 301 SHORT - - *
	// TransferRange 342 SHORT - - *
	// XPosition 286 RATIONAL * * *
	// XResolution 282 RATIONAL * * *
	// YCbCrCoefficients 529 RATIONAL - - *
	// YCbCrPositioning 531 SHORT - - *
	// YCbCrSubSampling 530 SHORT - - *
	// YPosition 287 RATIONAL * * *
	// YResolution 283 RATIONAL * * *
	// WhitePoint 318 RATIONAL - * *
	// ExtraSamples 338 SHORT
};
enum class SAMPLE_FORMAT
{
	SAMPLEFORMAT_UINT = 1,
	SAMPLEFORMAT_INT = 2,
	SAMPLEFORMAT_IEEEFP = 3,
	SAMPLEFORMAT_VOID = 4,
	//#define SAMPLEFORMAT_COMPLEXINT  5
	//#define SAMPLEFORMAT_COMPLEXIEEEFP 6
};


enum class photo_metric_interpretations
{
	PI_Not_Defined = -1,
	PI_WhiteIsZero = 0,
	PI_BlackIsZero = 1,
	PI_RGB = 2,
	PI_RGB_Palette = 3,
	PI_Tranparency_Mask = 4,
	PI_CMYK = 5,
	PI_YCbCr = 6,
	PI_CIELab = 8,
};

class TAG
{
public:
	TID tagid;
	TAG_TYPE datatype;
	unsigned long datacount;
	double scalar;
	char* ascii;
	void* vector;
	int bad;
	TAG ()
	{
		tagid = TID::TID_NONE;
		datatype = TAG_TYPE::TAG_NONE;
		datacount = 0;
		scalar = 0;
		ascii = NULL;
		vector = NULL;
		bad = 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////
///BASICHEADER
/////////////////////////////////////////////////////////////////////////////////////////////////
class  BASICHEADER
{
public:
	unsigned long newsubfiletype;
	int imagewidth;
	int imageheight;
	int bitspersample[16];
	COMPRESSION compression;
	int fillorder;
	photo_metric_interpretations photo_metric_interpretation;
	unsigned long* stripoffsets;
	int Nstripoffsets;
	int samplesperpixel;
	int rowsperstrip;
	unsigned long* stripbytecounts;
	int Nstripbytecounts;
	double xresolution;
	double yresolution;
	int resolutionunit;
	/*  Palette files */
	BYTE* colormap;
	int Ncolormap;
	/* RGB */
	int planarconfiguration;
	int predictor;
	/* YCbCr*/


	//double YCbCrCoefficients[3];
	double LumaRed;
	double LumaGreen;
	double LumaBlue;
	int YCbCrSubSampling_h;
	int YCbCrSubSampling_v;
	int YCbCrPositioning;
	int ReferenceBlackWhite;

	/* Class F */

	int BadFaxLines;
	int CleanFaxData;
	int ConsecutiveBadFaxLines;
	unsigned long T4options;

	/* tiling */
	int tilewidth;
	int tileheight;
	unsigned long* tileoffsets;
	int Ntileoffsets;
	unsigned long* tilebytecounts;
	int Ntilebytecounts;
	/* Malcolm easier to support this now*/
	SAMPLE_FORMAT sampleformat[16];
	double* smaxsamplevalue;
	int Nsmaxsamplevalue;
	double* sminsamplevalue;
	int Nsminsamplevalue;
	int extrasamples;
	ENDIAN endianness;

	BASICHEADER ()
	{
		int i;
		newsubfiletype = 0;
		imagewidth = -1;
		imageheight = -1;
		bitspersample[0] = -1;
		compression = COMPRESSION::COMPRESSION_NONE;
		fillorder = 1;
		photo_metric_interpretation = photo_metric_interpretations::PI_Not_Defined;
		stripoffsets = NULL;
		Nstripoffsets = 0;
		samplesperpixel = 0;
		rowsperstrip = 0;
		stripbytecounts = NULL;
		Nstripbytecounts = 0;
		xresolution = 1.0;
		yresolution = 1.0;
		resolutionunit = 0;
		/*  Palette files */
		colormap = NULL;
		Ncolormap = 0;
		/* RGB */
		planarconfiguration = 1;
		predictor = 1;
		/* YCbCr*/

		LumaRed = 0.299;
		LumaGreen = 0.587;
		LumaBlue = 0.114;
		//YCbCrCoefficients = 0;
		YCbCrSubSampling_h = 2;
		YCbCrSubSampling_v = 2;
		YCbCrPositioning = 1;
		ReferenceBlackWhite = 0;

		/* Class F */

		//int BadFaxLines;
		//int CleanFaxData;
		//int ConsecutiveBadFaxLines;
		T4options = 0;

		tilewidth = 0;
		tileheight = 0;
		tileoffsets = NULL;
		Ntileoffsets = 0;
		tilebytecounts = NULL;
		Ntilebytecounts = 0;

		for (i = 0; i < 16;i++) {
			sampleformat[i] = SAMPLE_FORMAT::SAMPLEFORMAT_UINT;
		}
		smaxsamplevalue = NULL;
		Nsmaxsamplevalue = 0;
		sminsamplevalue = NULL;
		Nsminsamplevalue = 0;
		extrasamples = 0;
		endianness = ENDIAN::NOT_DEFINED;

		//for cppcheck
		BadFaxLines = 0;
		CleanFaxData = 0;
		ConsecutiveBadFaxLines = 0;
	}

	~BASICHEADER ()
	{
		delete[] stripbytecounts;
		delete[] stripoffsets;
		delete[] tilebytecounts;
		delete[] tileoffsets;
		delete[] colormap;
		delete[] smaxsamplevalue;
		delete[] sminsamplevalue;
	}
	//void header_defaults ();
	//void freeheader ();
	int header_fixupsections ();
	int header_not_ok ();
	int fill_header (TAG* tags, int Ntags);
	int header_Noutsamples ();
	int header_Ninsamples ();
	FMT header_outputformat ();
	BYTE* load_raster (FileData* fd, FMT* format);
	BYTE* read_strip (int index, int* strip_width, int* strip_height, FileData* fd, int* insamples);
	BYTE* read_tile (int index, int* tile_width, int* tile_height, FileData* fd, int* insamples);
	BYTE* read_channel (int index, int* channel_width, int* channel_height, FileData* fd);
	/*//////////////////////////////////////////////////////////////////////////////////////////////////*/
	/* stip tile and plane loading section*/
	/*//////////////////////////////////////////////////////////////////////////////////////////////////*/
	int plane_to_channel (BYTE* out, int width, int height, BYTE* bits, unsigned long Nbytes, int sample_index);
	int grey_to_grey (BYTE* rgba, int width, int height, BYTE* bits, unsigned long N, int insamples);
	int pal_to_rgba (BYTE* rgba, int width, int height, BYTE* bits, unsigned long Nbytes);
	int ycbcr_to_rgba (BYTE* rgba, int width, int height, BYTE* bits, unsigned long N);
	int cmyk_to_cmyk (BYTE* rgba, int width, int height, BYTE* bits, unsigned long Nbytes, int insamples);
	int bitstream_to_rgba (BYTE* rgba, int width, int height, BYTE* bits, unsigned long Nbytes, int insamples);
	void unpredict_samples (BYTE* buff, int width, int height, int depth);
	int read_byte_sample (BYTE* bytes, int sample_index);
	int read_int_sample (const BYTE* bytes, int sample_index);

};

class BSTREAM
{
public:
	BYTE* data;
	int N;
	int pos;
	int bit;
	ENDIAN endianness;
	BSTREAM ()
	{
		data = NULL;
		N = 0;
		pos = 0;
		bit = 0;
		endianness = ENDIAN::NOT_DEFINED;
	}

	/// <summary>
	/// create a bitstream.
	/// </summary>
	/// <param name="data">the data buffer</param>
	/// <param name="N">size of data buffer</param>
	/// <param name="endianness"></param>
	/// <returns>constructed object</returns>
	BSTREAM (BYTE* data, int N, ENDIAN endianness)
	{
		this->data = data;
		this->pos = 0;
		this->N = N;
		this->endianness = endianness;

		if (this->endianness == ENDIAN::BIG_ENDIAN) {
			this->bit = 128;
		}
		else {
			this->bit = 1;
		}
	}
	int getbit ();
	int getbits (int nbits);
	int synch_to_byte ();
	int writebit (int bit);
};
enum class CCITT
{
	CCITT_PASS = 101,
	CCITT_HORIZONTAL = 102,
	CCITT_VERTICAL_0 = 103,
	CCITT_VERTICAL_R1 = 104,
	CCITT_VERTICAL_R2 = 105,
	CCITT_VERTICAL_R3 = 106,
	CCITT_VERTICAL_L1 = 107,
	CCITT_VERTICAL_L2 = 108,
	CCITT_VERTICAL_L3 = 109,
	CCITT_EXTENSION = 110,
	CCITT_ENDOFFAXBLOCK = 110,
};


BYTE* decompress (FileData* fd, unsigned long count, COMPRESSION compression, unsigned long* Nret, int width, int height, unsigned long T4options);
TAG* load_header (FileData* fd, int* Ntags);
void killtags (TAG* tags, int N);
int load_tags (TAG* tag, FileData* fd);
double tag_get_entry (TAG* tag, int index);
void pasteflexible (BYTE* buff, int width, int height, int depth, const BYTE* tile, int twidth, int theight, int tdepth, int x, int y);
char* fread_asciiz (FileData* fd);
double memread_ieee754 (BYTE* buff, int bigendian);
float memread_ieee754f (BYTE* buff, int bigendian);
int YcbcrToRGB (int Y, int cb, int cr, BYTE* red, BYTE* green, BYTE* blue);

class TIFF
{
public:
	FMT format;
	int height;
	int width;
	FileData* fd;
	TIFF ()
	{
		fd = new FileData ();
		format = FMT::FMT_ERROR;
		height = 0;
		width = 0;
	}
	~TIFF ()
	{
		delete fd;
	}
	/// <summary>
	/// ファイル読み込み
	/// </summary>
	/// <param name="filename"></param>
	void file_read (char* filename)
	{
		if (!fd->FileRead (filename)) {
			perror ("error");
		}
	}
	BYTE* floadtiffwhite ();
	BYTE* load_tiff ();
};

#endif
