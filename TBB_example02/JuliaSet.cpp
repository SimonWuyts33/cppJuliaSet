
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
#include "setup_cl.h"

using namespace std;
using namespace std::chrono;
using namespace tbb;

struct BGRcolor {

	byte b, g, r;
};

class JuliaGenerator {
public:
	complex<float> C;
	float limit;
	vector<BGRcolor> colors;

	JuliaGenerator(const complex<float>  C, const float limit = 1.7f, vector<BGRcolor> colors = { { 100 ,0 ,0 }, { 140, 0, 0 }, { 180,0,0 }, { 220,0,0 }, { 255, 0, 0 } }) : C(C), limit(limit), colors(colors) {};

	virtual void operator ()(BGRcolor* outputBuffer, const UINT size, const UINT MAX_ITERATIONS) = 0;
	
	void cppKernel(UINT x, UINT y, BGRcolor* outputBuffer, const UINT size, const UINT MAX_ITERATIONS)
	{
		complex<float> Z = complex<float>(-limit + 2.0f * limit / size * x, -limit + 2.0f * limit / size * y);
		UINT i;
		for (i = 0; i < MAX_ITERATIONS; i++) {
			Z = Z * Z + C;
			if (abs(Z) > 2.0f) break;
		}
		if (i < MAX_ITERATIONS) {
			outputBuffer[y*size + x] = colors[i % 5];
		}
	}
};

class SequentialJulia : public JuliaGenerator {
public:
	SequentialJulia(const complex<float>  C) : JuliaGenerator(C) {};

	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT MAX_ITERATIONS) {
		for (UINT y = 0; y < size; y++) {
			for (UINT x = 0; x < size; x++) {
				cppKernel(x, y, outputBuffer, size, MAX_ITERATIONS);
			}

		}
	}
};

class TBBJulia : public JuliaGenerator {
public:
	TBBJulia(const complex<float>  C) : JuliaGenerator(C) {};

	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT MAX_ITERATIONS) {
		parallel_for(
			blocked_range2d<UINT, UINT>(0, size, 0, size),
			[=](const blocked_range2d<UINT, UINT>& r) {

				auto y1 = r.rows().begin();
				auto y2 = r.rows().end();
				auto x1 = r.cols().begin();
				auto x2 = r.cols().end();

				for (UINT y = y1; y < y2; y++) {
					for (UINT x = x1; x < x2; x++) {
						cppKernel(x, y, outputBuffer, size, MAX_ITERATIONS);
					}
				}
		});
	}
};

class OpenCLJulia : public JuliaGenerator {
private:
	cl_context context;
	cl_command_queue commandQueue;
	cl_kernel kernel;
	double lastExecutionTimeSec;

public:
	const double getLastExecutionTimeInSeconds() const{
		return lastExecutionTimeSec;
	}
	OpenCLJulia(const complex<float>  C, const char * platform = "INTEL") : JuliaGenerator(C), lastExecutionTimeSec (-1.0){

		context = createContext(platform);
		if (!context) {
			cout << "OpenCL context not created\n";
			return;
		}
		size_t deviceBufferSize;
		clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, nullptr, &deviceBufferSize);
		cl_device_id *contextDevices = (cl_device_id*)malloc(deviceBufferSize);
		clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceBufferSize, contextDevices, nullptr);
		cl_device_id device = contextDevices[0];

		commandQueue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, nullptr);
		if (!commandQueue) {
			cout << "Command queue not created\n";
			return;
		}

		cl_program program = createProgram(context, device, "julia.cl");
		kernel = clCreateKernel(program, "julia_kernel", nullptr);
		if (!kernel) {
			cout << "Could not create kernel: julia_kernel\n";
			return;
		}
		
	};

	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT MAX_ITERATIONS) {

		cl_int err = 0;
		// Setup output object: openCL Image2D
		cl_image_format outputFormat;
		outputFormat.image_channel_order = CL_RGB;
		outputFormat.image_channel_data_type = CL_UNSIGNED_INT8;

		cl_mem	imageBuffer = 0;
		imageBuffer = clCreateImage2D(context, CL_MEM_WRITE_ONLY, &outputFormat, size, size, 0, 0, &err);

		if (!imageBuffer) {
			cout << "Cannot create output image object \n";
			cout << "ErrorCode:" << err << "\n";
			return;
		}

		// Setup memory object -> kernel parameter bindings
		cl_float2 Cfloat2;
		Cfloat2.s[0] = C.real();
		Cfloat2.s[1] = C.imag();

		clSetKernelArg(kernel, 0, sizeof(cl_mem), &imageBuffer);
		clSetKernelArg(kernel, 1, sizeof(cl_float2), &Cfloat2);
		clSetKernelArg(kernel, 2, sizeof(cl_uint), &MAX_ITERATIONS);
		clSetKernelArg(kernel, 3, sizeof(cl_float), &limit);

		// Setup worksize arrays
		size_t globalWorkSize[2] = { size, size };

		// Setup event (for profiling)
		cl_event juliaEvent;

		// Enqueue kernel
		err = clEnqueueNDRangeKernel(commandQueue, kernel, 2, 0, globalWorkSize, 0, 0, 0, &juliaEvent);

		// Block until  kernel finishes and report time taken to run the kernel
		clWaitForEvents(1, &juliaEvent);

		cl_ulong startTime = (cl_ulong)0;
		cl_ulong endTime = (cl_ulong)0;

		clGetEventProfilingInfo(juliaEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, 0);
		clGetEventProfilingInfo(juliaEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, 0);

		lastExecutionTimeSec = (double)(endTime - startTime) * 1.0e-9;


		// Extract the resulting image from OpenCL
		fipImage outputImage = fipImage(FIT_BITMAP, size, size, 32);

		if (!outputImage.isValid()) {
			cout << "Cannot create the output image \n";
			return;
		}

		size_t origin[3] = { 0, 0, 0 };
		size_t region[3] = { size, size, 1 };

		err = clEnqueueReadImage(commandQueue, imageBuffer, CL_TRUE, origin, region, 0, 0, outputBuffer, 0, 0, 0);
		/*
		outputImage.convertTo24Bits();
		outputImage.save("images\\temptest.png");
		outputBuffer = (BGRcolor*)outputImage.accessPixels();
		*/
	}
};
void generateJulia(JuliaGenerator& generator, const UINT size, const UINT MAX_ITERATIONS, const char* filename = 0) {

	// Setup output image
	fipImage outputImage = fipImage(FIT_BITMAP, size, size, 24);
	BGRcolor* outputBuffer = (BGRcolor*)outputImage.accessPixels();

	cout << "Processing...\n";
	steady_clock::time_point start(std::chrono::steady_clock::now());
	
	generator(outputBuffer, size, MAX_ITERATIONS);

	steady_clock::time_point end(std::chrono::steady_clock::now());
	cout << "processing time in seconds = " << duration_cast<duration<double>>(end - start).count() << "\n";

	cout << "Saving image...\n";
	ostringstream name;
	if (filename == 0) {
		name << "images\\JuliaSet C=" << generator.C << " size=" << size << " mIterations=" << MAX_ITERATIONS << ".png";
	}
	else { name << "images\\" << filename << ".png"; }
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";
}

void serialJulia(const complex<float>  C, const UINT size = 1000, const UINT MAX_ITERATIONS = 100, const float limit = 1.7f) {

	// Setup output image
	fipImage outputImage;
	outputImage = fipImage(FIT_BITMAP, size, size, 24);
	UINT bytesPerElement = 3;
	BGRcolor* outputBuffer = (BGRcolor*) outputImage.accessPixels();
	// BGR format
	vector<BGRcolor> colors{ {100 ,0 ,0}, {140, 0, 0}, {180,0,0}, {220,0,0}, {255, 0, 0} };


	complex<float> Z;
	steady_clock::time_point start(std::chrono::steady_clock::now());
	std::cout << "Processing...\n";
	for (UINT y = 0; y < size; y++) {
		//tracking progress; remove during perfomance testing (performance hit of cout)
		//cout << y * 100 / size << "%\r";
		//cout.flush();
		for (UINT x = 0; x < size; x++) {

			Z = complex<float>(-limit + 2.0f * limit / size * x, -limit + 2.0f * limit / size * y);
			UINT i;
			for (i = 0; i < MAX_ITERATIONS; i++) {
				Z = Z * Z + C;
				if (abs(Z) > 2.0f) break;
			}
			if (i < MAX_ITERATIONS) { //only changing red byte
				outputBuffer[y*size + x] = colors[i % 5];
			}
			//default color == black; else not necesarry
			
		}

	}
	steady_clock::time_point end(std::chrono::steady_clock::now());
	cout << "processing time in seconds = " << duration_cast<duration<double>>(end - start).count() << "\n";
	cout << "Saving image...\n";
	ostringstream name;
	name << "images\\JuliaSet C=" << C << " size=" << size << " mIterations=" << MAX_ITERATIONS << " serial.png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";
}

void tbbjulia(const complex<float>  C, const UINT size = 1000, const UINT MAX_ITERATIONS = 100, const float limit = 1.7f) {

	// Setup output image
	fipImage outputImage;
	UINT bytesPerElement = 3;
	outputImage = fipImage(FIT_BITMAP, size, size, bytesPerElement*8);
	BYTE* outputBuffer = outputImage.accessPixels();
	vector<int> colors{ 100, 140, 180, 220, 255 };
	// byte or char ipv int? 


	std::cout << "Processing[PARALLEL]...\n";

	steady_clock::time_point start(std::chrono::steady_clock::now());

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
	steady_clock::time_point end(std::chrono::steady_clock::now());
	cout << "processing time in seconds = " << duration_cast<duration<double>>(end - start).count() << "\n";

	cout << "Saving image...\n";
	ostringstream name;
	name << "images\\JuliaSet C=" << C << " size=" << size << " mIterations=" << MAX_ITERATIONS << " parallel.png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	outputImage.save(name.str().c_str());

	cout << "...done\n\n";
}



void openCLJulia(const complex<float>  C, const UINT size = 1000, const UINT MAX_ITERATIONS = 100, const float limit = 1.7f) {
	
	// Setup output image
	fipImage outputImage;
	UINT bytesPerElement = 3;
	//outputImage = fipImage(FIT_BITMAP, size, size, bytesPerElement * 8);
	BYTE* outputBuffer = outputImage.accessPixels();
	vector<int> colors{ 100, 140, 180, 220, 255 };



	std::cout << "Processing[opencl]...\n";



	////////////////////////////////////////////////////

	
	//
	// 1. Create and validate the OpenCL context (default to nVidia platform)
	//
	cl_context context = createContext("INTEL");

	if (!context) {

		cout << "OpenCL context not created\n";
		return;
	}


	//
	// 2. Query the device from the context - should be the GPU since we requested this before
	//
	size_t deviceBufferSize;

	cl_int errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, nullptr, &deviceBufferSize);

	cl_device_id *contextDevices = (cl_device_id*)malloc(deviceBufferSize);

	errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceBufferSize, contextDevices, nullptr);

	cl_device_id		device = contextDevices[0];


	//
	// 3. Create and validate the command queue (for first device in context)
	// Note: Add the 'CL_QUEUE_PROFILING_ENABLE' profiling flag so we can get timing data from event!!
	//
	cl_command_queue commandQueue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, nullptr);

	if (!commandQueue) {

		cout << "Command queue not created" << endl;
		return;

	}

	//
	// 4. Create and validate the program object based on julia.cl
	//
	cl_program program = createProgram(context, device, "julia.cl");


	//
	// 5. Get "hello_world kernel" from program object created above
	//
	cl_kernel kernel = clCreateKernel(program, "julia_kernel", nullptr);

	if (!kernel) {

		cout << "Could not create kernel: julia_kernel" << endl;
		return;
	}

	//
	// 6. Create memory objects to be used as arguments to the kernel
	//




	cl_int	err = 0;

		// Setup output image
	cl_image_format outputFormat;
	outputFormat.image_channel_order = CL_RGBA;
	outputFormat.image_channel_data_type = CL_UNSIGNED_INT8;

	cl_mem	imageBuffer = 0;
	imageBuffer = clCreateImage2D(context, CL_MEM_WRITE_ONLY, &outputFormat, size, size, 0, 0, &err);

	if (!imageBuffer) {
		cout << "Cannot create output image object \n";
		cout << err << endl;
	}


	// Setup memory object -> kernel parameter bindings
	cl_float2 Cfloat2;
	Cfloat2.s[0] = C.real();
	Cfloat2.s[1] = C.imag();

	clSetKernelArg(kernel, 0, sizeof(cl_mem), &imageBuffer);
	clSetKernelArg(kernel, 1, sizeof(cl_float2), &Cfloat2);
	clSetKernelArg(kernel, 2, sizeof(cl_uint), &MAX_ITERATIONS);
	clSetKernelArg(kernel, 3, sizeof(cl_float), &limit);

	// Setup worksize arrays
	size_t globalWorkSize[2] = { size, size };

	// Setup event (for profiling)
	cl_event juliaEvent;

	// Enqueue kernel
	err = clEnqueueNDRangeKernel(commandQueue, kernel, 2, 0, globalWorkSize, 0, 0, 0, &juliaEvent);

	// Block until voronoi kernel finishes and report time taken to run the kernel
	clWaitForEvents(1, &juliaEvent);

	cl_ulong startTime = (cl_ulong)0;
	cl_ulong endTime = (cl_ulong)0;

	clGetEventProfilingInfo(juliaEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, 0);
	clGetEventProfilingInfo(juliaEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, 0);

	double tdelta = (double)(endTime - startTime);

	std::cout << "Time taken (in seconds) to create the julia diagram = " << (tdelta * 1.0e-9) << endl;


	// Extract the resulting voronoi diagram image from OpenCL
	outputImage = fipImage(FREE_IMAGE_TYPE::FIT_BITMAP, size, size, 32);
	
	if (!outputImage.isValid())
		cout << "Cannot create the output image here \n";

	size_t origin[3] = { 0, 0, 0 };
	size_t region[3] = { size, size, 1 };

	err = clEnqueueReadImage(commandQueue, imageBuffer, CL_TRUE, origin, region, 0, 0, outputImage.accessPixels(), 0, 0, 0);
	
	outputImage.convertTo24Bits();

	cout << "Saving image...\n" <<endl;
	ostringstream name;
	name << "images\\OpenCL C=" << C << " size=" << size << " mIterations=" << MAX_ITERATIONS << ".png"; //moet een beter oplossing voor zijn eh???
	cout << "saving in: " << name.str().c_str() << "\n";
	bool saved = outputImage.save(name.str().c_str());

	if (!saved)
		cout << "Cannot save image" <<endl;
	cout << "...done\n\n" <<endl;



}



int main(void) {

	complex<float>  C(-0.805f, 0.156f);
	//complex<float> C(0.327f, 0.412f);
	int size = 1000;
	int MAX_ITERATIONS = 800;
	float limit = 1.7f;
	SequentialJulia seq(C);
	TBBJulia tbb(C);
	OpenCLJulia opencl(C);

	generateJulia(opencl, size, MAX_ITERATIONS, "opencl");
	cout << "OpenCLtime" << opencl.getLastExecutionTimeInSeconds() << endl;
	/*
	cl_context context = createContext("NVIDIA");
	cl_uint number;
	cl_image_format formats[44];
	clGetSupportedImageFormats(context,
		CL_MEM_WRITE_ONLY,
		CL_MEM_OBJECT_IMAGE2D,
		44,
		formats,
		&number);
	for (int i = 0; i < number; i++) {
		cout << formats[i].image_channel_order << "   " << formats[i].image_channel_data_type << "\n";
	}

	cout << "\n number :" << number << endl;*/

	//TBBJulia(C, size, MAX_ITERATIONS, limit);
	return 0;
}


/*
Why can't you use sizes not divisable by 4?????
*/

