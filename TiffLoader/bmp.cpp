#include "bmp.h"

/// <summary>
/// ビットマップイメージをメモリ上に作成する。
/// </summary>
/// <param name="width"></param>
/// <param name="height"></param>
/// <returns>エラーコードを返す。基本的に1なら成功</returns>
int bmp_image::allocate_image(unsigned int width, unsigned int height) {
    this->nHeight = height;
    this->nWidth = width;

    rgbImage = new RGBCOLOR[width * height];
    return 1;
}

/// <summary>
/// ファイルにビットマップイメージを出力する
/// </summary>
/// <param name="filename">出力先のファイル名</param>
/// <returns>エラーコードを返す。基本的に1なら成功</returns>
int bmp_image::save_image(char* filename) {
    FILE* fp;
    fopen_s(&fp, filename, "wb");
    if (fp == NULL) {
        perror("Can't open the file.");
        return 0;
    }

    // ヘッダ部分を生成、書き込み
    set_header();
    fwrite(&bmpHeader, sizeof(BMPFILEHEADER), 1, fp);
    fwrite(&bmpInfo, sizeof(BMPINFOHEADER), 1, fp);

    // データ部分を一行ずつ書き込み
    unsigned int i;
    char buf[4] = "";
    for (i = 0;i < nHeight;i++) {
        fwrite(&rgbImage[i * nWidth], sizeof(RGBCOLOR), nWidth, fp);
        fwrite(buf, sizeof(char), nWidth % 4, fp);
    }

    fclose(fp);
    return 1;
}

/// <summary>
/// <summary>
/// ファイルヘッダ、情報部ヘッダを作成する
/// </summary>
void bmp_image::set_header() {
    int linedsize;

    // １行のバイトサイズの計算がめんどくさい！ｗ
    linedsize = (nWidth + 3) / 4;
    linedsize *= 4;

    // file header
    bmpHeader.bfReserved1 = 0;
    bmpHeader.bfReserved2 = 0;
    bmpHeader.bfType[0] = 'B';
    bmpHeader.bfType[1] = 'M';
    bmpHeader.bfOffBits = sizeof(BMPFILEHEADER) + sizeof(BMPINFOHEADER);
    bmpHeader.bfSize = linedsize * nHeight * 3 + bmpHeader.bfOffBits;

    // info header
    bmpInfo.biSize = sizeof(BMPINFOHEADER);
    bmpInfo.biWidth = nWidth;
    bmpInfo.biHeight = nHeight;
    bmpInfo.biPlanes = 1;
    bmpInfo.biBitCount = 24;
    bmpInfo.biCompression = 0;
    bmpInfo.biSizeImage = linedsize * nHeight * 3;	// １行あたりのバイト数×高さ×24bit
    bmpInfo.biXPelsPerMeter = 1;
    bmpInfo.biYPelsPerMeter = 1;
    bmpInfo.biClrUsed = 0;
    bmpInfo.biClrImportant = 0;

    return;
}

/// <summary>
/// データの割り当て
/// </summary>
/// <param name="data"></param>
void bmp_image::assign_data(const char* data) {

	int ptr = 0;
	for (auto i = 0;i < nHeight;i++) {
		for (auto j = 0;j < nWidth;j++) {
			rgbImage[(nHeight - i - 1) * nWidth + j].rgbBlue = data[ptr * 4 + 2];
			rgbImage[(nHeight - i - 1) * nWidth + j].rgbGreen = data[ptr * 4 + 1];
			rgbImage[(nHeight - i - 1) * nWidth + j].rgbRed = data[ptr * 4 + 0];
			ptr++;
		}
	}
    return;
}