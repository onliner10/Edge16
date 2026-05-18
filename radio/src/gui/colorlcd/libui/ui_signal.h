/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#define SIGSLOT_EDGE16_NO_STD_MUTEX_SIGNAL 1
#include <sigslot/signal.hpp>

#include <stdint.h>
#include <utility>
#include <vector>

class UiScopedConnection
{
 public:
  UiScopedConnection() = default;

  UiScopedConnection(const UiScopedConnection&) = delete;
  UiScopedConnection& operator=(const UiScopedConnection&) = delete;

  UiScopedConnection(UiScopedConnection&& other) noexcept :
      connection(std::move(other.connection))
  {
  }

  UiScopedConnection& operator=(UiScopedConnection&& other) noexcept
  {
    if (this != &other) {
      disconnect();
      connection = std::move(other.connection);
    }
    return *this;
  }

  ~UiScopedConnection() { disconnect(); }

  bool connected() const { return connection.connected(); }
  void disconnect() { connection.disconnect(); }

 private:
  template <typename... Args>
  friend class UiSignal;

  explicit UiScopedConnection(sigslot::connection&& conn) :
      connection(std::move(conn))
  {
  }

  sigslot::scoped_connection connection;
};

class UiConnectionBag
{
 public:
  UiConnectionBag() = default;
  ~UiConnectionBag() { disconnectAll(); }

  UiConnectionBag(const UiConnectionBag&) = delete;
  UiConnectionBag& operator=(const UiConnectionBag&) = delete;

  void add(UiScopedConnection&& connection)
  {
    connections.emplace_back(std::move(connection));
  }

  void disconnectAll()
  {
    for (auto& connection : connections) {
      connection.disconnect();
    }
    connections.clear();
  }

  bool empty() const { return connections.empty(); }

 private:
  std::vector<UiScopedConnection> connections;
};

template <typename... Args>
class UiSignal
{
 public:
  template <typename Callable>
  UiScopedConnection connect(Callable&& callback, int32_t group = 0)
  {
    return UiScopedConnection(
        signal.connect(std::forward<Callable>(callback), group));
  }

  void emit(Args... args) { signal(std::forward<Args>(args)...); }

  void disconnectAll() { signal.disconnect_all(); }

 private:
  sigslot::signal_st<Args...> signal;
};
