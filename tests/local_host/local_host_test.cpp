// SPDX-License-Identifier: AGPL-3.0-only
// The gary4local host setting: which inputs the settings dialog accepts, and the service URLs
// the processor builds from them. An empty host must keep the original localhost URLs.
#include <JuceHeader.h>
#include "../../Source/PluginProcessor.h"
#include <iostream>
#include <mutex>

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
// A stand-in for gary4local's listener on 127.0.0.1:8020: records each request and answers 200
// only when it carries the expected Authorization header, like the real one.
struct FakeListener : juce::Thread
{
    juce::StreamingSocket server;
    juce::String token;
    std::mutex lock;
    juce::StringArray requests;

    FakeListener() : juce::Thread("fake gary4local listener") {}
    ~FakeListener() override { stop(); }

    bool start(const juce::String& expectedToken)
    {
        token = expectedToken;
        if (!server.createListener(ServiceRequests::kListenerPort, "127.0.0.1"))
            return false;
        startThread();
        return true;
    }

    void stop()
    {
        signalThreadShouldExit();
        server.close();
        stopThread(4000);
    }

    juce::String last()
    {
        std::lock_guard<std::mutex> guard(lock);
        return requests.isEmpty() ? juce::String() : requests[requests.size() - 1];
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> client(server.waitForNextConnection());
            if (client == nullptr || threadShouldExit())
                return;

            juce::MemoryOutputStream received;
            char buffer[4096];
            int bodyNeeded = -1, headerBytes = 0;
            while (client->waitUntilReady(true, 3000) == 1)
            {
                const int n = client->read(buffer, sizeof(buffer), false);
                if (n <= 0)
                    break;
                received.write(buffer, (size_t) n);
                const auto text = received.toString();
                if (bodyNeeded < 0 && text.contains("\r\n\r\n"))
                {
                    headerBytes = (int) text.upToFirstOccurrenceOf("\r\n\r\n", true, false).getNumBytesAsUTF8();
                    bodyNeeded = 0;
                    juce::StringArray lines;
                    lines.addLines(text.upToFirstOccurrenceOf("\r\n\r\n", false, false));
                    for (const auto& line : lines)
                        if (line.startsWithIgnoreCase("Content-Length:"))
                            bodyNeeded = line.fromFirstOccurrenceOf(":", false, false).trim().getIntValue();
                }
                if (bodyNeeded >= 0 && (int) received.getDataSize() >= headerBytes + bodyNeeded)
                    break;
            }

            const auto request = received.toString();
            {
                std::lock_guard<std::mutex> guard(lock);
                requests.add(request);
            }
            const bool authorized = request.containsIgnoreCase("\r\nAuthorization: Bearer " + token + "\r\n");
            const juce::String body = authorized ? "{\"status\":\"ok\"}" : "{\"error\":\"pairing_token_required\"}";
            const juce::String response = juce::String(authorized ? "HTTP/1.1 200 OK" : "HTTP/1.1 401 Unauthorized")
                + "\r\nContent-Type: application/json\r\nContent-Length: " + juce::String(body.getNumBytesAsUTF8())
                + "\r\nConnection: close\r\n\r\n" + body;
            client->write(response.toRawUTF8(), (int) response.getNumBytesAsUTF8());
            client->close();
        }
    }
};

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

    // The pairing token for gary4local's listener: case, spaces and dashes don't matter, and empty
    // means no token. Anything else is refused rather than sent.
    const juce::String token = "gary-7k2mq-x9c4r-hw3ve-pt8an";
    auto tokenAccepts = [&](const juce::String& input, const juce::String& expected) {
        juce::String out = "unset";
        const bool ok = Gary4juceAudioProcessor::normaliseLocalToken(input, out);
        check(ok && out == expected, "token '" + input + "' -> '" + expected + "' (got "
            + (ok ? "'" + out + "'" : juce::String("rejected")) + ")");
    };
    auto tokenRejects = [&](const juce::String& input) {
        juce::String out;
        check(!Gary4juceAudioProcessor::normaliseLocalToken(input, out), "token '" + input + "' should be rejected");
    };
    tokenAccepts("", "");
    tokenAccepts("   ", "");
    tokenAccepts(token, token);
    tokenAccepts("GARY-7K2MQ-X9C4R-HW3VE-PT8AN", token);
    tokenAccepts("gary7k2mqx9c4rhw3vept8an", token);
    tokenAccepts(" gary - 7k2mq x9c4r-hw3ve pt8an ", token);
    tokenRejects("7k2mq-x9c4r-hw3ve-pt8an");          // no prefix
    tokenRejects("gary-7k2mq-x9c4r-hw3ve-pt8a");      // a symbol short
    tokenRejects("gary-7k2mq-x9c4r-hw3ve-pt8ann");    // a symbol long
    tokenRejects("gary-7k2mq-x9c4r-hw3ve-pt8ai");     // i is not Crockford base32
    tokenRejects("gary-7k2mq-x9c4r-hw3ve-pt8al");
    tokenRejects("gary-7k2mq-x9c4r-hw3ve-pt8ao");
    tokenRejects("gary-7k2mq-x9c4r-hw3ve-pt8au");
    tokenRejects("Bearer gary-7k2mq-x9c4r-hw3ve-pt8an");

    // Every service and endpoint, every host: without a token the URLs are exactly v5.0.7's, and a
    // token only changes them for another computer in local mode.
    {
        const Service services[] = { Service::Gary, Service::Terry, Service::Jerry, Service::Carey,
                                     Service::Foundation, Service::SA3, Service::Yuey };
        const juce::String hosts[] = { "", "192.168.1.50", "spark-c7ca", "[fd7a:115c:a1e0::1]" };
        const juce::String endpoints[] = { "", "/health", "/generate", "/poll_status/abc?x=1" };
        auto v507 = [](bool local, const juce::String& host, Service service, const juce::String& endpoint) {
            if (local)
                return "http://" + (host.isEmpty() ? juce::String("localhost") : host) + ":"
                    + juce::String(Gary4juceAudioProcessor::getLocalServicePort(service)) + endpoint;
            switch (service) {
                case Service::Carey:      return "https://g4l.thecollabagepatch.com/carey" + endpoint;
                case Service::Foundation: return "https://g4l.thecollabagepatch.com/foundation" + endpoint;
                case Service::SA3:        return "https://g4l.thecollabagepatch.com/sa3" + endpoint;
                case Service::Yuey:       return "https://g4l.thecollabagepatch.com/yuey" + endpoint;
                default:                  return "https://g4l.thecollabagepatch.com" + endpoint;
            }
        };
        int combinations = 0;
        for (const bool local : { false, true })
            for (const auto& host : hosts)
                for (const auto service : services)
                    for (const auto& endpoint : endpoints)
                    {
                        ++combinations;
                        const auto before = v507(local, host, service, endpoint);
                        check(Gary4juceAudioProcessor::buildServiceUrl(local, host, {}, service, endpoint) == before,
                              "no token keeps " + before);
                        const auto withToken = Gary4juceAudioProcessor::buildServiceUrl(local, host, token, service, endpoint);
                        const auto expected = local && host.isNotEmpty()
                            ? "http://" + host + ":8020/" + Gary4juceAudioProcessor::getListenerRoute(service) + endpoint
                            : before;
                        check(withToken == expected, "with a token " + before + " -> " + expected + " (got " + withToken + ")");
                    }
        check(combinations == 2 * 4 * 7 * 4, "every url combination checked");
    }
    check(Gary4juceAudioProcessor::getListenerRoute(Service::Gary) == "gary"
          && Gary4juceAudioProcessor::getListenerRoute(Service::Terry) == "terry"
          && Gary4juceAudioProcessor::getListenerRoute(Service::Jerry) == "jerry"
          && Gary4juceAudioProcessor::getListenerRoute(Service::Carey) == "carey"
          && Gary4juceAudioProcessor::getListenerRoute(Service::Foundation) == "foundation"
          && Gary4juceAudioProcessor::getListenerRoute(Service::SA3) == "sa3"
          && Gary4juceAudioProcessor::getListenerRoute(Service::Yuey) == "yuey",
          "listener routes are gary4local's tab names");

    // The token goes to the listener on the configured host and nowhere else.
    {
        const juce::String host = "192.168.1.50";
        const juce::String auth = "Authorization: Bearer " + token;
        auto headers = [&](const juce::String& url, const juce::String& extra) {
            return ServiceRequests::withListenerToken(url, host, token, extra);
        };
        check(headers("http://192.168.1.50:8020/sa3/health", "") == auth, "listener, no other headers");
        check(headers("http://192.168.1.50:8020/sa3/health", "Accept: application/json")
              == "Accept: application/json\r\n" + auth, "listener keeps Accept");
        check(headers("http://192.168.1.50:8020/yuey/generate", "Content-Type: application/json\r\nAccept: application/json")
              == "Content-Type: application/json\r\nAccept: application/json\r\n" + auth, "listener keeps both headers");
        check(headers("http://192.168.1.50:8020/gary/x", "Accept: application/json\r\n")
              == "Accept: application/json\r\n" + auth, "a trailing line break is not doubled");
        check(headers("HTTP://192.168.1.50:8020/gary/x", "") == auth, "scheme case does not matter");
        for (const auto& elsewhere : { juce::String("https://g4l.thecollabagepatch.com/sa3/health"),
                                       juce::String("http://192.168.1.50:8006/health"),
                                       juce::String("http://192.168.1.51:8020/sa3/health"),
                                       juce::String("http://192.168.1.500:8020/sa3/health"),
                                       juce::String("http://localhost:8020/sa3/health"),
                                       juce::String("https://github.com/betweentwomidnights/gary4juce/releases"),
                                       juce::String("http://192.168.1.50:80200/sa3/health") })
            check(headers(elsewhere, "Accept: application/json") == "Accept: application/json",
                  "no token for " + elsewhere);
        check(ServiceRequests::withListenerToken("http://192.168.1.50:8020/sa3/health", host, {}, "Accept: x")
              == "Accept: x", "no token set, nothing added");
        check(ServiceRequests::withListenerToken("http://:8020/sa3/health", {}, token, "Accept: x")
              == "Accept: x", "no host, nothing added");
    }

    // The processor publishes its host and token to ServiceRequests, and only another computer
    // in local mode uses the listener.
    {
        Gary4juceAudioProcessor processor;
        processor.setLocalToken(token);
        check(processor.getLocalToken() == token, "token kept");
        check(!processor.isUsingListener(), "remote mode never uses the listener");
        check(processor.getServiceUrl(Service::SA3, "/health") == "https://g4l.thecollabagepatch.com/sa3/health",
              "remote mode ignores the token");

        processor.setUsingLocalhost(true);
        check(!processor.isUsingListener(), "this computer never uses the listener");
        check(processor.getServiceUrl(Service::SA3, "/health") == "http://localhost:8006/health",
              "this computer keeps its own ports with a token set");

        processor.setLocalHost("192.168.1.50");
        check(processor.isUsingListener(), "another computer with a token uses the listener");
        check(processor.getServiceUrl(Service::SA3, "/health") == "http://192.168.1.50:8020/sa3/health",
              "listener sa3 url");
        check(processor.getServiceUrl(Service::Gary, "") == "http://192.168.1.50:8020/gary", "listener base url");
        check(ServiceRequests::headersFor("http://192.168.1.50:8020/sa3/health", "Accept: application/json")
              == "Accept: application/json\r\nAuthorization: Bearer " + token, "processor published the token");
        check(ServiceRequests::headersFor("http://192.168.1.50:8006/health", "Accept: application/json")
              == "Accept: application/json", "a service's own port gets no token");

        processor.setLocalToken("not a token");
        check(processor.getLocalToken().isEmpty() && !processor.isUsingListener(), "an invalid token clears");
        check(processor.getServiceUrl(Service::SA3, "/health") == "http://192.168.1.50:8006/health",
              "without a token, another computer is back on its own ports");
        check(ServiceRequests::headersFor("http://192.168.1.50:8020/sa3/health", "Accept: application/json")
              == "Accept: application/json", "a cleared token is not sent");

        processor.setLocalHost({});
        processor.setUsingLocalhost(false);
    }

    // The real network stack sends the header, through every way gary4juce opens a request.
    {
        FakeListener listener;
        if (!listener.start(token))
        {
            std::cout << "SKIP: port " << ServiceRequests::kListenerPort
                      << " is busy here; the request header checks did not run\n";
        }
        else
        {
            ServiceRequests::setListener("127.0.0.1", token);
            const auto auth = "Authorization: Bearer " + token + "\r\n";

            int status = 0;
            auto get = ServiceRequests::open(juce::URL("http://127.0.0.1:8020/sa3/health"),
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(5000).withStatusCode(&status)
                    .withExtraHeaders("Accept: application/json"));
            const auto getBody = get != nullptr ? get->readEntireStreamAsString() : juce::String();
            auto request = listener.last();
            check(status == 200 && getBody.contains("ok"), "GET through the listener (status "
                  + juce::String(status) + ")");
            check(request.startsWith("GET /sa3/health "), "GET path: " + request.upToFirstOccurrenceOf("\r\n", false, false));
            check(request.containsIgnoreCase(auth), "GET carries the token");
            check(request.containsIgnoreCase("Accept: application/json\r\n"), "GET keeps Accept");

            status = 0;
            const juce::String json = "{\"prompt\":\"drums\"}";
            auto post = ServiceRequests::open(juce::URL("http://127.0.0.1:8020/yuey/generate").withPOSTData(json),
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withConnectionTimeoutMs(5000).withStatusCode(&status)
                    .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json"));
            if (post != nullptr)
                post->readEntireStreamAsString();
            request = listener.last();
            check(status == 200, "POST through the listener (status " + juce::String(status) + ")");
            check(request.startsWith("POST /yuey/generate "), "POST path");
            check(request.containsIgnoreCase(auth), "POST carries the token");
            check(request.containsIgnoreCase("Content-Type: application/json\r\n"), "POST keeps Content-Type");
            check(request.endsWith(json), "POST body intact");

            status = 0;
            juce::WebInputStream web(juce::URL("http://127.0.0.1:8020/gary/poll_status/s1"), false);
            web.withExtraHeaders(ServiceRequests::headersFor("http://127.0.0.1:8020/gary/poll_status/s1",
                    "Accept: application/json\r\nContent-Type: application/json"))
               .withConnectionTimeout(5000);
            const bool connected = web.connect(nullptr);
            check(connected && web.getStatusCode() == 200, "polling stream through the listener (status "
                  + juce::String(web.getStatusCode()) + ")");
            check(listener.last().containsIgnoreCase(auth), "polling stream carries the token");

            // A listener on another host gets nothing, so the fake refuses it.
            ServiceRequests::setListener("192.0.2.1", token);
            status = 0;
            auto elsewhere = ServiceRequests::open(juce::URL("http://127.0.0.1:8020/sa3/health"),
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(5000).withStatusCode(&status)
                    .withExtraHeaders("Accept: application/json"));
            check(status == 401, "another host's token is not sent (status " + juce::String(status) + ")");
            check(!listener.last().containsIgnoreCase("Authorization:"), "no Authorization header at all");

            ServiceRequests::setListener({}, {});
            listener.stop();
        }
    }

    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
