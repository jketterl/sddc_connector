#include "sdr_cuda.hpp"
#include <cuda_fp16.h>
#include <iostream>

using namespace SdrCuda;

__global__ void convert_ui16_c_kernel(int16_t* input, float* output, double* phase_offset, double* angle_per_sample) {
    uint32_t i = blockDim.x * blockIdx.x + threadIdx.x;
    float converted = (float)input[i] / INT16_MAX;
    double angle = *phase_offset + *angle_per_sample * i;
    double sinValue;
    double cosValue;
    sincospi(angle, &sinValue, &cosValue);
    output[i * 2] = sinValue * converted;
    output[i * 2 + 1] = cosValue * converted;
}

__global__ void fir_decimate_c_kernel(float* input, float* output, uint16_t* decimation, uint16_t* decimation_offset, float* taps, uint16_t taps_length) {
    uint32_t i = blockDim.x * blockIdx.x + threadIdx.x;
    uint32_t dec_i = *decimation_offset + i * *decimation;
    float acci = 0;
    float accq = 0;
    for (uint16_t k = 0; k < taps_length; k++) {
        int32_t index = dec_i - (taps_length - k);
        acci += input[index * 2] * taps[k];
        accq += input[index * 2 + 1] * taps[k];
    }
    output[i * 2] = acci;
    output[i * 2 + 1] = accq;
}

__global__ void fir_decimate_copy_delay(float* input, float* output) {
    uint32_t i = blockDim.x * blockIdx.x + threadIdx.x;
    output[i * 2] = input[i * 2];
    output[i * 2 + 1] = input[i * 2 + 1];
}

Ddc::Ddc(uint32_t new_buffersize) {
    buffersize = new_buffersize;

    cudaMalloc((void**)&decimation_device, sizeof(uint16_t));
    cudaMemcpy(decimation_device, &decimation, sizeof(uint16_t), cudaMemcpyHostToDevice);

    cudaMalloc((void**)&decimation_offset_device, sizeof(uint16_t));
    cudaMemcpy(decimation_offset_device, &decimation_offset, sizeof(uint16_t), cudaMemcpyHostToDevice);

    cudaMalloc((void**)&phase_offset_device, sizeof(double));
    cudaMemcpy(phase_offset_device, &phase_offset, sizeof(double), cudaMemcpyHostToDevice);

    cudaMalloc((void**)&angle_per_sample_device, sizeof(double));

    cudaMalloc((void**)&raw, sizeof(int16_t) * buffersize);
    cudaMalloc((void**)&input, sizeof(float) * 2 * (buffersize + taps_length));
    cudaMalloc((void**)&output, sizeof(float) * buffersize);

    reconfigure();
}

void Ddc::set_frequency_offset(float new_frequency_offset) {
    freq_offset = new_frequency_offset;
    reconfigure();
}

void Ddc::set_decimation(uint16_t new_decimation) {
    decimation = new_decimation;
    cudaMemcpy(decimation_device, &decimation, sizeof(uint16_t), cudaMemcpyHostToDevice);

    decimation_offset = 0;
    cudaMemcpy(decimation_offset_device, &decimation_offset, sizeof(uint16_t), cudaMemcpyHostToDevice);
    reconfigure();
}

void Ddc::reconfigure() {
    taps_length = 4 * decimation;
    if (taps_length %2 == 0) taps_length++;
    // maximum number of taps currently limited by maximum number of cuda threads
    if (taps_length > 511) taps_length = 511;
    taps_length = max(taps_length, 121);
    std::cerr << "taps length: " << taps_length << "\n";

    float* new_taps = (float*) malloc(sizeof(float) * taps_length);
    firdes_lowpass_f(new_taps, taps_length, 0.485/decimation, WINDOW_HAMMING);

    if (taps != nullptr) cudaFree(taps);
    cudaMalloc((void**)&taps, sizeof(float) * taps_length);
    cudaMemcpy(taps, new_taps, sizeof(float) * taps_length, cudaMemcpyHostToDevice);

    free(new_taps);

    angle_per_sample = 2 * freq_offset;
    cudaMemcpy(angle_per_sample_device, &angle_per_sample, sizeof(double), cudaMemcpyHostToDevice);
}

Ddc::~Ddc() {
    cudaFree(decimation_device);
    cudaFree(decimation_offset_device);
    cudaFree(phase_offset_device);
    cudaFree(angle_per_sample_device);
    cudaFree(raw);
    cudaFree(input);
    cudaFree(output);
    cudaFree(taps);
}

float* Ddc::alloc_output(uint32_t length) {
    float* output;
    cudaHostAlloc((void**)&output, sizeof(float) * 2 * length, cudaHostAllocDefault);
    return output;
}

void Ddc::free_output(float* output) {
    cudaFree(output);
}

float* Ddc::get_fir_decimate_input() {
    return input + (taps_length * 2);
}

uint32_t Ddc::run(int16_t* input_samples, float* host_output, uint32_t length) {
    cudaMemcpy(raw, input_samples, sizeof(int16_t) * length, cudaMemcpyHostToDevice);

    int blocks = length / 1024;
    // run an extra block if memory does not line up ideally
    if (blocks % 1024 > 0) blocks += 1;
    convert_ui16_c_kernel<<<blocks, 1024>>>(raw, get_fir_decimate_input(), phase_offset_device, angle_per_sample_device);

    // move the phase forward
    phase_offset += angle_per_sample * length;
    while (phase_offset >= 2) phase_offset -= 2;
    cudaMemcpy(phase_offset_device, &phase_offset, sizeof(double), cudaMemcpyHostToDevice);

    uint32_t out_samples = (decimation_offset + length) / decimation;
    blocks = out_samples / 512;
    // run an extra block if memory does not line up ideally
    if (out_samples % 512 > 0) blocks += 1;
    fir_decimate_c_kernel<<<blocks, 512>>>(get_fir_decimate_input(), output, decimation_device, decimation_offset_device, taps, taps_length);

    // update decimation offset
    decimation_offset = (decimation_offset + length) % decimation;
    cudaMemcpy(decimation_offset_device, &decimation_offset, sizeof(uint16_t), cudaMemcpyHostToDevice);

    // copy unprocessed samples from the end to the beginning of the input buffer
    fir_decimate_copy_delay<<<1, taps_length>>>(input + (length * 2), input);
    cudaMemcpy(host_output, output, sizeof(float) * (out_samples * 2), cudaMemcpyDeviceToHost);

    cudaDeviceSynchronize();
    cudaError_t error = cudaGetLastError();
    if (error != cudaSuccess) {
        std::cerr << "CUDA ERROR: " << cudaGetErrorString(error) << "\n";
        exit(-1);
    }

    return out_samples;
}
