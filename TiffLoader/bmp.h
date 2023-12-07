#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "loadtiff.h"

// 画像ジェネレートテスト

// windows . UNIX 両方対応できる工夫をここで行う。
typedef unsigned char       BYTE;            /* 1byte符号なし整数 */
typedef unsigned short      WORD;            /* 2byte符号なし整数 */
typedef unsigned long       DWORD;           /* 4byte符号なし整数 */
typedef long                LONG;            /* 4byte整数         */

#pragma pack(1)

class BMPFILEHEADER
{
public:
	char    bfType[2];
	DWORD	bfSize;
	WORD    bfReserved1;
	WORD    bfReserved2;
	DWORD	bfOffBits;
	BMPFILEHEADER ()
	{
		bfType[0] = 0;
		bfType[1] = 0;
		bfSize = 0;
		bfReserved1 = 0;
		bfReserved2 = 0;
		bfOffBits = 0;
	}
};

class BMPINFOHEADER
{
public:
	DWORD	biSize;
	LONG    biWidth;
	LONG    biHeight;
	WORD    biPlanes;
	WORD    biBitCount;
	DWORD	biCompression;
	DWORD	biSizeImage;
	LONG    biXPelsPerMeter;
	LONG    biYPelsPerMeter;
	DWORD	biClrUsed;
	DWORD	biClrImportant;
	BMPINFOHEADER ()
	{
		biSize = 0;
		biWidth = 0;
		biHeight = 0;
		biPlanes = 0;
		biBitCount = 0;
		biCompression = 0;
		biSizeImage = 0;
		biXPelsPerMeter = 0;
		biYPelsPerMeter = 0;
		biClrUsed = 0;
		biClrImportant = 0;
	}
};

class RGBCOLOR
{
public:
	BYTE    rgbBlue;
	BYTE    rgbGreen;
	BYTE    rgbRed;
	RGBCOLOR ()
	{
		rgbBlue = 0;
		rgbGreen = 0;
		rgbRed = 0;
	}
};

#pragma pack()


// 管理用のクラス
class bmp_image
{
private:
	BMPFILEHEADER	bmpHeader;
	BMPINFOHEADER	bmpInfo;

protected:
	int	nWidth;		// 横幅
	int	nHeight;	// 縦幅

	RGBCOLOR* rgbImage;	// 画像データの本体	

public:

	// コントラクタ
	bmp_image ()
	{
		nWidth = 0;
		nHeight = 0;
		rgbImage = NULL;
		//memset(&bmpHeader, 0, sizeof(BMPFILEHEADER));
		//memset(&bmpInfo, 0, sizeof(BMPINFOHEADER));
	};

	// デストラクタ
	~bmp_image ()
	{
		delete[] rgbImage;
	};

	// 画像データの生成
	int allocate_image (unsigned int width, unsigned int height);

	// 画像データの吐き出し
	int save_image (char* filename);

	// ヘッダの作成
	void set_header ();

	// 画像データ設定
	void assign_data (const char* data, FMT format);
};
