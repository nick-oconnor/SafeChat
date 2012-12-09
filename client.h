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

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include "block.h"

#define __version               2

#define __set_name              1
#define __set_available         2
#define __get_hosts             3
#define __try_host              4
#define __decline_client        5
#define __accept_client         6
#define __data                  7
#define __disconnect            8

#define __key_bits              256
#define __fragment_size         1048576

class Client {
public:

    bool _display_menu;

    Client(int argc, char *argv[]);
    ~Client();

    void start();

    static void *socket_listener(void *client) {
        return ((Client *) client)->socket_listener();
    }

    static void *cin_listener(void *client) {
        return ((Client *) client)->cin_listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    };

private:

    bool _socket_data, _cin_data, _encryption;
    int _port, _socket;
    pthread_t _socket_listener, _cin_listener;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
    std::string _config_path, _name, _server, _file_path, _peer_name, _str;
    Block _block;

    void console();
    void *socket_listener();
    void *cin_listener();
    void send_block(Block &block);
    Block &recv_block(Block &block);
    std::string &cin_str(std::string &str);
    Block &encrypt(Block &block);
    void print_help();
    std::string trim_path(std::string str);
    std::string format_size(long rate);
    std::string format_time(long seconds);
};

#endif
