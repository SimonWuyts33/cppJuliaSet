
//
// Generating a JuliaSet image in 3 different ways to show the possibilities of Parallel Programming
//

#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <vector>
#include <tbb/tbb.h>
#include <FreeImage\FreeImagePlus.h>
#include <complex>
#include "setup_cl.h"

using namespace std;
using namespace std::chrono;
using namespace tbb;

// Struct of a pixel with 24 bits BGR color
struct BGRcolor {

	byte b, g, r;
};


//Base class for generator functors
class JuliaGenerator {
public:
	// Constant complex number used in Julia algorithm
	complex<float> C;
	// Range of complex numbers mapped to image size. [-limit -i*limit to limit + i*limit]
	float limit;
	// Vector of colors assigned to pixels in output image.
	vector<BGRcolor> colors;

	JuliaGenerator(const complex<float>  C, const float limit = 1.7f, vector<BGRcolor> colors = { { 100 ,0 ,0 }, { 130, 0,0 }, { 160,0,0 }, { 190,0,0 }, { 230, 0, 0 } }) : C(C), limit(limit), colors(colors) {};

	// Virtual functor
	virtual void operator ()(BGRcolor* outputBuffer, const UINT size, const UINT max_iterations) = 0;
	
	// used in operator<<
	virtual ostream& out(ostream& ostr) const = 0;

	// Kernel is the same for TBB and sequential version
	void cppKernel(UINT x, UINT y, BGRcolor* outputBuffer, const UINT size, const UINT max_iterations)
	{
		complex<float> Z = complex<float>(-limit + 2.0f * limit / size * x, -limit + 2.0f * limit / size * y);
		UINT i;
		for (i = 0; i < max_iterations; i++) {
			Z = Z * Z + C;
			if (abs(Z) > 2.0f) break;
		}
		if (i < max_iterations) {
			outputBuffer[y*size + x] = colors[i % colors.size()];
		}
		//default color == black; else unnecesarry
	}
};

// Makes it possible to output the used version of the algorithme to a stream
ostream& operator<<(ostream& ostr, const JuliaGenerator& julia) {
	return julia.out(ostr);
}

// Sequential implementation to generate JuliaSet image
class SequentialJulia : public JuliaGenerator {
public:
	SequentialJulia(const complex<float>  C) : JuliaGenerator(C) {};
	ostream& out(ostream& ostr) const { return ostr << "sequential"; }
	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT max_iterations) {
		for (UINT y = 0; y < size; y++) {
			for (UINT x = 0; x < size; x++) {
				cppKernel(x, y, outputBuffer, size, max_iterations);
			}

		}
	}
};

// Parallel implementation (on CPU) using Intel's Threading Building Blocks library
class TBBJulia : public JuliaGenerator {
public:
	TBBJulia(const complex<float>  C) : JuliaGenerator(C) {};
	ostream& out(ostream& ostr) const { return ostr << "TBB"; }
	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT max_iterations) {
		parallel_for(
			blocked_range2d<UINT, UINT>(0, size, 0, size),
			[=](const blocked_range2d<UINT, UINT>& r) {

				auto y1 = r.rows().begin();
				auto y2 = r.rows().end();
				auto x1 = r.cols().begin();
				auto x2 = r.cols().end();

				for (UINT y = y1; y < y2; y++) {
					for (UINT x = x1; x < x2; x++) {
						cppKernel(x, y, outputBuffer, size, max_iterations);
					}
				}
		});
	}
};

// Parallel implemetation (on GPU) using OpenCL
class OpenCLJulia : public JuliaGenerator {
private:
	// Reusable resources created in constructor
	cl_context context;
	cl_command_queue commandQueue;
	cl_kernel kernel;
	cl_program program;
	// Time measured with cl_event during last execution of operator(). -1 when not yet run or error during last execution
	double lastExecutionTimeSec;

public:
	// platform used to creat context. Default is 'INTEL', change appropriately to used system. Running code will print device information to cout
	const char * platform;
	// getter exposing private variable
	const double getLastExecutionTimeInSeconds() const{
		return lastExecutionTimeSec;
	}
	ostream& out(ostream& ostr) const { return ostr << "OpenCL (platform=" << platform << ")"; }

	// Constructor creating all necesarry reusable resources.
	OpenCLJulia(const complex<float>  C, const char * platform = "INTEL") : JuliaGenerator(C), platform(platform), lastExecutionTimeSec (-1.0){

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

		program = createProgram(context, device, "julia.cl");
		kernel = clCreateKernel(program, "julia_kernel", nullptr);
		if (!kernel) {
			cout << "Could not create kernel: julia_kernel\n";
			return;
		}
		
	};

	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT max_iterations) {
		// Resetime, so no invalid time is shown when exception is thrown
		lastExecutionTimeSec = -1;
		cl_int err = 0;
		cl_mem imageBuffer = 0;
		//Create memory buffer for image 
		imageBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY , size * size * sizeof(BGRcolor), nullptr, &err);
		if (!imageBuffer) {
			cout << "\nCannot create output imageBuffer \n";
			cout << "ErrorCode:" << err << endl;
			clReleaseMemObject(imageBuffer);
			return;
		}
		UINT colorCount = colors.size();
		cl_mem colorsBuffer = 0;
		//Create memory buffer for array of colors using the 'colors' array 
		colorsBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, colorCount * sizeof(BGRcolor), &colors[0], &err);
		if (!colorsBuffer) {
			cout << "\nCannot create colorsBuffer \n";
			cout << "ErrorCode:" << err << endl;
			clReleaseMemObject(imageBuffer);
			clReleaseMemObject(colorsBuffer);
			return;
		}

		cl_float2 Cfloat2;
		Cfloat2.s[0] = C.real();
		Cfloat2.s[1] = C.imag();

		// Setup memory object -> kernel parameter bindings
		clSetKernelArg(kernel, 0, sizeof(cl_mem), &imageBuffer);
		clSetKernelArg(kernel, 1, sizeof(cl_float2), &Cfloat2);
		clSetKernelArg(kernel, 2, sizeof(cl_uint), &max_iterations);
		clSetKernelArg(kernel, 3, sizeof(cl_float), &limit);
		clSetKernelArg(kernel, 4, sizeof(cl_mem), &colorsBuffer);
		clSetKernelArg(kernel, 5, sizeof(cl_uint), &colorCount);
		
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

		err = clEnqueueReadBuffer(commandQueue, imageBuffer, CL_TRUE, 0, size * size * sizeof(BGRcolor), outputBuffer, 0, nullptr, nullptr);

		// Release memory buffers
		clReleaseMemObject(imageBuffer);
		clReleaseMemObject(colorsBuffer);


	}
	~OpenCLJulia() {
		// Release reusable resources created in constructor
		clReleaseKernel(kernel);
		clReleaseProgram(program);
		clReleaseCommandQueue(commandQueue);
		clReleaseContext(context);
	}
};

// Generates JuliaSet Image using the provided generator to do the actual calculations. 
double generateJulia(JuliaGenerator& generator, const UINT size, const UINT max_iterations, const char* filename = 0) {
	// Setup output image
	fipImage outputImage = fipImage(FIT_BITMAP, size, size, 24);
	BGRcolor* outputBuffer = (BGRcolor*)outputImage.accessPixels();

	cout << "Processing...\n";
	// Start timing
	steady_clock::time_point start(std::chrono::steady_clock::now());
	// Generate image using the generator functor
	generator(outputBuffer, size, max_iterations);
	// End timing
	steady_clock::time_point end(std::chrono::steady_clock::now());
	double executionTime = duration_cast<duration<double>>(end - start).count();
	cout << "processing time in seconds = " << executionTime << "\n";

	cout << "Saving image...\n";
	ostringstream name;
	if (filename == 0) {
		name << "images\\JuliaSet C=" << generator.C << " size=" << size << " mIterations=" << max_iterations <<" made by " << generator << ".png";
	}
	else { name << "images\\" << filename << ".png"; }
	cout << "saving in: " << name.str().c_str() << endl;
	outputImage.save(name.str().c_str());

	cout << "...done\n" << endl;
	return executionTime;
}

// Generates a CSV file (semicolon separated), containing timing data and the used variables.
void test(complex<float>  C, vector <pair<UINT, UINT>> size_iterationsList) {

	ofstream out("testdata.csv", ios::app);
	out << endl;

	out << "Constant=;" << C << endl;
	out << "range=;" << "( [-1.7f, 1.7f], [-1.7f, 1.7f] )\n" << endl;

	SequentialJulia seq(C);
	TBBJulia tbb(C);
	OpenCLJulia opencl(C);
	OpenCLJulia openclNvidia(C, "NVIDIA");


	out << "Variables;;Timings OpenCL;;;;C++" << endl;
	out << "size;max_iterations;Nvidia;Nvidia event;Intel;Intel event;TBB;Sequential " << endl;

	for each(pair<UINT, UINT> vars in size_iterationsList) {
		UINT size = vars.first;
		UINT max_iterations = vars.second;
		out << size << ";" << max_iterations << ";";

		out << generateJulia(openclNvidia, size, max_iterations) << ";";
		out << openclNvidia.getLastExecutionTimeInSeconds() << ";";

		out << generateJulia(opencl, size, max_iterations) << ";";
		out << opencl.getLastExecutionTimeInSeconds() << ";";

		out << generateJulia(tbb, size, max_iterations) << ";";

		out << generateJulia(seq, size, max_iterations) << endl;
	}


}

int main(void) {
	vector < pair<UINT, UINT>> list = {

	{6000, 2000 },
	};

	complex<float> C1(-0.805f, 0.156f);
	test(C1, list);
	return 0;
}