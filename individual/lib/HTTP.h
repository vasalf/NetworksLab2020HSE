#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace NHttpProxy {

class THttpRequestLine {
public:
    THttpRequestLine(
        const std::string& method,
        const std::string& url,
        const std::string& httpVersion
    );

    const std::string& Method() const;
    const std::string& URL() const;
    const std::string& HttpVersion() const;

    std::string Serialize() const;

private:
    std::string Method_;
    std::string URL_;
    std::string HttpVersion_;
};

class THttpResponseStatusLine {
public:
    THttpResponseStatusLine(
        const std::string& httpVersion,
        const std::string& statusCode,
        const std::string& reason
    );

    const std::string& HttpVersion() const;
    const std::string& StatusCode() const;
    const std::string& Reason() const;

    std::string Serialize() const;

private:
    std::string HttpVersion_;
    std::string StatusCode_;
    std::string Reason_;
};

class THttpHeader {
public:
    THttpHeader(
        const std::string& key,
        const std::string& value
    );

    const std::string& Key() const;
    const std::string& Value() const;

    std::vector<std::string_view> SplitValue() const;

    std::string Serialize() const;

private:
    std::string Key_;
    std::string Value_;
};

class THttpHeaders {
public:
    THttpHeaders(const std::vector<THttpHeader>& headers);

    std::size_t Size() const;

    const THttpHeader& operator[](std::size_t i) const;
    const std::string& operator[](const std::string& key) const;

    void Append(const THttpHeader& header);

    std::string Serialize() const;

    std::optional<THttpHeader> Find(const std::string& name) const;

    void Update(const THttpHeader& header);

    void Remove(const std::string& key);

private:
    std::vector<THttpHeader> Headers_;
    mutable std::map<std::string, std::string> Values_;
};

class THttpRequest {
public:
    THttpRequest(
        const THttpRequestLine& requestLine,
        const THttpHeaders& headers,
        const std::string& data
    );

    const THttpRequestLine& RequestLine() const;
    const THttpHeaders& Headers() const;
    THttpHeaders& Headers();
    const std::string& Data() const;

    std::string Serialize() const;

private:
    THttpRequestLine RequestLine_;
    THttpHeaders Headers_;
    std::string Data_;
};

enum class EParseResult {
    Await,
    Parsed
};

class THttpRequestParser {
public:
    THttpRequestParser();
    ~THttpRequestParser();

    void Reset();

    EParseResult Consume(char c);

    THttpRequest Parsed() const;

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

class THttpResponse {
public:
    THttpResponse(
        const THttpResponseStatusLine& responseStatusLine,
        const THttpHeaders& headers,
        const std::string& data
    );

    const THttpResponseStatusLine& ResponseStatusLine() const;
    const THttpHeaders& Headers() const;
    THttpHeaders& Headers();
    const std::string& Data() const;

    std::string Serialize() const;

    void UpdateContentLength();

private:
    THttpResponseStatusLine StatusLine_;
    THttpHeaders Headers_;
    std::string Data_;
};

class THttpResponseParser {
public:
    THttpResponseParser();
    ~THttpResponseParser();

    void Reset();

    EParseResult Consume(char c);

    THttpResponse Parsed() const;

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
