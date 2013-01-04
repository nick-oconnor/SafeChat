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
#include <string.h>
#include <pthread.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

#define __version       4.0
#define __timeout       30
#define __key_length    256
#define __block_size    1024 * 1024

class Client {
public:

    Client(int argc, char *argv[]);
    ~Client();

    int start();

    static void *keepalive_sender(void *client) {
        return ((Client *) client)->keepalive_sender();
    }

    static void *network_listener(void *client) {
        return ((Client *) client)->network_listener();
    }

    static void *terminal_listener(void *client) {
        return ((Client *) client)->terminal_listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    typedef std::vector<std::pair<int, std::string> > peers_t;

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

    } _block;

    bool _network_data, _terminal_data, _encryption;
    int _port, _socket;
    unsigned char _key[__key_length];
    std::string _config_path, _name, _server, _file_path, _peer_name, _string;
    time_t _time;
    pthread_t _keepalive_sender, _network_listener, _terminal_listener;
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
    EVP_CIPHER_CTX _encryption_ctx, _decryption_ctx;

    void shell();
    void *keepalive_sender();
    void *network_listener();
    void *terminal_listener();
    void send_block(const block_t &block);
    void recv_block(block_t &block);
    void get_string(std::string &string);
    std::string trim_path(std::string path);
    std::string format_size(long bytes);
    std::string format_time(long seconds);

};

#endif
