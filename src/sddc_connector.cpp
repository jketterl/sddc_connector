#include "sddc_connector.hpp"

int main (int argc, char** argv) {
    SddcConnector* connector = new SddcConnector();
    return connector->main(argc, argv);
}

uint32_t SddcConnector::get_buffer_size() {
    return 0;
}

int SddcConnector::open() {
    return 0;
}

int SddcConnector::read() {
    return 0;
}

int SddcConnector::close() {
    return 0;
}

int SddcConnector::set_center_frequency(double frequency) {
    return 0;
}

int SddcConnector::set_sample_rate(double sample_rate) {
    return 0;
}

int SddcConnector::set_gain(Owrx::GainSpec* gain) {
    return 0;
}

int SddcConnector::set_ppm(int ppm) {
    return 0;
}

