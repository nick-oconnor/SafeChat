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

#define __version           2.0
#define __timeout           60

#define __keepalive         0
#define __name              1
#define __available         2
#define __hosts             3
#define __try               4
#define __accept            5
#define __data              6
#define __disconnect        7

#define __max_block_size    1024 * 1024
#define __key_length        256

class Client {
public:

    typedef std::vector<std::pair<int, std::string> > host_t;

    bool _display_menu;

    Client(int argc, char *argv[]);
    ~Client();

    int start();

    static void *keepalive(void *client) {
        return ((Client *) client)->keepalive();
    }

    static void *socket_listener(void *client) {
        return ((Client *) client)->socket_listener();
    }

    static void *cin_listener(void *client) {
        return ((Client *) client)->cin_listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    bool _socket_data, _cin_data, _encryption;
    int _port, _socket;
    unsigned char _key[__key_length];
    std::string _config_path, _name, _server, _file_path, _peer_name, _string;
    time_t _time;
    pthread_t _keepalive, _socket_listener, _cin_listener;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
    EVP_CIPHER_CTX _encryption_ctx, _decryption_ctx;
    Block _block;

    void shell();
    void *keepalive();
    void *socket_listener();
    void *cin_listener();
    void send_block(const Block &block);
    Block &recv_block(Block &block);
    std::string &cin_string(std::string &string);
    std::string trim_path(std::string path);
    std::string format_size(long bytes);
    std::string format_time(long seconds);

};

#endif
