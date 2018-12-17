## Parallel & Distributed systems
### Creating a Julia Set in C++
The goal of this project is to test the differences in execution speed between 3 versions. Every version generates a JuliaSet using FreeImage to manage the images. 
Version one is a simple sequential function, nothing special here. 
Version two uses Intel's Threading Building Blocks to divide the pixel generation over multiple threads. Parallel on CPU
The last version uses a OpenCL kernel to perform the brunt of calculations on a GPU. Parallel on GPU


#### Setup
After Cloning, add FreeImage, TBB and OpenCL libraries. Check "properties > VC++ Directories  > Include Directories & Library Directories" to configure directories. 