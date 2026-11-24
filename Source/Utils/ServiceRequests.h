// SPDX-License-Identifier: AGPL-3.0-only
// Every request gary4juce makes to a gary4local service opens here, so a pairing token for
// gary4local's network listener (gary `listen on`) reaches exactly the requests meant for it.
// tests/service_requests/check_call_sites.py fails if a URL stream opens anywhere else.
//
// The listener's host and token are machine preferences, the same in every plugin instance, so
// they live here rather than in one processor. Requests run on background threads that must not
// reach back into an editor that may already be gone; this needs nothing from them.
#pragma once

#include <JuceHeader.h>

namespace ServiceRequests
{
    // gary4local's listener: one port for every service, as /<service>/...
    constexpr int kListenerPort = 8020;

    // Pure: extraHeaders, plus Authorization: Bearer <token> when url is the listener on host.
    // Any other URL - the remote backend, the update server, a stems download, or the same host
    // on a service's own port - gets extraHeaders unchanged.
    inline juce::String withListenerToken(const juce::String& url, const juce::String& host,
        const juce::String& token, const juce::String& extraHeaders)
    {
        if (host.isEmpty() || token.isEmpty())
            return extraHeaders;

        const auto listener = "http://" + host + ":" + juce::String(kListenerPort) + "/";
        if (!url.startsWithIgnoreCase(listener))
            return extraHeaders;

        auto headers = extraHeaders.trimEnd();
        if (headers.isNotEmpty())
            headers << "\r\n";
        return headers + "Authorization: Bearer " + token;
    }

    struct Listener
    {
        juce::SpinLock lock;
        juce::String host;
        juce::String token;
    };

    inline Listener& listener()
    {
        static Listener state;
        return state;
    }

    // The processor calls this whenever the gary4local host or token changes.
    inline void setListener(const juce::String& host, const juce::String& token)
    {
        auto& state = listener();
        const juce::SpinLock::ScopedLockType lock(state.lock);
        state.host = host;
        state.token = token;
    }

    inline juce::String headersFor(const juce::String& url, const juce::String& extraHeaders)
    {
        juce::String host, token;
        {
            auto& state = listener();
            const juce::SpinLock::ScopedLockType lock(state.lock);
            host = state.host;
            token = state.token;
        }
        return withListenerToken(url, host, token, extraHeaders);
    }

    // URL::InputStreamOptions::withExtraHeaders replaces rather than adds, so the token is merged
    // into whatever headers the caller set.
    inline std::unique_ptr<juce::InputStream> open(const juce::URL& url,
        const juce::URL::InputStreamOptions& options)
    {
        const auto headers = headersFor(url.toString(false), options.getExtraHeaders());
        return url.createInputStream(options.withExtraHeaders(headers));
    }
}
