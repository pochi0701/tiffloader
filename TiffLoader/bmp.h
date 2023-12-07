#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "loadtiff.h"

// �摜�W�F�l���[�g�e�X�g

// windows . UNIX �����Ή��ł���H�v�������ōs���B
typedef unsigned char       BYTE;            /* 1byte�����Ȃ����� */
typedef unsigned short      WORD;            /* 2byte�����Ȃ����� */
typedef unsigned long       DWORD;           /* 4byte�����Ȃ����� */
typedef long                LONG;            /* 4byte����         */

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


// �Ǘ��p�̃N���X
class bmp_image
{
private:
	BMPFILEHEADER	bmpHeader;
	BMPINFOHEADER	bmpInfo;

protected:
	int	nWidth;		// ����
	int	nHeight;	// �c��

	RGBCOLOR* rgbImage;	// �摜�f�[�^�̖{��	

public:

	// �R���g���N�^
	bmp_image ()
	{
		nWidth = 0;
		nHeight = 0;
		rgbImage = NULL;
		//memset(&bmpHeader, 0, sizeof(BMPFILEHEADER));
		//memset(&bmpInfo, 0, sizeof(BMPINFOHEADER));
	};

	// �f�X�g���N�^
	~bmp_image ()
	{
		delete[] rgbImage;
	};

	// �摜�f�[�^�̐���
	int allocate_image (unsigned int width, unsigned int height);

	// �摜�f�[�^�̓f���o��
	int save_image (char* filename);

	// �w�b�_�̍쐬
	void set_header ();

	// �摜�f�[�^�ݒ�
	void assign_data (const char* data, FMT format);
};
