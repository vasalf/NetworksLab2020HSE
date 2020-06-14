#pragma once

#include <HTTP.h>

namespace NHttpProxy {

bool CompressionSupported(const THttpRequest& request);

void Compress(THttpResponse& response);

}
