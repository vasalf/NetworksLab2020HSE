#include <HTTP.h>

#include <algorithm>
#include <cassert>
#include <charconv>

namespace NHttpProxy {

THttpRequestLine::THttpRequestLine(
    const std::string& method,
    const std::string& url,
    const std::string& httpVersion
)
    : Method_(method)
    , URL_(url)
    , HttpVersion_(httpVersion)
{}

const std::string& THttpRequestLine::Method() const {
    return Method_;
}

const std::string& THttpRequestLine::URL() const {
    return URL_;
}

const std::string& THttpRequestLine::HttpVersion() const {
    return HttpVersion_;
}

std::string THttpRequestLine::Serialize() const {
    return Method_ + " " + URL_ + " " + HttpVersion_;
}

THttpResponseStatusLine::THttpResponseStatusLine(
    const std::string& httpVersion,
    const std::string& statusCode,
    const std::string& reason
)
    : HttpVersion_(httpVersion)
    , StatusCode_(statusCode)
    , Reason_(reason)
{}

const std::string& THttpResponseStatusLine::HttpVersion() const {
    return HttpVersion_;
}

const std::string& THttpResponseStatusLine::StatusCode() const {
    return StatusCode_;
}

const std::string& THttpResponseStatusLine::Reason() const {
    return Reason_;
}

std::string THttpResponseStatusLine::Serialize() const {
    return HttpVersion_ + " " + StatusCode_ + " " + Reason_;
}

THttpHeader::THttpHeader(
    const std::string& key,
    const std::string& value
)
    : Key_(key)
    , Value_(value)
{}

const std::string& THttpHeader::Key() const {
    return Key_;
}

const std::string& THttpHeader::Value() const {
    return Value_;
}

std::vector<std::string_view> THttpHeader::SplitValue() const {
    std::vector<std::string_view> ret;
    std::size_t start = 0;
    for (std::size_t i = 0; i != Value_.size(); i++) {
        if (Value_[i] == ' ' || Value_[i] == ',') {
            if (i > start) {
                ret.emplace_back(std::string_view(Value_).substr(start, i - start));
            }
            start = i + 1;
        }
    }
    if (start < Value_.size()) {
        ret.emplace_back(std::string_view(Value_).substr(start, Value_.size() - start));
    }
    return ret;
}

std::string THttpHeader::Serialize() const {
    return Key_ + ": " + Value_;
}

THttpHeaders::THttpHeaders(const std::vector<THttpHeader>& headers)
    : Headers_(headers)
{
    for (const auto& header : Headers_) {
        auto hint = Values_.find(header.Key());
        if (hint == Values_.end() || hint->first != header.Key()) {
            Values_.emplace_hint(hint, header.Key(), header.Value());
        }
    }
}

std::size_t THttpHeaders::Size() const {
    return Headers_.size();
}

const THttpHeader& THttpHeaders::operator[](std::size_t i) const {
    return Headers_[i];
}

const std::string& THttpHeaders::operator[](const std::string& key) const {
    return Values_[key];
}

void THttpHeaders::Append(const THttpHeader& header) {
    Headers_.emplace_back(header);
    auto hint = Values_.find(header.Key());
    if (hint == Values_.end() || hint->first != header.Key()) {
        Values_.emplace_hint(hint, header.Key(), header.Value());
    }
}

void THttpHeaders::Update(const THttpHeader& header) {
    auto it = Values_.find(header.Key());
    if (it == Values_.end()) {
        Append(header);
        return;
    }
    it->second = header.Value();
    for (auto& h : Headers_) {
        if (h.Key() == header.Key()) {
            h = header;
        }
    }
}

void THttpHeaders::Remove(const std::string& key) {
    Values_.erase(key);
    Headers_.erase(
        std::remove_if(
            Headers_.begin(),
            Headers_.end(),
            [&key](const THttpHeader& header) {
                return header.Key() == key;
            }
        ),
        Headers_.end()
    );
}

std::optional<THttpHeader> THttpHeaders::Find(const std::string& key) const {
    auto it = Values_.find(key);

    if (it == Values_.end()) {
        return {};
    }

    return THttpHeader{it->first, it->second};
}

std::string THttpHeaders::Serialize() const {
    std::string ret;
    for (const THttpHeader& header : Headers_)
        ret += header.Serialize() + "\r\n";
    return ret;
}

THttpRequest::THttpRequest(
    const THttpRequestLine& requestLine,
    const THttpHeaders& headers,
    const std::string& data
)
    : RequestLine_(requestLine)
    , Headers_(headers)
    , Data_(data)
{}

const THttpRequestLine& THttpRequest::RequestLine() const {
    return RequestLine_;
}

const THttpHeaders& THttpRequest::Headers() const {
    return Headers_;
}

THttpHeaders& THttpRequest::Headers() {
    return Headers_;
}

const std::string& THttpRequest::Data() const {
    return Data_;
}

std::string THttpRequest::Serialize() const {
    return RequestLine_.Serialize()
        + "\r\n" + Headers_.Serialize()
        + "\r\n" + Data_;
}

THttpResponse::THttpResponse(
    const THttpResponseStatusLine& requestLine,
    const THttpHeaders& headers,
    const std::string& data
)
    : StatusLine_(requestLine)
    , Headers_(headers)
    , Data_(data)
{}

const THttpResponseStatusLine& THttpResponse::ResponseStatusLine() const {
    return StatusLine_;
}

const THttpHeaders& THttpResponse::Headers() const {
    return Headers_;
}

THttpHeaders& THttpResponse::Headers() {
    return Headers_;
}

const std::string& THttpResponse::Data() const {
    return Data_;
}

void THttpResponse::UpdateContentLength() {
    Headers_.Update({"Content-Length", std::to_string(Data_.size())});
}

std::string THttpResponse::Serialize() const {
    return StatusLine_.Serialize()
        + "\r\n" + Headers_.Serialize()
        + "\r\n" + Data_;
}

namespace {

bool EndsWith(const std::string& s, const std::string& t) {
    if (t.size() > s.size()) {
        return false;
    }
    for (std::size_t i = 0; i < t.size(); i++) {
        if (s[s.size() - 1 - i] != t[t.size() - 1 - i]) {
            return false;
        }
    }
    return true;
}

class TUntilParser {
public:
    TUntilParser(const std::string& delimiter)
        : Delimiter_(delimiter)
    {}

    EParseResult Consume(char c) {
        Parsed_.push_back(c);
        if (EndsWith(Parsed_, Delimiter_)) {
            Parsed_.erase(Parsed_.end() - Delimiter_.size(), Parsed_.end());
            return EParseResult::Parsed;
        }
        return EParseResult::Await;
    }

    void Reset() {
        Parsed_.clear();
    }

    const std::string& Parsed() const {
        return Parsed_;
    }

private:
    std::string Delimiter_;
    std::string Parsed_;
};

class TNParser {
public:
    struct TParseResult {
        EParseResult Result;
        std::optional<std::string> Parsed_;
    };

    TNParser() {}

    void SetN(int n) {
        N_ = n;
        Parsed_.clear();
        Parsed_.reserve(N_);
    }

    EParseResult Consume(char c) {
        Parsed_.push_back(c);
        N_--;
        if (N_ == 0) {
            return EParseResult::Parsed;
        }
        return EParseResult::Await;
    }

    const std::string& Parsed() const {
        return Parsed_;
    }

private:
    int N_;
    std::string Parsed_;
};

class THttpRequestLineParser {
public:
    THttpRequestLineParser()
        : MethodParser_(" ")
        , URLParser_(" ")
        , HttpVersionParser_("\r\n")
        , State_(EState::METHOD)
    {}

    EParseResult Consume(char c) {
        if (State_ == EState::METHOD) {
            if (MethodParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::URL;
            }
        } else if (State_ == EState::URL) {
            if (URLParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::HTTP_VERSION;
            }
        } else {
            if (HttpVersionParser_.Consume(c) == EParseResult::Parsed) {
                return EParseResult::Parsed;
            }
        }
        return EParseResult::Await;
    }

    THttpRequestLine Parsed() const {
        return THttpRequestLine(
            MethodParser_.Parsed(),
            URLParser_.Parsed(),
            HttpVersionParser_.Parsed()
        );
    }

private:
    TUntilParser MethodParser_;
    TUntilParser URLParser_;
    TUntilParser HttpVersionParser_;

    enum class EState {
        METHOD,
        URL,
        HTTP_VERSION
    };

    EState State_;
};

class THttpResponseStatusLineParser {
public:
    THttpResponseStatusLineParser()
        : HttpVersionParser_(" ")
        , StatusCodeParser_(" ")
        , ReasonParser_("\r\n")
        , State_(EState::HTTP_VERSION)
    {}

    EParseResult Consume(char c) {
        if (State_ == EState::HTTP_VERSION) {
            if (HttpVersionParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::STATUS_CODE;
            }
        } else if (State_ == EState::STATUS_CODE) {
            if (StatusCodeParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::REASON;
            }
        } else {
            if (ReasonParser_.Consume(c) == EParseResult::Parsed) {
                return EParseResult::Parsed;
            }
        }
        return EParseResult::Await;
    }

    THttpResponseStatusLine Parsed() const {
        return THttpResponseStatusLine(
            HttpVersionParser_.Parsed(),
            StatusCodeParser_.Parsed(),
            ReasonParser_.Parsed()
        );
    }

private:
    TUntilParser HttpVersionParser_;
    TUntilParser StatusCodeParser_;
    TUntilParser ReasonParser_;

    enum class EState {
        HTTP_VERSION,
        STATUS_CODE,
        REASON
    };

    EState State_;
};

THttpHeader ParseHeader(const std::string& header) {
    TUntilParser key(": ");
    auto it = header.begin();
    auto status = EParseResult::Await;
    while (it != header.end() && status == EParseResult::Await) {
        status = key.Consume(*(it++));
    }
    return THttpHeader(
        key.Parsed(),
        std::string(it, header.end())
    );
}

class THttpHeadersParser {
public:
    THttpHeadersParser()
        : LineParser_("\r\n")
    {}

    EParseResult Consume(char c) {
        if (LineParser_.Consume(c) == EParseResult::Parsed) {
            if (LineParser_.Parsed().empty()) {
                return EParseResult::Parsed;
            }
            Parsed_.emplace_back(ParseHeader(LineParser_.Parsed()));
            LineParser_ = TUntilParser("\r\n");
        }
        return EParseResult::Await;
    }

    THttpHeaders Parsed() const {
        return THttpHeaders(Parsed_);
    }

private:
    TUntilParser LineParser_;
    std::vector<THttpHeader> Parsed_;
};

int DataLength(THttpHeaders headers) {
    int dataLength = 0;
    for (std::size_t i = 0; i != headers.Size(); i++) {
        const auto& header = headers[i];
        if (header.Key() == "Content-Length") {
            auto [_, e] = std::from_chars(
                header.Value().c_str(),
                header.Value().c_str() + header.Value().length(),
                dataLength
            );
            if (e != std::errc()) {
                dataLength = 0;
            }
        }
    }
    return dataLength;
}

}

class THttpRequestParser::TImpl {
public:
    TImpl()
        : State_(EState::REQUEST_LINE)
    {}

    EParseResult Consume(char c) {
        if (State_ == EState::REQUEST_LINE) {
            if (RequestLineParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::HEADERS;
            }
        } else if (State_ == EState::HEADERS) {
            if (HeadersParser_.Consume(c) == EParseResult::Parsed) {
                int dataLength = DataLength(HeadersParser_.Parsed());
                if (dataLength == 0) {
                    return EParseResult::Parsed;
                }
                State_ = EState::DATA;
                DataParser_.SetN(dataLength);
            }
        } else {
            if (DataParser_.Consume(c) == EParseResult::Parsed) {
                return EParseResult::Parsed;
            }
        }
        return EParseResult::Await;
    }

    THttpRequest Parsed() const {
        return THttpRequest(
            RequestLineParser_.Parsed(),
            HeadersParser_.Parsed(),
            DataParser_.Parsed()
        );
    }

private:
    THttpRequestLineParser RequestLineParser_;
    THttpHeadersParser HeadersParser_;
    TNParser DataParser_;

    enum class EState {
        REQUEST_LINE,
        HEADERS,
        DATA
    };

    EState State_;
};

THttpRequestParser::THttpRequestParser()
    : Impl_(new TImpl())
{}

THttpRequestParser::~THttpRequestParser() = default;

void THttpRequestParser::Reset() {
    Impl_.reset(new TImpl());
}

EParseResult THttpRequestParser::Consume(char c) {
    return Impl_->Consume(c);
}

THttpRequest THttpRequestParser::Parsed() const {
    return Impl_->Parsed();
}

namespace {

bool IsChunked(const THttpHeaders& headers) {
    auto mbHeader = headers.Find("Transfer-Encoding");
    if (!mbHeader.has_value()) {
        return false;
    }
    auto directives = mbHeader.value().SplitValue();
    return std::find(directives.begin(), directives.end(), "chunked") != directives.end();
}

}

class THttpResponseParser::TImpl {
public:
    TImpl()
        : ChunkLengthParser_("\r\n")
        , State_(EState::RESPONSE_LINE)
    {}

    EParseResult Consume(char c) {
        if (State_ == EState::RESPONSE_LINE) {
            if (ResponseStatusLineParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::HEADERS;
            }
        } else if (State_ == EState::HEADERS) {
            if (HeadersParser_.Consume(c) == EParseResult::Parsed) {
                int dataLength = DataLength(HeadersParser_.Parsed());
                bool isChunked = IsChunked(HeadersParser_.Parsed());
                if (dataLength == 0 && !isChunked) {
                    return EParseResult::Parsed;
                }
                if (!isChunked) {
                    State_ = EState::DATA;
                    DataParser_.SetN(dataLength);
                } else {
                    State_ = EState::CHUNK_LENGTH;
                    Chunked_ = true;
                }
            }
        } else if (State_ == EState::DATA) {
            if (DataParser_.Consume(c) == EParseResult::Parsed) {
                return EParseResult::Parsed;
            }
        } else if (State_ == EState::CHUNK_LENGTH) {
            if (ChunkLengthParser_.Consume(c) == EParseResult::Parsed) {
                const std::string& lengthStr = ChunkLengthParser_.Parsed();
                int length;
                std::from_chars(
                    lengthStr.c_str(),
                    lengthStr.c_str() + lengthStr.size(),
                    length,
                    16
                );
                if (length == 0) {
                    return EParseResult::Parsed;
                }
                State_ = EState::CHUNK_DATA;
                ChunkParser_.SetN(length + 2);
                ChunkLengthParser_.Reset();
            }
        } else {
            if (ChunkParser_.Consume(c) == EParseResult::Parsed) {
                State_ = EState::CHUNK_LENGTH;
                ChunkedData_ += ChunkParser_.Parsed();
            }
        }
        return EParseResult::Await;
    }

    THttpResponse Parsed() const {
        auto ret = THttpResponse(
            ResponseStatusLineParser_.Parsed(),
            HeadersParser_.Parsed(),
            Chunked_ ? ChunkedData_ : DataParser_.Parsed()
        );
        if (Chunked_) {
            ret.UpdateContentLength();
            ret.Headers().Remove("Transfer-Encoding");
        }
        return ret;
    }

private:
    THttpResponseStatusLineParser ResponseStatusLineParser_;
    THttpHeadersParser HeadersParser_;
    TNParser DataParser_;

    bool Chunked_ = false;
    TUntilParser ChunkLengthParser_;
    TNParser ChunkParser_;
    std::string ChunkedData_;

    enum class EState {
        RESPONSE_LINE,
        HEADERS,
        DATA,
        CHUNK_LENGTH,
        CHUNK_DATA
    };

    EState State_;
};

THttpResponseParser::THttpResponseParser()
    : Impl_(new TImpl())
{}

THttpResponseParser::~THttpResponseParser() = default;

void THttpResponseParser::Reset() {
    Impl_.reset(new TImpl());
}

EParseResult THttpResponseParser::Consume(char c) {
    return Impl_->Consume(c);
}

THttpResponse THttpResponseParser::Parsed() const {
    return Impl_->Parsed();
}

}
