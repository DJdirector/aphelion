#include "http_client.h"

#include <curl/curl.h>

namespace ai {

namespace {

// curl_global_init/cleanup must run exactly once per process. This RAII
// guard handles that regardless of how many HttpClient instances get made.
struct CurlGlobalInit {
  CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobalInit() { curl_global_cleanup(); }
};
const CurlGlobalInit g_curl_global_init;

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* buffer = static_cast<std::string*>(userdata);
  buffer->append(ptr, size * nmemb);
  return size * nmemb;
}

}  // namespace

HttpClient::HttpClient() : curl_(curl_easy_init()) {}

HttpClient::~HttpClient() {
  if (curl_) {
    curl_easy_cleanup(static_cast<CURL*>(curl_));
  }
}

HttpResponse HttpClient::post(const std::string& url,
                               const std::vector<std::string>& headers,
                               const std::string& body,
                               long timeout_seconds) {
  HttpResponse result;

  auto* curl = static_cast<CURL*>(curl_);
  if (!curl) {
    result.error = "Failed to initialize curl handle";
    return result;
  }

  curl_easy_reset(curl);

  std::string response_body;
  struct curl_slist* header_list = nullptr;
  for (const auto& h : headers) {
    header_list = curl_slist_append(header_list, h.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    result.error = curl_easy_strerror(res);
    result.success = false;
  } else {
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    result.status_code = status;
    result.body = response_body;
    result.success = (status >= 200 && status < 300);
    if (!result.success) {
      result.error = "HTTP " + std::to_string(status);
    }
  }

  curl_slist_free_all(header_list);
  return result;
}

}  // namespace ai
