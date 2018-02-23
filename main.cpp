#include "cuda.h"
#include "gomoku.h"

int main() {
    cuInit(0);
    CUdevice cuDevice;
    cuDeviceGet(&cuDevice, 0);
    CUcontext cuContext;
    cuCtxCreate(&cuContext, 0, cuDevice);
    gomoku();
}


