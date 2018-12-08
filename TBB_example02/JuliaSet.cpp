
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

	JuliaGenerator(const complex<float>  C, const float limit = 1.7f, vector<BGRcolor> colors = { { 100 ,0 ,0 }, { 130, 0,0 }, { 160,0,0 }, { 190,0,0 }, { 230, 0, 0 } }) : C(C), limit(limit), colors(colors) {};

	virtual void operator ()(BGRcolor* outputBuffer, const UINT size, const UINT max_iterations) = 0;
	virtual ostream& out(ostream& ostr) const = 0;

	//
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

ostream& operator<<(ostream& ostr, const JuliaGenerator& julia) {
	return julia.out(ostr);
}


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


class OpenCLJulia : public JuliaGenerator {
private:
	cl_context context;
	cl_command_queue commandQueue;
	cl_kernel kernel;
	double lastExecutionTimeSec;

public:
	const char * platform;

	const double getLastExecutionTimeInSeconds() const{
		return lastExecutionTimeSec;
	}
	ostream& out(ostream& ostr) const { return ostr << "OpenCL (platform=" << platform << ")"; }
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

		cl_program program = createProgram(context, device, "julia.cl");
		kernel = clCreateKernel(program, "julia_kernel", nullptr);
		if (!kernel) {
			cout << "Could not create kernel: julia_kernel\n";
			return;
		}
		
	};

	void operator()(BGRcolor* outputBuffer, const UINT size, const UINT max_iterations) {
		lastExecutionTimeSec = -1;
		cl_int err = 0;
		cl_mem imageBuffer = 0;
		imageBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY , size * size * sizeof(BGRcolor), nullptr, &err);
		if (!imageBuffer) {
			cout << "Cannot create output imageBuffer \n";
			cout << "ErrorCode:" << err << "\n";
			return;
		}
		UINT colorCount = colors.size();
		cl_mem colorsBuffer = 0;
		colorsBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, colorCount * sizeof(BGRcolor), &colors[0], &err);
		if (!colorsBuffer) {
			cout << "Cannot create colorsBuffer \n";
			cout << "ErrorCode:" << err << "\n";
			return;
		}

		// Setup memory object -> kernel parameter bindings
		cl_float2 Cfloat2;
		Cfloat2.s[0] = C.real();
		Cfloat2.s[1] = C.imag();

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

	}
};
double generateJulia(JuliaGenerator& generator, const UINT size, const UINT max_iterations, const char* filename = 0) {

	// Setup output image
	fipImage outputImage = fipImage(FIT_BITMAP, size, size, 24);
	BGRcolor* outputBuffer = (BGRcolor*)outputImage.accessPixels();

	cout << "Processing...\n";
	steady_clock::time_point start(std::chrono::steady_clock::now());
	
	generator(outputBuffer, size, max_iterations);

	steady_clock::time_point end(std::chrono::steady_clock::now());
	double executionTime = duration_cast<duration<double>>(end - start).count();
	cout << "processing time in seconds = " << executionTime << "\n";

	cout << "Saving image...\n";
	ostringstream name;
	if (filename == 0) {
		name << "images\\JuliaSet C=" << generator.C << " size=" << size << " mIterations=" << max_iterations << generator << ".png";
	}
	else { name << "images\\" << filename << ".png"; }
	cout << "saving in: " << name.str().c_str() << endl;
	outputImage.save(name.str().c_str());

	cout << "...done\n" << endl;
	return executionTime;
}

void test() {
	complex<float>  C(-0.805f, 0.156f);
	//complex<float> C(0.327f, 0.412f);

	int size = 16000;
	int max_iterations = 800;

	SequentialJulia seq(C);
	TBBJulia tbb(C);
	OpenCLJulia opencl(C);
	OpenCLJulia openclNvidia(C, "NVIDIA");

	generateJulia(openclNvidia, size, max_iterations);
	cout << "openclNvidia " << openclNvidia.getLastExecutionTimeInSeconds() << endl;

	generateJulia(opencl, size, max_iterations);
	cout << "OpenCLtime " << opencl.getLastExecutionTimeInSeconds() << endl;
	//generateJulia(tbb, size, max_iterations, "tbb");

	//generateJulia(seq, size, max_iterations, "seq");
}

int main(void) {
	test();
	
	return 0;
}