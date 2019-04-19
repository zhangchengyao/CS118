//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <string>
#include <vector>

struct header {
    std::string name;
    std::string value;
};

// A request received from a client.
struct request {
    //this is all the necessary information contained by request
    std::string method;
    std::string uri;
    int http_version_major;
    int http_version_minor;
    std::vector<header> headers;
    std::string body;
    void clear() {
        method = "";
        uri = "";
        headers.clear();
    }

    std::string rawString() {
        std::string request;
        request.append(method);
        request.append(" ");
        request.append(uri);
        request.append(" ");
        request.append("HTTP/");
        request.append(std::to_string(http_version_major));
        request.append(".");
        request.append(std::to_string(http_version_minor));
        request.append("\r\n");
        for(header hd : headers) {
            request.append(hd.name);
            request.append(": ");
            request.append(hd.value);
            request.append("\r\n");
        }
        request.append("\r\n");
        return request;
    }
};