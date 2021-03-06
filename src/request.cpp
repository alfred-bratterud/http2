// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015-2016 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <request.hpp>

#include <http_parser.h>

namespace http {

static void configure_settings(http_parser_settings&) noexcept;

static void execute_parser(Request*, http_parser&, http_parser_settings&, const std::string&) noexcept;

///////////////////////////////////////////////////////////////////////////////
Request::Request(std::string request, const Limit limit)
  : Message{limit}
  , request_{std::move(request)}
  , field_{nullptr, 0}
{
  http_parser          parser;
  http_parser_settings settings;

  configure_settings(settings);
  execute_parser(this, parser, settings, request_);
}

///////////////////////////////////////////////////////////////////////////////
Method Request::method() const noexcept {
  return method_;
}

///////////////////////////////////////////////////////////////////////////////
Request& Request::set_method(const Method method) {
  method_ = method;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
const URI& Request::uri() const noexcept {
  return uri_;
}

///////////////////////////////////////////////////////////////////////////////
Request& Request::set_uri(const URI& uri) {
  uri_ = uri;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
const Version& Request::version() const noexcept {
  return version_;
}

///////////////////////////////////////////////////////////////////////////////
Request& Request::set_version(const Version& version) noexcept {
  version_ = version;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
Request& Request::reset() noexcept {
  Message::reset();
  return set_method(GET)
        .set_uri("/")
        .set_version(Version{1U, 1U});
}

///////////////////////////////////////////////////////////////////////////////
std::string Request::to_string() const {
  std::ostringstream request;
  //-----------------------------------
  request << method_ << " "      << uri_
          << " "     << version_ << "\r\n"
          << Message::to_string();
  //-----------------------------------
  return request.str();
}

///////////////////////////////////////////////////////////////////////////////
Request::operator std::string () const {
  return to_string();
}

///////////////////////////////////////////////////////////////////////////////
span& Request::field() noexcept {
  return field_;
}

///////////////////////////////////////////////////////////////////////////////
static void configure_settings(http_parser_settings& settings_) noexcept {
  http_parser_settings_init(&settings_);

  settings_.on_message_begin = [](http_parser* parser) {
    auto req = reinterpret_cast<Request*>(parser->data);
    req->set_method(
            http::method::code(
                http_method_str(static_cast<http_method>(parser->method))));
    return 0;
  };

  settings_.on_url = [](http_parser* parser, const char* at, size_t length) {
    auto req = reinterpret_cast<Request*>(parser->data);
    req->set_uri({at, length});
    return 0;
  };

  settings_.on_header_field = [](http_parser* parser, const char* at, size_t length) {
    auto req = reinterpret_cast<Request*>(parser->data);
    req->field().data = at;
    req->field().len  = length;
    return 0;
  };

  settings_.on_header_value = [](http_parser* parser, const char* at, size_t length) {
    auto req = reinterpret_cast<Request*>(parser->data);
    req->add_header(req->field(), {at, length});
    return 0;
  };

  settings_.on_body = [](http_parser* parser, const char* at, size_t length) {
    auto req = reinterpret_cast<Request*>(parser->data);
    req->add_chunk({at, length});
    return 0;
  };

  settings_.on_headers_complete = [](http_parser* parser) {
    auto req = reinterpret_cast<Request*>(parser->data);
    req->set_version(Version{parser->http_major, parser->http_minor});
    return 0;
  };
}

///////////////////////////////////////////////////////////////////////////////
static void execute_parser(Request* req, http_parser& parser, http_parser_settings& settings,
                           const std::string& data) noexcept {
  http_parser_init(&parser, HTTP_REQUEST);
  parser.data = req;
  http_parser_execute(&parser, &settings, data.data(), data.size());
}

} //< namespace http
