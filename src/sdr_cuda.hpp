#include <stdint.h>

namespace SdrCuda {

class Ddc {
    public:
        Ddc(uint32_t buffersize);
        void set_frequency_offset(float freq_offset);
        void set_decimation(uint16_t decimation);
        ~Ddc();
        uint32_t run(int16_t* input, float* output, uint32_t length);
        static float* alloc_output(uint32_t length);
        static void free_output(float* output);
    private:
        float freq_offset = .25;
        int16_t* raw;
        float* input;
        float* output;
        uint32_t buffersize;
        uint32_t raw_offset;
        float* taps = nullptr;
        uint16_t taps_length;
        uint16_t decimation = 2;
        uint16_t* decimation_device;
        uint16_t decimation_offset = 0;
        uint16_t* decimation_offset_device;
        double phase_offset = 0;
        double* phase_offset_device;
        double angle_per_sample;
        double* angle_per_sample_device;

        void reconfigure();
        float* get_fir_decimate_input();
    };

/*
uint32_t ddc(int16_t* input, float* output, ddc_t* filter, uint32_t length);

void ddc_init(ddc_t* filter, uint32_t buffersize, float freq_offset, uint16_t decimation);
void ddc_close(ddc_t* filter);
float* ddc_alloc_output(size_t length);
*/

}