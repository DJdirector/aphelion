#pragma once

#include <string>
#include <vector>

namespace ai {

struct HttpResponse {
  bool success = false;      // true if the request completed with a 2xx status
  long status_code = 0;
  std::string body;
  std::string error;         // curl error or "HTTP <code>" when success == false
};

// Thin RAII wrapper around a libcurl easy handle, scoped to simple
// synchronous JSON POST requests (which is all the AI providers need).
class HttpClient {
 public:
  HttpClient();
  ~HttpClient();

  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  HttpResponse post(const std::string& url,
                     const std::vector<std::string>& headers,
                     const std::string& body,
                     long timeout_seconds = 60);

 private:
  void* curl_;  // CURL*, opaque here to avoid leaking <curl/curl.h> into callers
};

}  // namespace ai
