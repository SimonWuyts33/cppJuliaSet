
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



int main(void) {

	//Image variables
	int imgSize = 10000; // Pixel dimensions of generated square image
	float limit = 1.6f; // Range of calculated complex numbers ( -limit -> + limit in both real and imaginary parts)

	// Setup output image array
	fipImage outputImage;
	outputImage = fipImage(FIT_BITMAP, imgSize, imgSize, 24); 
	int bytesPerElement = 3;
	BYTE* outputBuffer = outputImage.accessPixels();
	vector<int> colors{ 100, 140, 180, 220, 255 };

	int MAX_ITERATIONS = 1000;
	//complex<float> Z, C(-0.805f, 0.156f); // input constant. 
	complex<float> Z, C(0.305f, 0.256f);
	std::cout << "Processing...\n";
	for (int y = 0; y < imgSize; y++) {
		cout << y*100/imgSize << "%\r";
		cout.flush();
		for (int x = 0; x < imgSize; x++) {

			Z = complex<float>(-limit + 2.0f * limit / imgSize * x, -limit + 2.0f * limit / imgSize * y);
			int i;
			for (i = 0; i < MAX_ITERATIONS; i++) {
				Z = Z * Z + C;
				if (abs(Z) > 2.0f) break;
			}
			if (i < MAX_ITERATIONS) {
				outputBuffer[(y*imgSize + x)*bytesPerElement + 2] = colors[i % 5];
			}
			else {

				outputBuffer[(y*imgSize + x)*bytesPerElement] = 0;
				outputBuffer[(y*imgSize + x)*bytesPerElement + 1] = 0;
				outputBuffer[(y*imgSize + x)*bytesPerElement + 2] = 0;
			}
		}

	}
		
	cout << "Saving image...\n";
	ostringstream name;
	name << "images\\JuliaSet C=" << C << " size=" << imgSize << " mIterations=" << MAX_ITERATIONS << ".png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";


	
	

	return 0;
}

/*
-- - x-- -
| a  b  c
y   d  i  e
| f  g  h
*/