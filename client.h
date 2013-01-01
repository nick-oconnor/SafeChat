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
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include "block.h"

#define __version           4
#define __time_out          60

#define __keep_alive        1
#define __set_name          2
#define __set_available     3
#define __get_hosts         4
#define __try_host          5
#define __decline_client    6
#define __accept_client     7
#define __send_data         8
#define __disconnect        9

#define __max_block_size    1000000
#define __key_length        256

class Client {
public:

    typedef std::vector<std::pair<int, std::string> > host_t;

    bool _display_menu;

    Client(int argc, char *argv[]);
    ~Client();

    void start();

    static void *keep_alive(void *client) {
        return ((Client *) client)->keep_alive();
    }

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
    unsigned char _key[__key_length];
    time_t _time;
    pthread_t _keep_alive, _socket_listener, _cin_listener;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
    std::string _config_path, _name, _server, _file_path, _peer_name, _string;
    EVP_CIPHER_CTX _encryption_ctx, _decryption_ctx;
    Block _block;

    void console();
    void *keep_alive();
    void *socket_listener();
    void *cin_listener();
    void init_cipher();
    void send_block(Block &block);
    Block &recv_block(Block &block);
    std::string &cin_str(std::string &str);
    std::string trim_path(std::string str);
    std::string format_size(long rate);
    std::string format_time(long seconds);
};

#endif
