#include <iostream>
#include <direct.h> // _getcwd
#include "loadtiff.h"
#include "bmp.h"
#define ImageNum 1
//#define path "C:\\Users\\macro\\Desktop\\TIF\\"
#define path "..\\"
/// <summary>
/// main
/// 1.set const char name fullpath of tiff file.
/// 2.run.
/// </summary>
/// <returns></returns>
int main ()
{
	const char name[ImageNum][100] =
	{
				path "aaa.tif",
				//path "tire.tif",
				//path "totem.tif",
				//path "underwater_bmx.tif",
				//path "venus1.tif",
				//path "venus2.tif",
				//path "x31_f18.tif",
				//path "21188wmz.tif",
				//path "a2.tif",
				//path "201s39yp.tif",
				//path "at3_1m4_01.tif",
				//path "at3_1m4_02.tif",
				//path "at3_1m4_03.tif",
				//path "at3_1m4_04.tif",
				//path "at3_1m4_05.tif",
				//path "at3_1m4_06.tif",
				//path "at3_1m4_07.tif",
				//path "at3_1m4_08.tif",
				//path "at3_1m4_09.tif",
				//path "at3_1m4_10.tif",
				//path "autumn.tif",
				//path "balloons.tif",
				//path "board.tif",
				//path "body1.tif",
				//path "body2.tif",
				//path "body3.tif",
				//path "brain.tif",
				//path "brain_398.tif",
				//path "brain_492.tif",
				//path "brain_508.tif",
				//path "brain_604.tif",
				//path "cameraman.tif",
				//path "canoe.tif",
				//path "casablanca.tif",
				//path "cell.tif",
				//path "circuit_bw.tif",
				//path "columns.tif",
				//path "eight.tif",
				//path "f14.tif",
				//path "flagler.tif",
				//path "foliage.tif",
				//path "football_seal.tif",
				//path "forest.tif",
				//path "galaxy.tif",
				//path "hands.tif",
				//path "handsmat.tif",
				//path "kids.tif",
				//path "leaves.tif",
				//path "m83.tif",
				//path "marcie.tif",
				//path "mountain.tif",
				//path "mri.tif",
				//path "pout.tif",
				//path "roi_14.tif",
				//path "sampldoc.tif",
				//path "saturn.tif",
				//path "sc_logo.tif",
				//path "screws.tif",
				//path "scs_logo.tif",
				//path "shadow.tif",
				//path "spine.tif",
				//path "surf.tif",
	};
	TIFF tiff;

	for (int i = 0; i < ImageNum; i++) {
		tiff.file_read (const_cast<char*>(name[i]));
		auto data = tiff.load_tiff ();
		if (data == 0) {
			printf ("TIFF file unreadable %s\n", name[i]);
		}
		if (tiff.format == FMT::FMT_RGBA || tiff.format == FMT::FMT_GREYALPHA) {
			bmp_image		bmp;
			char work[256];
			sprintf_s<256> (work, "%s.bmp", name[i]);
			bmp.allocate_image (tiff.width, tiff.height);
			bmp.assign_data (reinterpret_cast<char*>(data), tiff.format);
			if (!bmp.save_image (work))
				printf ("error");
		}
	}
}
