/*
  Copyright Xphysics 2012. All Rights Reserved.

  SafeChat is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  SafeChat is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE. See the GNU General Public License for more details.

  <http://www.gnu.org/licenses/>
 */

#ifndef block_h
#define	block_h

#include <string.h>

#define __block_size    1024 * 1024

struct block_t {

    enum cmd_t {
        keepalive, version, full, name, add, list, connect, unavailable, data, disconnect
    } _cmd;
    int _size;
    unsigned char *_data;

    block_t() {
        _data = new unsigned char[__block_size];
    }

    block_t(cmd_t cmd) {
        _cmd = cmd;
        _size = 0;
        _data = new unsigned char[__block_size];
    }

    block_t(cmd_t cmd, int size) {
        _cmd = cmd;
        _size = size;
        _data = new unsigned char[__block_size];
    }

    block_t(cmd_t cmd, const void *data, int size) {
        _cmd = cmd;
        _size = size;
        _data = new unsigned char[__block_size];
        memcpy(_data, data, size);
    }

    ~block_t() {
        delete[] _data;
    }

    block_t &operator=(const block_t & block) {
        _cmd = block._cmd;
        _size = block._size;
        memcpy(_data, block._data, block._size);
        return *this;
    }

};

#endif
