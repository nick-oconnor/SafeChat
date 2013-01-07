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

#ifndef client_h
#define	client_h

#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "crypto.h"

#define __version       4.1
#define __timeout       30

class Client {
public:

    Client(int argc, char *argv[]);
    ~Client();

    int start();

    static void *terminal_listener(void *client) {
        return ((Client *) client)->terminal_listener();
    }

    static void *network_listener(void *client) {
        return ((Client *) client)->network_listener();
    }

    static void *keepalive_sender(void *client) {
        return ((Client *) client)->keepalive_sender();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    typedef std::vector<std::pair<int, std::string> > peers_t;

    bool _network_data, _terminal_data;
    int _port, _socket;
    std::string _config_path, _name, _server, _file_path, _peer_name, _string;
    time_t _time;
    pthread_t _terminal_listener, _network_listener, _keepalive_sender;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
    block_t _block;
    Crypto _crypto;

    void shell();
    void *terminal_listener();
    void *network_listener();
    void *keepalive_sender();
    void get_string(std::string &dest);
    void send_block(const block_t &source);
    void recv_block(block_t &dest);
    std::string trim_path(std::string path);
    std::string format_size(long bytes);
    std::string format_time(long seconds);

};

#endif
