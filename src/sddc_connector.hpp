#pragma once

#include "sdr_cuda.hpp"
#include <owrx/connector.hpp>
#include <owrx/gainspec.hpp>
extern "C" {
#define this extern_this
#include <libsddc.h>
#undef this
}

#define RX888_HF_SAMPLERATE 128e6
#define RX888_VHF_SAMPLERATE 20e6
#define TUNER_IF -5e6

class SddcConnector: public Owrx::Connector {
    public:
        void read_callback(uint32_t data_size, uint8_t* data);
    protected:
        virtual uint32_t get_buffer_size() override;
        virtual int open() override;
        virtual int read() override;
        virtual int close() override;
        virtual int set_center_frequency(double frequency) override;
        virtual int set_sample_rate(double sample_rate) override;
        virtual int set_gain(Owrx::GainSpec* gain) override;
        virtual int set_ppm(double ppm) override;
    private:
        sddc_t* dev;

        // must be divisible by 512 for CUDA
        uint32_t frame_size = 512*256;
        float* conversion_buffer;
        const char* imagefile = nullptr;;
        SdrCuda::Ddc* ddc;

        double adc_sample_rate = RX888_HF_SAMPLERATE;
};