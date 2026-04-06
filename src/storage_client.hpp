// SPDX-License-Identifier: MIT
// Copyright 2026 Joel Rosdahl

#pragma once

#include "config.hpp"

#include <hiredis/async.h>
#include <hiredis/hiredis.h>
#ifdef HAVE_HIREDIS_SSL
#  include <hiredis/hiredis_ssl.h>
#endif
#include <hiredis/adapters/libuv.h>
#include <uv.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class StorageResult { OK, NOOP, ERROR };

struct StorageResponse
{
  StorageResult result;
  std::string error;
  std::vector<uint8_t> data;
};

using StorageCallback = std::function<void(StorageResponse&&)>;

enum class RedisOperation { GET, SET, DEL, EXISTS };

struct RedisCommand
{
  RedisOperation operation;
  std::string url;
  long long response_int;
  std::string response_str;
  StorageCallback callback;
  int error;
  char error_buf[128] = {0};
};

class StorageClient;

struct RedisCommandContext
{
  uv_work_t req_handle;
  RedisCommand* command;
  StorageClient* client;
};

class StorageClient
{
public:
  StorageClient(uv_loop_t& loop, const Config& config);
  ~StorageClient();

  bool init();

  void connect();

  void get(const std::string& hex_key, StorageCallback&& callback);
  void put(const std::string& hex_key,
           std::vector<uint8_t>&& data,
           bool overwrite,
           StorageCallback&& callback);
  void remove(const std::string& hex_key, StorageCallback&& callback);

private:
  void do_put(const std::string& hex_key, std::vector<uint8_t>&& data, StorageCallback&& callback);
  void command_completed(RedisCommandContext* context);

  uv_work_t* create_command_context(RedisCommand* command, const char* format, ...);

  // Static callbacks for hiredis:
  static void connect_callback(const redisAsyncContext* c, int status);
  static void command_callback(redisAsyncContext* c, void* reply, void* privdata);

  // Static callbacks for libuv:
  static void command_session(uv_work_t* req);
  static void command_cleanup(uv_work_t* req, int status);

  uv_loop_t& _loop;
  const Config& _config;
  redisAsyncContext* _context = nullptr;
#ifdef HAVE_HIREDIS_SSL
  redisSSLContext* _ssl_context = nullptr;
#endif
  uv_timer_t _timeout_timer;
  std::unordered_map<uv_work_t*, std::unique_ptr<RedisCommand>> _active_commands;
};
