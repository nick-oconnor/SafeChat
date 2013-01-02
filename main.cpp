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

#include "client.h"

Client *client;

void main_handler(int signal) {
    delete client;
    exit(signal);
}

int main(int argc, char **argv) {
    signal(SIGINT, main_handler);
    client = new Client(argc, argv);
    if (client->start()) {
        delete client;
        return EXIT_FAILURE;
    }
    delete client;
    return EXIT_SUCCESS;
}
