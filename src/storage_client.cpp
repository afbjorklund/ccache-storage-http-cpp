// SPDX-License-Identifier: MIT
// Copyright 2026 Joel Rosdahl

#include "storage_client.hpp"

#include "logger.hpp"
#include "version.hpp"

#include <cstring>
#include <sstream>

namespace {

const uint ERROR_SIZE = 128;

const uint DEFAULT_PORT = 6379;

static std::string build_url(const Config& config, const std::string& hex_key)
{
  std::string base_url = config.prefix;

  base_url += ':';

  std::ostringstream url;
  url << base_url;

  url << hex_key;

  return url.str();
}

} // namespace

StorageClient::StorageClient(uv_loop_t& loop, const Config& config)
  : _loop(loop),
    _config(config)
{
#ifdef HAVE_HIREDIS_SSL
  redisInitOpenSSL();
#endif
}

StorageClient::~StorageClient()
{
  if (_context) {
    _active_commands.clear();
#ifdef HAVE_HIREDIS_SSL
    redisFreeSSLContext(_ssl_context);
#endif
    redisAsyncFree(_context);
  }
}

bool StorageClient::init()
{
  uv_timer_init(&_loop, &_timeout_timer);
  _timeout_timer.data = this;

  return true;
}

void StorageClient::connect()
{
  auto url = _config.url;
  bool secure = false;

  const char* host = _config.host.empty() ? "localhost" : _config.host.c_str();
  const int port = _config.port.empty() ? DEFAULT_PORT : std::stoi(_config.port);

#ifdef HAVE_HIREDIS_SSL
  redisSSLContextError ssl_error;
  if (secure) {
    _ssl_context = redisCreateSSLContext(NULL, NULL, NULL, NULL, NULL, &ssl_error);
  }
#endif
  if (_config.scheme == "redis+unix") {
    _context = redisAsyncConnectUnix(_config.path.c_str());
  } else {
    _context = redisAsyncConnect(host, port);
  }
#ifdef HAVE_HIREDIS_SSL
  if (secure) {
    redisInitiateSSLWithContext(reinterpret_cast<redisContext*>(_context), _ssl_context);
  }
#endif

  redisLibuvAttach(_context, &_loop);
  redisAsyncSetConnectCallback(_context, connect_callback);

  if (!_config.pass.empty()) {
    redisAsyncCommand(_context, nullptr, nullptr, "AUTH %s", _config.pass.c_str());
  }
}

static bool connect_failed = false;

void StorageClient::connect_callback(const redisAsyncContext*, int status)
{
  if (status != REDIS_OK) {
    LOG("connect error");
    connect_failed = true;
  }
}

void StorageClient::get(const std::string& hex_key, StorageCallback&& callback)
{
  auto command = std::make_unique<RedisCommand>();
  command->operation = RedisOperation::GET;
  command->url = build_url(_config, hex_key);
  command->callback = std::move(callback);

  LOG("GET " + hex_key);

  uv_work_t* handle = create_command_context(command.get(), "GET %s", command->url.c_str());
  if (!handle) {
    command->callback(StorageResponse{StorageResult::ERROR, "Failed to create redis command", {}});
    return;
  }

  _active_commands[handle] = std::move(command);
}

void StorageClient::put(const std::string& hex_key,
                        std::vector<uint8_t>&& data,
                        bool overwrite,
                        StorageCallback&& callback)
{
  LOG("SET " + hex_key + " (" + std::to_string(data.size())
      + " bytes, overwrite=" + (overwrite ? "true" : "false") + ")");

  if (overwrite) {
    do_put(hex_key, std::move(data), std::move(callback));
  } else {
    std::string url = build_url(_config, hex_key);
    auto command = std::make_unique<RedisCommand>();
    command->operation = RedisOperation::EXISTS;
    command->url = url;

    uv_work_t* handle = create_command_context(command.get(), "EXISTS %s", command->url.c_str());
    if (!handle) {
      callback(StorageResponse{StorageResult::ERROR, "Failed to create redis command", {}});
      return;
    }

    command->callback = [this, hex_key, data = std::move(data), callback = std::move(callback)](
                          StorageResponse&& response) mutable {
      if (response.result == StorageResult::NOOP) {
        LOG("EXISTS check: resource doesn't exist, proceeding with SET");
        do_put(hex_key, std::move(data), std::move(callback));
      } else if (response.result == StorageResult::OK) {
        LOG("EXISTS check: resource exists, not overwriting");
        callback(StorageResponse{StorageResult::NOOP, "", {}});
      } else {
        callback(std::move(response));
      }
    };

    _active_commands[handle] = std::move(command);
  }
}

void StorageClient::do_put(const std::string& hex_key,
                           std::vector<uint8_t>&& data,
                           StorageCallback&& callback)
{
  auto command = std::make_unique<RedisCommand>();
  command->operation = RedisOperation::SET;
  command->url = build_url(_config, hex_key);
  auto request_data = std::move(data);
  command->callback = std::move(callback);

  uv_work_t* handle = create_command_context(
    command.get(), "SET %s %b", command->url.c_str(), request_data.data(), request_data.size());
  if (!handle) {
    command->callback(StorageResponse{StorageResult::ERROR, "Failed to create redis command", {}});
    return;
  }

  _active_commands[handle] = std::move(command);
}

void StorageClient::remove(const std::string& hex_key, StorageCallback&& callback)
{
  auto command = std::make_unique<RedisCommand>();
  command->operation = RedisOperation::DEL;
  command->url = build_url(_config, hex_key);
  command->callback = std::move(callback);

  LOG("DEL " + hex_key);

  uv_work_t* handle = create_command_context(command.get(), "DEL %s", command->url.c_str());
  if (!handle) {
    command->callback(StorageResponse{StorageResult::ERROR, "Failed to create redis command", {}});
    return;
  }

  _active_commands[handle] = std::move(command);
}

uv_work_t* StorageClient::create_command_context(RedisCommand* command, const char* format, ...)
{
  auto ctx = new RedisCommandContext;
  ctx->command = command;
  ctx->client = this;
  ctx->req_handle.data = ctx;

  connect();

  va_list args;
  va_start(args, format);
  int err = redisvAsyncCommand(_context, command_callback, ctx, format, args);
  va_end(args);
  if (err != REDIS_OK) {
    LOG("fail");
    LOG(_context->errstr);
    return nullptr;
  }

  // TODO proper connect handling
  redisAsyncHandleRead(_context);
  if (connect_failed) {
    return nullptr;
  }

  return &ctx->req_handle;
}

void StorageClient::command_completed(RedisCommandContext* context)
{
  RedisCommand* command = context->command;
  int err = command->error;

  uv_work_t* handle = &context->req_handle;

  StorageResult redis_result = StorageResult::ERROR;
  std::string error;

  if (err != 0) {
    error = command->error_buf;
    LOG("Redis error: " + error);
    redis_result = StorageResult::ERROR;
  } else {
    switch (command->operation) {
    case RedisOperation::GET:
      if (!command->response_str.empty()) {
        redis_result = StorageResult::OK;
      } else {
        // Not found means key doesn't exist -> NOOP
        redis_result = StorageResult::NOOP;
      }
      break;
    case RedisOperation::EXISTS:
      // EXISTS is used to check if resource exists before SET.
      if (command->response_int == 1) {
        // Resource exists -> OK (callback will convert to NOOP if needed)
        redis_result = StorageResult::OK;
      } else {
        // Resource doesn't exist -> NOOP (callback will proceed with SET)
        redis_result = StorageResult::NOOP;
      }
      break;

    case RedisOperation::SET:
      if (command->response_str == "OK") {
        redis_result = StorageResult::OK;
        command->response_str.clear();
      } else {
        // Precondition failed or conflict -> NOOP (key already exists, not overwritten)
        redis_result = StorageResult::NOOP;
      }
      break;

    case RedisOperation::DEL:
      if (command->response_int == 1) {
        redis_result = StorageResult::OK;
      } else {
        // Key not found -> NOOP (nothing to remove)
        redis_result = StorageResult::NOOP;
      }
      break;
    }
  }

  LOG("Command completed: " + command->url);

  auto it = _active_commands.find(handle);
  if (it != _active_commands.end()) {
    auto req = std::move(it->second);
    _active_commands.erase(it);
    std::string response_str = std::move(command->response_str);
    std::vector<uint8_t> response_data(response_str.begin(), response_str.end());
    if (command->callback) {
      command->callback(StorageResponse{redis_result, std::move(error), response_data});
    }
  }
}

void StorageClient::command_callback(redisAsyncContext* c, void* r, void* privdata)
{
  redisReply* reply = static_cast<redisReply*>(r);
  RedisCommandContext* ctx = static_cast<RedisCommandContext*>(privdata);
  memset(ctx->command->error_buf, 0, ERROR_SIZE);
  if (reply == NULL) {
    ctx->command->error = c->err;
    if (c->errstr) {
      memcpy(ctx->command->error_buf, c->errstr, ERROR_SIZE);
    }
    return;
  }
  if (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_STATUS) {
    ctx->command->response_str.insert(
      ctx->command->response_str.end(), reply->str, reply->str + reply->len);
  } else if (reply->type == REDIS_REPLY_INTEGER) {
    ctx->command->response_int = reply->integer;
  } else if (reply->type == REDIS_REPLY_NIL) {
    ctx->command->response_str.clear();
  } else if (reply->type == REDIS_REPLY_ERROR) {
    ctx->command->error = REDIS_ERR;
  } else {
    LOG("unknown redis reply type");
    return;
  }

  uv_queue_work(&ctx->client->_loop, &ctx->req_handle, command_session, command_cleanup);

  redisAsyncDisconnect(c);
}

void StorageClient::command_session(uv_work_t* req)
{
  RedisCommandContext* ctx = static_cast<RedisCommandContext*>(req->data);

  ctx->client->command_completed(ctx);
}

void StorageClient::command_cleanup(uv_work_t* req, int)
{
  RedisCommandContext* ctx = static_cast<RedisCommandContext*>(req->data);

  free(ctx);
}
