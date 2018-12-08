
kernel void julia_kernel(
	global uchar* outputBuffer, // array of BGR pixels, length of size²*3 bytes (byte == uchar)
	const float2 C, 
	const uint max_iterations,
	const float limit,
	global uchar* colors,
	const uint colorCount
	)
{

	const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

	// Get id of element in buffer
	int x = get_global_id(0);
	int y = get_global_id(1);
	int size = get_global_size(0);
	
	float2 Z = (float2)(-limit + 2.0f * limit * x / size , -limit + 2.0f * limit * y / size);

	uint i;
	for (i = 0; i < max_iterations; i++) {
		Z = (float2)(Z.x*Z.x - Z.y*Z.y + C.x , Z.x*Z.y*2.0f + C.y);

		if (Z.x*Z.x + Z.y*Z.y > 4.0f){
			break;
		}
	}

	// this if-statement can be avoided by using a color array that contains black too. 
	if (i < max_iterations) { 
		uint ic =  i % colorCount * 3;
		i =(y * size + x)*3;
		outputBuffer[i] = colors[ic]; //B
		outputBuffer[i+1] = colors[ic+1]; //G
		outputBuffer[i+2] = colors[ic+2]; //R
	}
	//default color == black; else unnecesarry
}
