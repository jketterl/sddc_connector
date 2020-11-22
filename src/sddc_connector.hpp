#pragma once

#include <owrx/connector.hpp>
#include <owrx/gainspec.hpp>

class SddcConnector: public Owrx::Connector {
    protected:
        virtual uint32_t get_buffer_size() override;
        virtual int open() override;
        virtual int read() override;
        virtual int close() override;
        virtual int set_center_frequency(double frequency) override;
        virtual int set_sample_rate(double sample_rate) override;
        virtual int set_gain(Owrx::GainSpec* gain) override;
        virtual int set_ppm(int ppm) override;
};