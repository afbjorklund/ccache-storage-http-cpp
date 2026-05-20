// SPDX-License-Identifier: MIT
// Copyright 2026 Joel Rosdahl

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifndef _WIN32
#  include <sys/types.h>
#endif

struct Config
{
  std::string ipc_endpoint;
  std::string url;
  std::string scheme;
  std::string user;
  std::string pass;
  std::string host;
  std::string port;
  std::string path;
  std::string prefix;
  unsigned int idle_timeout_seconds = 0;

  std::vector<std::string> diagnostics;

  // Attributes from CRSH_ATTR_*
  std::optional<std::string> bearer_token;
  bool use_netrc = false;
  std::optional<std::string> netrc_file;
};

std::optional<Config> parse_config();
