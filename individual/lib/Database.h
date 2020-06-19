#pragma once

#include <HTTP.h>

#include <chrono>
#include <map>
#include <optional>

namespace NHttpProxy {

class TDatabase {
public:
    TDatabase();

    std::optional<THttpResponse> ServeCached(const std::string& url);

    void CacheResponse(const THttpRequest& request, const THttpResponse& response);

private:
    struct TEntry {
        using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;

        THttpResponse Response;
        TTimePoint Expire;
    };

    std::map<std::string, TEntry> SavedResponses_;
};

}
