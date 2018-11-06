
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

	complex<float> Z, C(-0.805, 0.156);
	Z = complex<float>(1.0, 1.0);
	int MAX_ITERATIONS = 5;
	vector<float> colours(MAX_ITERATIONS + 1);

	for (int i = 0; i < MAX_ITERATIONS; i++) {
		Z = Z * Z + C;
		if (abs(Z) > 2.0) break;
	}
	current = 
	

	return 0;
}
