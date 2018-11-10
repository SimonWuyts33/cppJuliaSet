
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




void serialJulia(const complex<float>  C, const int size = 1000, const int MAX_ITERATIONS = 100, const float limit = 1.7f) {

	// Setup output image
	fipImage outputImage;
	outputImage = fipImage(FIT_BITMAP, size, size, 24);
	int bytesPerElement = 3;
	BYTE* outputBuffer = outputImage.accessPixels();
	vector<int> colors{ 100, 140, 180, 220, 255 };


	complex<float> Z;

	std::cout << "Processing...\n";
	for (int y = 0; y < size; y++) {
		cout << y * 100 / size << "%\r";
		cout.flush();
		for (int x = 0; x < size; x++) {

			Z = complex<float>(-limit + 2.0f * limit / size * x, -limit + 2.0f * limit / size * y);
			int i;
			for (i = 0; i < MAX_ITERATIONS; i++) {
				Z = Z * Z + C;
				if (abs(Z) > 2.0f) break;
			}
			if (i < MAX_ITERATIONS) {
				outputBuffer[(y*size + x)*bytesPerElement + 2] = colors[i % 5];
			}
			/*else { //default color == black; not necesarry
				
				outputBuffer[(y*size + x)*bytesPerElement] = 0;
				outputBuffer[(y*size + x)*bytesPerElement + 1] = 0;
				outputBuffer[(y*size + x)*bytesPerElement + 2] = 0;
			}*/
		}

	}

	cout << "Saving image...\n";
	ostringstream name;
	name << "images\\JuliaSet C=" << C << " size=" << size << " mIterations=" << MAX_ITERATIONS << ".png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";
}


int main(void) {

	complex<float>  C(-0.805f, 0.156f);
	int size = 10000;
	int MAX_ITERATIONS = 20;
	float limit = 1.5f;

	serialJulia(C, size, MAX_ITERATIONS, limit);


	return 0;
}
