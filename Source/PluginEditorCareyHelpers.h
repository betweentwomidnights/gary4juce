// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

// Carey's backend contract: acestep.cpp's ace-server job API, which gary's
// remote backend implements too (docs/CAREY_BACKEND_CONTRACT.md).
//
//   POST /synth                 multipart: "request" (one AceRequest as JSON),
//                               optional "audio" and "ref_audio" (WAV) -> {"id"}
//   GET  /job?id=ID             {"status":"running","progress":{...}}, "done",
//                               "failed" (with "error") or "cancelled"
//   GET  /job?id=ID&result=1    multipart/mixed: an audio/* part per track
//   POST /job?id=ID&cancel=1    stop it
//   GET  /props                 models.dit, adapters and adapter_info
//
// Everything here blocks: call it from a worker thread.
namespace carey_native
{
    struct Request
    {
        juce::var json;                    // the AceRequest fields, as an object
        juce::MemoryBlock sourceAudio;     // WAV sent as "audio"; empty for text2music
        juce::MemoryBlock referenceAudio;  // WAV sent as "ref_audio"; optional
    };

    struct Progress
    {
        juce::String stage;  // queued, load, encode, dit or decode
        int step = 0;
        int total = 0;
    };

    struct Result
    {
        bool succeeded = false;
        bool cancelled = false;
        juce::MemoryBlock audio;  // the generated WAV
        juce::String error;       // short, for the status bar
    };

    struct Adapter
    {
        juce::String name;
        juce::String family;  // "standard" (2B) or "xl"; empty when the server does not say
        juce::StringArray captions;
    };

    struct Props
    {
        juce::StringArray dits;
        std::vector<Adapter> adapters;
    };

    // A seed in the range ace-server's Philox noise uses. The plugin always
    // sends one, since the server does not report a seed it chose itself.
    inline juce::int64 newSeed()
    {
        return juce::Random::getSystemRandom().nextInt64() & 0xffffffffLL;
    }

    // One bar for the whole job: loading, then encoding, sampling and decoding.
    inline int progressPercent(const Progress& progress)
    {
        const double fraction = progress.total > 0
            ? juce::jlimit(0.0, 1.0, (double) progress.step / (double) progress.total)
            : 0.0;
        if (progress.stage == "encode") return 2 + juce::roundToInt(8.0 * fraction);
        if (progress.stage == "dit")    return 10 + juce::roundToInt(85.0 * fraction);
        if (progress.stage == "decode") return 95 + juce::roundToInt(4.0 * fraction);
        if (progress.stage == "load")   return 1;
        return 0;
    }

    inline juce::String progressText(const Progress& progress)
    {
        if (progress.stage == "load")   return "loading models...";
        if (progress.stage == "encode") return "encoding source audio...";
        if (progress.stage == "dit")    return juce::String(progress.step) + "/" + juce::String(progress.total) + " steps";
        if (progress.stage == "decode") return "decoding audio...";
        return "queued";
    }

    // The DiT the server lists for `variant` (base, sft or turbo) in the 2B or
    // XL family, e.g. acestep-v15-xl-base-Q4_K_M.gguf. With several
    // precisions, the highest wins. Empty when the server has none.
    inline juce::String resolveDit(const juce::StringArray& dits, const juce::String& variant, bool xl)
    {
        const juce::String prefix = "acestep-v15-" + juce::String(xl ? "xl-" : "") + variant;
        static const juce::StringArray kPrecisionOrder { "BF16", "Q8_0", "Q6_K", "Q5_K_M", "Q4_K_M" };
        juce::String best;
        int bestRank = 1000;
        for (const auto& dit : dits)
        {
            const juce::String stem = dit.endsWithIgnoreCase(".gguf") ? dit.dropLastCharacters(5) : dit;
            if (stem != prefix && !stem.startsWith(prefix + "-"))
                continue;
            const juce::String precision = stem.substring(prefix.length()).trimCharactersAtStart("-");
            int rank = kPrecisionOrder.indexOf(precision);
            if (rank < 0)
                rank = precision.isEmpty() ? 0 : 999;
            if (rank < bestRank)
            {
                best = dit;
                bestRank = rank;
            }
        }
        return best;
    }

    namespace detail
    {
        struct Response
        {
            bool reached = false;
            int status = 0;
            juce::MemoryBlock body;
            juce::String contentType;

            juce::String text() const { return body.toString(); }
        };

        inline Response send(const juce::String& url, const juce::String& method,
                             const juce::MemoryBlock* body, const juce::String& contentType,
                             int timeoutMs)
        {
            Response response;
            juce::URL target(url);
            if (body != nullptr)
                target = target.withPOSTData(*body);
            else if (method == "POST")
                target = target.withPOSTData(juce::MemoryBlock());

            juce::StringPairArray headers;
            juce::String extraHeaders = "Accept: application/json, multipart/mixed";
            if (contentType.isNotEmpty())
                extraHeaders = "Content-Type: " + contentType + "\r\n" + extraHeaders;

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd(method)
                .withConnectionTimeoutMs(timeoutMs)
                .withStatusCode(&response.status)
                .withResponseHeaders(&headers)
                .withExtraHeaders(extraHeaders);

            auto stream = target.createInputStream(options);
            if (stream == nullptr)
                return response;

            response.reached = true;
            stream->readIntoMemoryBlock(response.body);
            for (const auto& key : headers.getAllKeys())
                if (key.equalsIgnoreCase("Content-Type"))
                    response.contentType = headers[key];
            return response;
        }

        // `fallback`, or the server's own reason from {"error": "..."}.
        inline juce::String errorFrom(const Response& response, const juce::String& fallback)
        {
            const juce::var reply = juce::JSON::parse(response.text());
            const juce::String reason = reply.getProperty("error", {}).toString().trim();
            return reason.isNotEmpty() ? reason : fallback;
        }

        inline void appendText(juce::MemoryOutputStream& out, const juce::String& text)
        {
            out.write(text.toRawUTF8(), text.getNumBytesAsUTF8());
        }

        inline juce::MemoryBlock buildForm(const Request& request, const juce::String& boundary)
        {
            juce::MemoryOutputStream out;
            appendText(out, "--" + boundary + "\r\nContent-Disposition: form-data; name=\"request\"\r\n"
                            "Content-Type: application/json\r\n\r\n");
            appendText(out, juce::JSON::toString(request.json, true));
            appendText(out, "\r\n");
            const auto file = [&](const char* name, const juce::MemoryBlock& wav)
            {
                if (wav.getSize() == 0)
                    return;
                appendText(out, "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + juce::String(name)
                                + "\"; filename=\"" + juce::String(name) + ".wav\"\r\nContent-Type: audio/wav\r\n\r\n");
                out.write(wav.getData(), wav.getSize());
                appendText(out, "\r\n");
            };
            file("audio", request.sourceAudio);
            file("ref_audio", request.referenceAudio);
            appendText(out, "--" + boundary + "--\r\n");
            return out.getMemoryBlock();
        }

        // The first audio part of a multipart/mixed result.
        inline juce::MemoryBlock firstAudioPart(const juce::MemoryBlock& body, const juce::String& contentType)
        {
            const juce::String boundary = contentType.fromFirstOccurrenceOf("boundary=", false, true)
                                              .upToFirstOccurrenceOf(";", false, false)
                                              .trim()
                                              .unquoted();
            if (boundary.isEmpty())
                return {};

            const std::string delimiter = "--" + boundary.toStdString();
            static const std::string blankLine = "\r\n\r\n";
            const char* data = static_cast<const char*>(body.getData());
            const char* end = data + body.getSize();
            const char* cursor = std::search(data, end, delimiter.begin(), delimiter.end());
            while (cursor != end)
            {
                const char* part = cursor + delimiter.size();
                if (end - part >= 2 && part[0] == '-' && part[1] == '-')
                    break;
                const char* next = std::search(part, end, delimiter.begin(), delimiter.end());
                const char* headersEnd = std::search(part, next, blankLine.begin(), blankLine.end());
                if (headersEnd != next)
                {
                    const juce::String headers = juce::String::fromUTF8(part, (int) (headersEnd - part));
                    if (headers.containsIgnoreCase("Content-Type: audio/"))
                    {
                        const char* payload = headersEnd + blankLine.size();
                        const char* payloadEnd = next;
                        if (payloadEnd - payload >= 2 && payloadEnd[-2] == '\r' && payloadEnd[-1] == '\n')
                            payloadEnd -= 2;
                        return juce::MemoryBlock(payload, (size_t) (payloadEnd - payload));
                    }
                }
                cursor = next;
            }
            return {};
        }
    }

    // Submit `request`, report its progress until it finishes, and fetch the
    // audio. `shouldStop` is polled between status checks; when it says so,
    // the job is cancelled on the server too.
    inline Result run(const juce::String& baseUrl, const Request& request,
                      const std::function<void(const Progress&)>& onProgress,
                      const std::function<bool()>& shouldStop)
    {
        using namespace detail;
        Result result;

        const juce::String boundary = "gary4juce-" + juce::String::toHexString(newSeed());
        const juce::MemoryBlock form = buildForm(request, boundary);
        const Response submitted = send(baseUrl + "/synth", "POST", &form,
                                        "multipart/form-data; boundary=" + boundary, 120000);
        if (!submitted.reached)
        {
            result.error = "carey not reachable - check connection first";
            return result;
        }
        if (submitted.status >= 400)
        {
            result.error = errorFrom(submitted, "carey refused the request");
            return result;
        }

        const juce::String id = juce::JSON::parse(submitted.text()).getProperty("id", {}).toString().trim();
        const bool validId = id.isNotEmpty() && id.length() <= 64
            && id.containsOnly("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (!validId)
        {
            result.error = "carey returned no job";
            return result;
        }

        const juce::String jobUrl = baseUrl + "/job?id=" + id;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30.0 * 60.0 * 1000.0;
        for (;;)
        {
            if (shouldStop())
            {
                send(jobUrl + "&cancel=1", "POST", nullptr, {}, 5000);
                result.cancelled = true;
                return result;
            }
            if (juce::Time::getMillisecondCounterHiRes() > deadline)
            {
                send(jobUrl + "&cancel=1", "POST", nullptr, {}, 5000);
                result.error = "carey generation timed out";
                return result;
            }

            const Response polled = send(jobUrl, "GET", nullptr, {}, 15000);
            if (!polled.reached || polled.status >= 400)
            {
                result.error = polled.reached ? errorFrom(polled, "carey lost the job") : "carey status polling failed";
                return result;
            }
            const juce::var status = juce::JSON::parse(polled.text());
            const juce::String state = status.getProperty("status", {}).toString();
            if (state == "done")
                break;
            if (state == "failed")
            {
                const juce::String reason = status.getProperty("error", {}).toString().trim();
                result.error = reason.isNotEmpty() ? reason : "carey generation failed";
                return result;
            }
            if (state == "cancelled")
            {
                result.cancelled = true;
                return result;
            }

            const juce::var progress = status.getProperty("progress", {});
            Progress reported;
            reported.stage = progress.getProperty("stage", "queued").toString();
            reported.step = (int) progress.getProperty("step", 0);
            reported.total = (int) progress.getProperty("total", 0);
            onProgress(reported);
            juce::Thread::sleep(500);
        }

        const Response fetched = send(jobUrl + "&result=1", "GET", nullptr, {}, 120000);
        if (!fetched.reached || fetched.status >= 400)
        {
            result.error = "carey finished but its audio could not be downloaded";
            return result;
        }
        result.audio = firstAudioPart(fetched.body, fetched.contentType);
        if (result.audio.getSize() == 0)
        {
            result.error = "carey finished without audio";
            return result;
        }
        result.succeeded = true;
        return result;
    }

    // The DiTs and LoRAs the server offers.
    inline bool fetchProps(const juce::String& baseUrl, Props& props, juce::String& error)
    {
        const detail::Response response = detail::send(baseUrl + "/props", "GET", nullptr, {}, 15000);
        if (!response.reached || response.status >= 400)
        {
            error = "carey not reachable - check connection first";
            return false;
        }
        const juce::var root = juce::JSON::parse(response.text());
        props = {};
        if (const auto* dits = root.getProperty("models", {}).getProperty("dit", {}).getArray())
            for (const auto& dit : *dits)
                props.dits.add(dit.toString());

        const juce::var info = root.getProperty("adapter_info", {});
        if (const auto* names = root.getProperty("adapters", {}).getArray())
        {
            for (const auto& name : *names)
            {
                Adapter adapter;
                adapter.name = name.toString();
                const juce::var details = info.getProperty(adapter.name, {});
                adapter.family = details.getProperty("model_family", {}).toString().trim().toLowerCase();
                if (const auto* captions = details.getProperty("captions", {}).getArray())
                    for (const auto& caption : *captions)
                        if (caption.toString().trim().isNotEmpty())
                            adapter.captions.add(caption.toString().trim());
                props.adapters.push_back(adapter);
            }
        }
        return true;
    }
}
