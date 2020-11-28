#include "sddc_connector.hpp"
#include <string>
#include <fstream>
#include <iostream>

int main (int argc, char** argv) {
    SddcConnector* connector = new SddcConnector();
    return connector->main(argc, argv);
}

uint32_t SddcConnector::get_buffer_size() {
    return frame_size;
}

int SddcConnector::open() {
    int index = 0;
    if (device_id != nullptr) {
        index = std::stoi(device_id);
    }

    if (imagefile == nullptr) {
        std::ifstream ifile;
        ifile.open("/lib/firmware/SDDC_FX3.img");
        if (not ifile) {
            std::cerr << "ERROR: no usable FX3 firmware\n";
            return 1;
        }
        imagefile = (const char*) "/lib/firmware/SDDC_FX3.img";
    }

    dev = sddc_open(index, imagefile);
    if (dev == 0) {
        std::cerr << "ERROR: could not open device\n";
        return 1;
    }

    if (sddc_set_sample_rate(dev, adc_sample_rate) < 0) {
        std::cerr << "ERROR - sddc_set_sample_rate() failed\n";
        return 2;
    }


    ddc = new SdrCuda::Ddc(get_buffer_size());
    conversion_buffer = SdrCuda::Ddc::alloc_output(get_buffer_size());

    return 0;
}

static void read_callback(uint32_t data_size, uint8_t *data, void *context) {
    ((SddcConnector*) context)->read_callback(data_size, data);
}

void SddcConnector::read_callback(uint32_t data_size, uint8_t* data) {
    if (!run)
          return;
    uint32_t samples = ddc->run((int16_t*) data, conversion_buffer, data_size / 2);
    processSamples(conversion_buffer, samples * 2);
}

int SddcConnector::read() {
    if (sddc_set_async_params(dev, frame_size, 0, ::read_callback, this) < 0) {
        fprintf(stderr, "ERROR - sddc_set_async_params() failed\n");
        return -1;
    }

    if (sddc_start_streaming(dev) < 0) {
        std::cerr << "ERROR - sddc_start_streaming() failed\n";
        return -1;
    }

    std::cerr << "started streaming...\n";

    /* todo: move this into a thread */
    while (run)
        sddc_handle_events(dev);

    std::cerr << "finished. now stop streaming ..\n";
    if (sddc_stop_streaming(dev) < 0) {
        std::cerr << "ERROR - sddc_stop_streaming() failed\n";
        return -1;
    }

    return 0;
}

int SddcConnector::close() {
    SdrCuda::Ddc::free_output(conversion_buffer);
    conversion_buffer = nullptr;
    delete ddc;
    sddc_close(dev);
    dev = nullptr;
    return 0;
}

int SddcConnector::set_center_frequency(double frequency) {
    if (frequency > RX888_HF_SAMPLERATE / 2) {
        adc_sample_rate = RX888_VHF_SAMPLERATE;

        if (sddc_set_rf_mode(dev, VHF_MODE) != 0) {
            std::cerr << "ERROR - sddc_set_rf_mode() failed\n";
            return -1;
        }

        if (sddc_set_tuner_frequency(dev, frequency) != 0) {
            std::cerr << "ERROR - sddc_set_tuner_frequency() failed\n";
            return -1;
        }

        if (sddc_set_sample_rate(dev, adc_sample_rate) != 0) {
            std::cerr << "ERROR - sddc_set_sample_rate() failed\n";
            return -1;
        }

        ddc->set_frequency_offset(TUNER_IF / adc_sample_rate);
    } else {
        adc_sample_rate = RX888_HF_SAMPLERATE;

        if (sddc_set_rf_mode(dev, HF_MODE) != 0) {
            std::cerr << "ERROR - sddc_set_rf_mode() failed\n";
            return -1;
        }

        if (sddc_set_sample_rate(dev, adc_sample_rate) != 0) {
            std::cerr << "ERROR - sddc_set_sample_rate() failed\n";
            return -1;
        }

        if (sddc_set_tuner_attenuation(dev, 0) != 0) {
            std::cerr << "ERROR - sddc_set_tuner_attenuation() failed\n";
            return -1;
        }

        ddc->set_frequency_offset(frequency / adc_sample_rate);
    }
    ddc->set_decimation(adc_sample_rate / get_sample_rate());

    // re-apply settings
    return set_gain(get_gain());
}

int SddcConnector::set_sample_rate(double sample_rate) {
    ddc->set_decimation(adc_sample_rate / sample_rate);
    double frequency = get_center_frequency();
    if (frequency > RX888_HF_SAMPLERATE / 2) {
        frequency = TUNER_IF;
    }
    ddc->set_frequency_offset(frequency / adc_sample_rate);
    return 0;
}

int SddcConnector::set_gain(Owrx::GainSpec* new_gain) {
    Owrx::SimpleGainSpec* simple_gain;
    if ((simple_gain = dynamic_cast<Owrx::SimpleGainSpec*>(new_gain)) != nullptr) {
        if (get_center_frequency() > RX888_HF_SAMPLERATE / 2) {
            return sddc_set_tuner_attenuation(dev, simple_gain->getValue());
        } else {
            return sddc_set_hf_attenuation(dev, -1 * simple_gain->getValue());
        }
    }

    std::cerr << "unsupported gain settings\n";
    return 100;
}

int SddcConnector::set_ppm(int ppm) {
    return sddc_set_frequency_correction(dev, ppm);
}
