// SPDX-License-Identifier: AGPL-3.0-only
// The gary4local host setting: which inputs the settings dialog accepts, and the service URLs
// the processor builds from them. An empty host must keep the original localhost URLs.
#include <JuceHeader.h>
#include "../../Source/PluginProcessor.h"
#include <iostream>

namespace {
int failures = 0, checks = 0;
void check(bool ok, const juce::String& message) {
    ++checks;
    if (!ok) { ++failures; std::cout << "FAIL: " << message << '\n'; }
}
void accepts(const juce::String& input, const juce::String& expected) {
    juce::String out = "unset";
    const bool ok = Gary4juceAudioProcessor::normaliseLocalHost(input, out);
    check(ok && out == expected, "'" + input + "' -> '" + expected + "' (got "
        + (ok ? "'" + out + "'" : juce::String("rejected")) + ")");
}
void rejects(const juce::String& input) {
    juce::String out;
    check(!Gary4juceAudioProcessor::normaliseLocalHost(input, out), "'" + input + "' should be rejected");
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI juce;

    // This computer, in all its spellings.
    accepts("", "");
    accepts("   ", "");
    accepts("localhost", "");
    accepts("LOCALHOST", "");
    accepts("127.0.0.1", "");
    accepts("http://127.0.0.1/", "");
    accepts("::1", "");
    accepts("[::1]", "");

    // Another machine.
    accepts("192.168.1.50", "192.168.1.50");
    accepts(" 192.168.1.50 ", "192.168.1.50");
    accepts("http://192.168.1.50", "192.168.1.50");
    accepts("https://192.168.1.50/", "192.168.1.50");
    accepts("Studio-Box.local", "studio-box.local");
    accepts("spark-c7ca", "spark-c7ca");
    accepts("gpu.tail1234.ts.net", "gpu.tail1234.ts.net");
    accepts("fd7a:115c:a1e0::1", "[fd7a:115c:a1e0::1]");
    accepts("[fd7a:115c:a1e0::1]", "[fd7a:115c:a1e0::1]");

    // Ports are fixed per service, and anything URL-shaped beyond a host is refused.
    rejects("192.168.1.50:8000");
    rejects("http://192.168.1.50:8000/");
    rejects("192.168.1.50/gary");
    rejects("studio box");
    rejects("user@studio");
    rejects("[studio]");
    rejects("[fd7a::1");
    rejects("fe80::1%eth0");
    accepts("http://", ""); // nothing left after the scheme: this computer

    using Service = Gary4juceAudioProcessor::ServiceType;
    {
        Gary4juceAudioProcessor processor;

        // Remote mode ignores the local host entirely.
        processor.setLocalHost("192.168.1.50");
        check(processor.getServiceUrl(Service::Gary, "/health") == "https://g4l.thecollabagepatch.com/health",
              "remote gary url");
        check(processor.getServiceUrl(Service::Carey, "/x") == "https://g4l.thecollabagepatch.com/carey/x",
              "remote carey url");

        processor.setUsingLocalhost(true);
        check(processor.getServiceUrl(Service::Gary, "/health") == "http://192.168.1.50:8000/health", "lan gary url");
        check(processor.getServiceUrl(Service::Terry, "") == "http://192.168.1.50:8002", "lan terry url");
        check(processor.getServiceUrl(Service::Carey, "/g") == "http://192.168.1.50:8003/g", "lan carey url");
        check(processor.getServiceUrl(Service::Jerry, "") == "http://192.168.1.50:8005", "lan jerry url");
        check(processor.getServiceUrl(Service::SA3, "") == "http://192.168.1.50:8006", "lan sa3 url");
        check(processor.getServiceUrl(Service::Yuey, "") == "http://192.168.1.50:8007", "lan yuey url");
        check(processor.getServiceUrl(Service::Foundation, "") == "http://192.168.1.50:8015", "lan foundation url");

        processor.setLocalHost("fd7a:115c:a1e0::1");
        check(processor.getServiceUrl(Service::SA3, "/health") == "http://[fd7a:115c:a1e0::1]:8006/health",
              "ipv6 sa3 url");

        // Invalid input falls back to this computer rather than building a broken URL.
        processor.setLocalHost("192.168.1.50:8000");
        check(processor.getLocalHost().isEmpty(), "invalid host clears to this computer");

        // Empty host: exactly the URLs gary4juce used before the setting existed.
        processor.setLocalHost({});
        check(processor.getServiceUrl(Service::Gary, "/health") == "http://localhost:8000/health", "default gary url");
        check(processor.getServiceUrl(Service::Yuey, "") == "http://localhost:8007", "default yuey url");
        check(processor.getServiceUrl(Service::Foundation, "") == "http://localhost:8015", "default foundation url");

        processor.setUsingLocalhost(false);
    }

    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
