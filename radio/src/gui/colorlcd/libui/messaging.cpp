/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   libopenui - https://github.com/opentx/libopenui
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "messaging.h"

static UiSignal<uint32_t> messageSignals[Messaging::REFRESH_OUTPUTS_WIDGET + 1];
static int32_t nextMessageGroup = 0;

Messaging::~Messaging()
{
  unsubscribe();
}

void Messaging::subscribe(uint32_t _id, std::function<void(uint32_t)> cb)
{
  unsubscribe();
  id = _id;
  callback = cb;

  if (id <= Messaging::REFRESH_OUTPUTS_WIDGET) {
    connection = messageSignals[id].connect(
        [this](uint32_t data) {
          if (callback) callback(data);
        },
        --nextMessageGroup);
  }
}

void Messaging::unsubscribe()
{
  connection.disconnect();
  callback = nullptr;
  id = 0;
}

void Messaging::send(uint32_t id)
{
  send(id, 0);
}

void Messaging::send(uint32_t msgId, uint32_t msgData)
{
  if (msgId <= Messaging::REFRESH_OUTPUTS_WIDGET) messageSignals[msgId].emit(msgData);
}
