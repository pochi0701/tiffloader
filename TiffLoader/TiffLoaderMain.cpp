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
int main ()
{
	const char name[ImageNum][100] =
	{
				"..\\aaa.tif",
				//"..\\tire.tif",
				//"..\\totem.tif",
				//"..\\underwater_bmx.tif",
				//"..\\venus1.tif",
				//"..\\venus2.tif",
				//"..\\x31_f18.tif",
				//"..\\21188wmz.tif",
				//"..\\a2.tif",
				//"..\\201s39yp.tif",
				//"..\\at3_1m4_01.tif",
				//"..\\at3_1m4_02.tif",
				//"..\\at3_1m4_03.tif",
				//"..\\at3_1m4_04.tif",
				//"..\\at3_1m4_05.tif",
				//"..\\at3_1m4_06.tif",
				//"..\\at3_1m4_07.tif",
				//"..\\at3_1m4_08.tif",
				//"..\\at3_1m4_09.tif",
				//"..\\at3_1m4_10.tif",
				//"..\\autumn.tif",
				//"..\\balloons.tif",
				//"..\\board.tif",
				//"..\\body1.tif",
				//"..\\body2.tif",
				//"..\\body3.tif",
				//"..\\brain.tif",
				//"..\\brain_398.tif",
				//"..\\brain_492.tif",
				//"..\\brain_508.tif",
				//"..\\brain_604.tif",
				//"..\\cameraman.tif",
				//"..\\canoe.tif",
				//"..\\casablanca.tif",
				//"..\\cell.tif",
				//"..\\circuit_bw.tif",
				//"..\\columns.tif",
				//"..\\eight.tif",
				//"..\\f14.tif",
				//"..\\flagler.tif",
				//"..\\foliage.tif",
				//"..\\football_seal.tif",
				//"..\\forest.tif",
				//"..\\galaxy.tif",
				//"..\\hands.tif",
				//"..\\handsmat.tif",
				//"..\\kids.tif",
				//"..\\leaves.tif",
				//"..\\m83.tif",
				//"..\\marcie.tif",
				//"..\\mountain.tif",
				//"..\\mri.tif",
				//"..\\pout.tif",
				//"..\\roi_14.tif",
				//"..\\sampldoc.tif",
				//"..\\saturn.tif",
				//"..\\sc_logo.tif",
				//"..\\screws.tif",
				//"..\\scs_logo.tif",
				//"..\\shadow.tif",
				//"..\\spine.tif",
				//"..\\surf.tif",
	};
	TIFF tiff;

	for (int i = 0; i < ImageNum; i++) {
		tiff.file_read ((char*)name[i]);
		auto data = tiff.load_tiff ();
		if (data == 0) {
			printf ("TIFF file unreadable %s\n", name[i]);
		}
		if (tiff.format == FMT::FMT_RGBA || tiff.format == FMT::FMT_GREYALPHA) {
			bmp_image		bmp;
			char work[256];
			sprintf_s<256> (work, "%s.bmp", name[i]);
			bmp.allocate_image (tiff.width, tiff.height);
			bmp.assign_data ((char*)data, tiff.format);
			if (!bmp.save_image (work))
				printf ("error");
		}
	}
}
