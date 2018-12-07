
kernel void julia_kernel(
	write_only image2d_t outputImage, 
	const float2 C, 
	const uint MAX_ITERATIONS,
	const float limit
	)
{

	const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

	// Get id of element in array
	int x = get_global_id(0);
	int y = get_global_id(1);
	int size = get_global_size(0); //square image
	
	float2 Z = (float2)(-limit + 2.0f * limit * x / size , -limit + 2.0f * limit * y / size);

	uint i;
	for (i = 0; i < MAX_ITERATIONS; i++) {
		Z = (float2)(Z.x*Z.x - Z.y*Z.y + C.x , Z.x*Z.y*2.0f + C.y);

		if (Z.x*Z.x + Z.y*Z.y > 4.0f){
			break;
		}
	}

	// this if-statement can be avoided by using a color array that contains black too.
	if (i < MAX_ITERATIONS) { 

		//BGRA
		uint4 color = (uint4)(i % 5 * 20 + 100, 0, 0, 0); //BGRA
		write_imageui(outputImage, (int2)(x, y), color);
	}
	//default color == black; else not necesarry
}
