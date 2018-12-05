kernel void julia_kernel(
	write_only image2d_t outputImage, 
	const float cReal, 
	const float cImag, 
	const uint MAX_ITERATIONS,
	const float limit
	)
{

	const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

	// Get id of element in array
	int x = get_global_id(0);
	int y = get_global_id(1);
	int size = get_global_size(0); //square image
	

	float zReal = -limit + 2.0 * limit / size * x;
	float zImag = -limit + 2.0 * limit / size * y;
	uint i;
	for (i = 0; i < MAX_ITERATIONS; i++) {
		zReal = zReal*zReal - zImag*zImag + cReal;
		zImag = zReal*zImag*2.0 +cImag;
		if (zReal*zReal + zImag*zImag > 4.0){
			break;
		}
	}

	// this if-statement can be avoided by using a color array that contains black too.
	if (i < MAX_ITERATIONS) { 

		//BGRA
		uint4 color = (uint4)(i % 5 * 20 + 100, 0, 0, 0);
		write_imageui(outputImage, (int2)(x, y), color);
	}
	//default color == black; else not necesarry
}
