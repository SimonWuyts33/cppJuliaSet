
kernel void julia_kernel(
	global uchar3* outputBuffer, // array of BGR pixels, length of size²
	const float2 C, 
	const uint MAX_ITERATIONS,
	const float limit
	)
{

	const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

	// Get id of element in buffer
	int x = get_global_id(0);
	int y = get_global_id(1);
	int size = get_global_size(0);
	
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
		uchar3 color = (uchar3)(i % 5 * 20 + 100, 0, 0); //BGRA
		outputBuffer[y * size + x] = color; //BGR
	}
	//default color == black; else unnecesarry
}
