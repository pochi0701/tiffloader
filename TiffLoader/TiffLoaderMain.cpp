#include <iostream>
#include <direct.h> // _getcwd
#include "loadtiff.h"
#include "bmp.h"
#define ImageNum 1
/// <summary>
/// main
/// 1.set const char name fullpath of tiff file.
/// 2.run.
/// </summary>
/// <returns></returns>
int main() {
    const char name[ImageNum][80] =
    {
        "..\\aaa.tif",
    };
    TIFF tiff;
    char tmp[256];
    _getcwd (tmp, 256);
    for (int i = 0; i < ImageNum; i++) {
        tiff.file_read((char*)name[i]);
        auto data = tiff.load_tiff();
        if (data == 0) {
            printf("TIFF file unreadable %s\n", name[i]);
        }
        if (tiff.format == FMT::FMT_RGBA) {
            bmp_image		bmp;
            char work[256];
            sprintf_s<256>(work, "%s.bmp", name[i]);
            bmp.allocate_image(tiff.width, tiff.height);
            bmp.assign_data((char*)data);
            if (!bmp.save_image(work))
                printf("エラーエラー");
        }
    }
}
