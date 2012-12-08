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

#include <iostream>
#include <string.h>
#include <stdlib.h>

class Block {
public:

    short _cmd;
    long _size;
    char *_data;

    Block(short cmd, const void *data, long size);
    ~Block();
    
    Block &set(short cmd, const void *data, long size);
};

#endif
