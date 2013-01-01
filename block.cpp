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

#include "block.h"

Block::Block(short cmd, const void *data, int size) {
    _cmd = cmd;
    _size = size;
    _data = new unsigned char[_size];
    if (data != NULL)
        memcpy(_data, data, _size);
}

Block::~Block() {
    delete[] _data;
}

Block &Block::set(short cmd, const void *data, int size) {
    _cmd = cmd;
    _size = size;
    delete[] _data;
    _data = new unsigned char[_size];
    if (data != NULL)
        memcpy(_data, data, _size);
    return *this;
}
