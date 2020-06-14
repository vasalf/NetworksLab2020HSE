#include <Database.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <iostream>
#include <string_view>

namespace NHttpProxy {

TDatabase::TDatabase() = default;

std::optional<THttpResponse> TDatabase::ServeCached(const std::string& url) {
    TEntry::TTimePoint now = std::chrono::steady_clock::now();

    auto it = SavedResponses_.find(url);
    if (it == SavedResponses_.end()) {
        return {};
    }
    if (it->second.Expire < now) {
        SavedResponses_.erase(it);
        return {};
    }

    return it->second.Response;
}

namespace {

bool StartsWith(const std::string_view& sv, const std::string& prefix) {
    if (sv.size() < prefix.size())
        return false;
    return sv.substr(0, prefix.size()) == prefix;
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T ToIntegral(std::string_view sv) {
    T ret;
    std::from_chars(sv.begin(), sv.end(), ret);
    return ret;
}

int CacheDuration(const THttpResponse& response) {
    auto mbHeader = response.Headers().Find("Cache-Control");
    if (!mbHeader.has_value()) {
        return 0;
    }
    auto header = mbHeader.value();

    auto directives = header.SplitValue();
    int ret = 0;
    for (const auto& directive : directives) {
        if (directive == "private" || directive == "no-store") {
            return 0;
        }
        if (StartsWith(directive, "max-age")) {
            ret = ToIntegral<int>(directive.substr(std::strlen("max-age=")));
        }
    }

    return ret;
}

}

void TDatabase::CacheResponse(const THttpRequest& request, const THttpResponse& response) {
    TEntry::TTimePoint now = std::chrono::steady_clock::now();
    int duration = CacheDuration(response);
    if (duration > 0) {
        auto it = SavedResponses_.find(request.RequestLine().URL());
        if (it == SavedResponses_.end()) {
            SavedResponses_.emplace(request.RequestLine().URL(),
                TEntry {
                    response,
                    now + std::chrono::seconds(duration)
                }
            );
        } else {
            it->second.Response = response;
            it->second.Expire = now + std::chrono::seconds(duration);
        }
    }
}

}
