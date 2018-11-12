
//
// TBB example 02 - 2D dataset processing using TBB
//


#include <iostream>
#include <random>
#include <chrono>
#include <vector>
#include <cmath>
#include <tbb/tbb.h>
#include <FreeImage\FreeImagePlus.h>
#include <complex>

using namespace std;
using namespace std::chrono;
using namespace tbb;




void serialJulia(const complex<float>  C, const UINT size = 1000, const UINT MAX_ITERATIONS = 100, const float limit = 1.7f) {

	// Setup output image
	fipImage outputImage;
	outputImage = fipImage(FIT_BITMAP, size, size, 24);
	UINT bytesPerElement = 3;
	BYTE* outputBuffer = outputImage.accessPixels();
	vector<int> colors{ 100, 140, 180, 220, 255 }; 


	complex<float> Z;

	std::cout << "Processing...\n";
	for (UINT y = 0; y < size; y++) {
		//tracking progress; remove during perfomance testing (performance hit of cout)
		cout << y * 100 / size << "%\r";
		cout.flush();
		for (UINT x = 0; x < size; x++) {

			Z = complex<float>(-limit + 2.0f * limit / size * x, -limit + 2.0f * limit / size * y);
			UINT i;
			for (i = 0; i < MAX_ITERATIONS; i++) {
				Z = Z * Z + C;
				if (abs(Z) > 2.0f) break;
			}
			if (i < MAX_ITERATIONS) { //only changing red byte
				outputBuffer[(y*size + x)*bytesPerElement + 2] = colors[i % 5];
			}
			//default color == black; else not necesarry
			
		}

	}

	cout << "Saving image...\n";
	ostringstream name;
	name << "images\\JuliaSet C=" << C << " size=" << size << " mIterations=" << MAX_ITERATIONS << " serial.png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";
}

void parallelJulia(const complex<float>  C, const UINT size = 1000, const UINT MAX_ITERATIONS = 100, const float limit = 1.7f) {

	// Setup output image
	fipImage outputImage;
	UINT bytesPerElement = 3;
	outputImage = fipImage(FIT_BITMAP, size, size, bytesPerElement*8);
	BYTE* outputBuffer = outputImage.accessPixels();
	vector<int> colors{ 100, 140, 180, 220, 255 };



	std::cout << "Processing[PARALLEL]...\n";

	parallel_for( 
		blocked_range2d<UINT,UINT>(0, size, 0, size),
		[=](const blocked_range2d<UINT, UINT>& r) {

		auto y1 = r.rows().begin();
		auto y2 = r.rows().end();
		auto x1 = r.cols().begin();
		auto x2 = r.cols().end();

		for (UINT y = y1; y < y2; y++) {
			//tracking progress not possible in the same way
			for (UINT x = x1; x < x2; x++) {

				complex<float> Z = complex<float>(-limit + 2.0f * limit / size * x, -limit + 2.0f * limit / size * y);
				UINT i;
				for (i = 0; i < MAX_ITERATIONS; i++) {
					Z = Z * Z + C;
					if (abs(Z) > 2.0f) break;
				}
				if (i < MAX_ITERATIONS) { //only changing red byte
					outputBuffer[(y*size + x)*bytesPerElement + 2] = colors[i % 5];
				}
				//default color == black; else not necesarry

			}
		}
	});

	cout << "Saving image...\n";
	ostringstream name;
	name << "images\\JuliaSet C=" << C << " size=" << size << " mIterations=" << MAX_ITERATIONS << " parallel.png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";
}



int main(void) {

	//complex<float>  C(-0.805f, 0.156f);
	complex<float> C(0.327f, 0.412f);
	int size = 10000;
	int MAX_ITERATIONS = 1000;
	float limit = 1.5f;

	//serialJulia(C, size, MAX_ITERATIONS, limit);
	parallelJulia(C, size, MAX_ITERATIONS, limit);
	


	return 0;
}


/*
Why can't you use sizes not dividable by 4?????
should canvas be square or adjustable?
better way to format filenames or dump variable data to file. 

*/
