#include "SubOnDsp.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        if (!subon::dsp::isValidConfiguration(48000.0, 30.0, 120.0)) {
            std::cerr << "Expected valid filter configuration\n";
            return EXIT_FAILURE;
        }
        if (subon::dsp::isValidConfiguration(48000.0, 120.0, 30.0)) {
            std::cerr << "Expected invalid reversed cutoff configuration\n";
            return EXIT_FAILURE;
        }

        subon::dsp::BassBandFilter filter(48000.0, 30.0, 120.0);
        double output = 0.0;
        for (int i = 0; i < 48000; ++i) {
            const double input = (i == 0) ? 1.0 : 0.0;
            output = filter.process(input);
            if (!std::isfinite(output)) {
                std::cerr << "Filter produced a non-finite output\n";
                return EXIT_FAILURE;
            }
        }

        filter.reset();
        const double resetOutput = filter.process(0.0);
        if (!std::isfinite(resetOutput)) {
            std::cerr << "Filter reset produced a non-finite output\n";
            return EXIT_FAILURE;
        }

        std::cout << "SubON DSP smoke test passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "SubON DSP smoke test failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
