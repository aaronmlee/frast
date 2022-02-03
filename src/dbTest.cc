#include "db.h"
#include "image.h"

#include <chrono>
#include <iostream>
#include <iomanip>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>


namespace {
int image_signature(const Image& i) {
	int n = i.size();
	int acc = 0;
	for (int j=0; j<n; j++) acc += static_cast<int>(i.buffer[j]);
	return acc;
}



int dumpTile(Dataset& dset, uint64_t z, uint64_t y, uint64_t x, int w, int h) {
	int32_t c = dset.channels();
	int ts = dset.tileSize();
	Image img { ts, ts, c }; img.alloc();

	if (x == -1 or y == -1) {
		uint64_t tlbr[4];
		dset.determineLevelAABB(tlbr, z);
		x = tlbr[0];
		y = tlbr[1];
		w = tlbr[2] - tlbr[0];
		h = tlbr[3] - tlbr[1];
	}

	auto cv_type = c == 1 ? CV_8U : c == 3 ? CV_8UC3 : CV_8UC4;
	cv::Mat mat ( ts * h, ts * w, cv_type );

	for (uint64_t yy=y, yi=0; yy<y+h; yy++, yi++)
	for (uint64_t xx=x, xi=0; xx<x+w; xx++, xi++) {
		BlockCoordinate coord { z,yy,xx };
		cv::Mat imgRef ( ts, ts, cv_type, img.buffer );
		if (dset.get(img, coord, nullptr)) {
			printf(" - accessed bad block %d %lu %lu.\n", z,yy,xx);
			imgRef = cv::Scalar{0};
			//return 1;
		}

		imgRef.copyTo(mat(cv::Rect({((int)xi)*ts, (int)(h-1-yi)*ts, ts, ts})));
	}

	if (img.channels() == 3) cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
	cv::imwrite("out/tile_" + std::to_string(z) + "_" + std::to_string(y) + "_" + std::to_string(x) + ".jpg", mat);
	return 0;
}

int rasterIo_it(DatasetReader& dset, double tlbr[4]) {
	Image img {512,512,3};
	img.alloc();

	if (dset.rasterIo(img, tlbr)) {
		printf(" - rasterIo failed.\n");
		fflush(stdout);
		return 1;
	}

	cv::Mat mat ( img.h, img.w, CV_8UC3, img.buffer );
	if (img.channels() == 3) cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
	cv::imwrite("out/rasterIoed.jpg", mat);
	return 0;
}
}

int main(int argc, char** argv) {
	if (argc > 1 and strcmp(argv[1],"dumpTile") == 0) {
		assert(argc == 6 or argc == 8);
		int z = std::atoi(argv[3]);
		int y = std::atoi(argv[4]);
		int x = std::atoi(argv[5]);
		int w = argc == 8 ? std::atoi(argv[6]) : -1;
		int h = argc == 8 ? std::atoi(argv[7]) : -1;
		Dataset dset(std::string{argv[2]});
		return dumpTile(dset, z,y,x, w,h);
	}

	if (argc > 1 and strcmp(argv[1],"rasterIo") == 0) {
		assert(argc == 7);
		double tlbr[4] = {
			std::atof(argv[3]),
			std::atof(argv[4]),
			std::atof(argv[5]),
			std::atof(argv[6]) };
		DatasetReader dset(std::string{argv[2]});
		return rasterIo_it(dset, tlbr);
	}


	return 0;
}
