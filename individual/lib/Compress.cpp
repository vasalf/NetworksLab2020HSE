#include <Compress.h>

#include <sstream>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

namespace NHttpProxy {
namespace {

// XXX: Only gzip detection =(
bool IsCompressed(const THttpResponse& response) {
    auto mbHeader = response.Headers().Find("Content-Encoding");
    if (!mbHeader.has_value()) {
        return false;
    }
    auto header = mbHeader.value();

    auto directives = header.SplitValue();
    return std::find(directives.begin(), directives.end(), "gzip") != directives.end();
}

THttpHeaders ExpandContentEncoding(const THttpHeaders& headers) {
    std::vector<THttpHeader> ret;
    bool changed = false;
    for (size_t i = 0; i < headers.Size(); i++) {
        if (headers[i].Key() == "Content-Encoding") {
            ret.emplace_back(
                "Content-Encoding",
                headers[i].Value() + ", gzip"
            );
            changed = true;
        } else {
            ret.emplace_back(headers[i]);
        }
    }
    if (!changed) {
        ret.emplace_back("Content-Encoding", "gzip");
    }
    return THttpHeaders{ret};
}

std::string Compress(const std::string& data) {
    std::istringstream in(data);
    std::ostringstream out;

    boost::iostreams::filtering_istreambuf filter;
    filter.push(boost::iostreams::gzip_compressor());
    filter.push(in);
    boost::iostreams::copy(filter, out);

    return out.str();
}

}

bool CompressionSupported(const THttpRequest& request) {
    auto mbHeader = request.Headers().Find("Accept-Encoding");
    if (!mbHeader.has_value()) {
        return false;
    }

    auto supported = mbHeader.value().SplitValue();
    return std::find(supported.begin(), supported.end(), "gzip") != supported.end();
}

void Compress(THttpResponse& response) {
    if (IsCompressed(response)) {
        return;
    }

    response = THttpResponse {
        response.ResponseStatusLine(),
        ExpandContentEncoding(response.Headers()),
        Compress(response.Data())
    };

    response.UpdateContentLength();
}

}
