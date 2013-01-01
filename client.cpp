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

Client::Client(int argc, char *argv[]) : _block(0, NULL, 0) {

    std::string string;
    std::ifstream config_file;

    try {
        _display_menu = true;
        _socket_data = _cin_data = _encryption = false;
        time(&_time);
        pthread_cond_init(&_cond, NULL);
        pthread_mutex_init(&_mutex, NULL);
        _config_path = std::string(getenv("HOME")) + "/.safechat";
        config_file.open(_config_path.c_str());
        if (!config_file)
            throw std::runtime_error("can't read config file");
        while (std::getline(config_file, string))
            if (string.substr(0, 11) == "local_name=")
                _name = string.substr(11);
            else if (string.substr(0, 7) == "server=")
                _server = string.substr(7);
            else if (string.substr(0, 5) == "port=")
                _port = atoi(string.substr(5).c_str());
            else if (string.substr(0, 10) == "file_path=")
                _file_path = string.substr(10);
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
    try {
        for (int i = 1; i < argc; i++) {
            string = argv[i];
            if (string == "-n" && i + 1 < argc)
                _name = argv[++i];
            else if (string == "-s" && i + 1 < argc)
                _server = argv[++i];
            else if (string == "-p" && i + 1 < argc)
                _port = atoi(argv[++i]);
            else if (string == "-f" && i + 1 < argc)
                _file_path = argv[++i];
            else
                throw std::runtime_error("unknown argument '" + std::string(argv[i]) + "'");
        }
        if (_name.size() < 1)
            throw std::runtime_error("name required");
        if (_server.size() < 1)
            throw std::runtime_error("server required");
        if (_port < 1 || _port > 65535)
            throw std::runtime_error("invalid port number");
        if (_file_path.size() < 1)
            throw std::runtime_error("file transfer path required");
        _file_path = trim_path(_file_path);
        if (_file_path[_file_path.size() - 1] != '/')
            _file_path += "/";
    } catch (const std::exception &exception) {
        std::cout << "SafeChat (version " << __version << ") - (c) 2012 Nicholas Pitt\nhttps://www.xphysics.net/\n\n    -n <name> Specifies the name forwarded to the SafeChat server (use quotes)\n    -s <serv> Specifies the DNS name or IP address of a SafeChat server\n    -p <port> Specifies the port the SafeChat server is running on\n    -f <path> Specifies the file transfer path (use quotes)\n\n" << std::flush;
        std::cerr << "Error: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

Client::~Client() {

    std::ofstream config_file;
    Block block(0, NULL, 0);

    try {
        pthread_kill(_keepalive, SIGTERM);
        pthread_kill(_socket_listener, SIGTERM);
        pthread_kill(_cin_listener, SIGTERM);
        send_block(block.set(__disconnect, NULL, 0));
        close(_socket);
        pthread_mutex_unlock(&_mutex);
        pthread_cond_destroy(&_cond);
        pthread_mutex_destroy(&_mutex);
        if (_encryption) {
            EVP_CIPHER_CTX_cleanup(&_encryption_ctx);
            EVP_CIPHER_CTX_cleanup(&_decryption_ctx);
        }
        config_file.open(_config_path.c_str());
        if (!config_file)
            throw std::runtime_error("can't write config file");
        config_file << "Config file for SafeChat\n\nlocal_name=" << _name << "\nserver=" << _server << "\nport=" << _port << "\nfile_path=" << _file_path;
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
}

void Client::start_client() {

    int id, hosts_size, choice, response;
    std::string string;
    hostent *host;
    sockaddr_in addr;
    DH *dh;
    BIGNUM *pub_key;
    host_t hosts;
    Block block(0, NULL, 0);

    try {
        do {
            pthread_create(&_cin_listener, NULL, &Client::cin_listener, this);
            do {
                std::cout << "\nMain Menu\n\n    1) Start new host\n    2) Connect to host\n\nChoice: " << std::flush;
                cin_string(string);
            } while (string != "1" && string != "2");
            _socket = socket(AF_INET, SOCK_STREAM, 0);
            host = gethostbyname(_server.c_str());
            if (!host)
                throw std::runtime_error("can't resolve server name");
            addr.sin_family = AF_INET;
            memcpy(&addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
            addr.sin_port = htons(_port);
            if (connect(_socket, (sockaddr *) & addr, sizeof addr))
                throw std::runtime_error("can't connect to server");
            pthread_create(&_keepalive, NULL, &Client::keepalive, this);
            pthread_create(&_socket_listener, NULL, &Client::socket_listener, this);
            send_block(block.set(__name, _name.c_str(), _name.size() + 1));
            if (string == "1") {
                _display_menu = false;
                send_block(block.set(__available, NULL, 0));
                do {
                    std::cout << "\nWaiting for client to connect..." << std::flush;
                    id = *(int *) recv_block(block)._data;
                    _peer_name = (char *) recv_block(block)._data;
                    std::cout << "\n" << std::flush;
                    do {
                        std::cout << "Accept connection from " << _peer_name << "? (y/n) " << std::flush;
                        cin_string(string);
                    } while (string != "y" && string != "Y" && string != "n" && string != "N");
                    if (string != "y" && string != "Y")
                        send_block(block.set(__decline, &id, sizeof id));
                    else {
                        send_block(block.set(__accept, &id, sizeof id));
                        dh = DH_generate_parameters(__key_length, 5, NULL, NULL);
                        block.set(__data, NULL, BN_num_bytes(dh->p));
                        BN_bn2bin(dh->p, block._data);
                        send_block(block);
                        DH_generate_key(dh);
                        block.set(__data, NULL, BN_num_bytes(dh->pub_key));
                        BN_bn2bin(dh->pub_key, block._data);
                        send_block(block);
                        recv_block(block);
                        pub_key = BN_bin2bn(block._data, block._size, NULL);
                        DH_compute_key(_key, pub_key, dh);
                        BN_free(pub_key);
                        DH_free(dh);
                        start_shell();
                    }
                } while (string != "y" && string != "Y");
            } else {
                _display_menu = false;
                do {
                    send_block(block.set(__hosts, NULL, 0));
                    hosts_size = *(int *) recv_block(block)._data;
                    if (!hosts_size) {
                        std::cout << "\nNo available hosts.\n" << std::flush;
                        _display_menu = true;
                        break;
                    } else {
                        hosts.clear();
                        for (int i = 0; i < hosts_size; i++) {
                            id = *(int *) recv_block(block)._data;
                            hosts.push_back(std::make_pair(id, (char *) recv_block(block)._data));
                        }
                        do {
                            std::cout << "\nHosts:\n\n" << std::flush;
                            for (int i = 0; i < hosts_size; i++)
                                std::cout << "    " << i + 1 << ") " << hosts[i].second << "\n" << std::flush;
                            std::cout << "\nChoice: " << std::flush;
                            cin_string(string);
                            choice = atoi(string.c_str()) - 1;
                        } while (choice < 0 || choice >= hosts_size);
                        _peer_name = hosts[choice].second;
                        send_block(block.set(__try, &hosts[choice].first, sizeof hosts[choice].first));
                        std::cout << "\nWaiting for " << _peer_name << " to accept your connection..." << std::flush;
                        response = recv_block(block)._cmd;
                        if (response != __accept)
                            std::cout << "\n" << _peer_name << " declined your connection.\n" << std::flush;
                        else {
                            std::cout << "\n" << std::flush;
                            dh = DH_generate_parameters(__key_length, 5, NULL, NULL);
                            recv_block(block);
                            BN_bin2bn(block._data, block._size, dh->p);
                            DH_generate_key(dh);
                            block.set(__data, NULL, BN_num_bytes(dh->pub_key));
                            BN_bn2bin(dh->pub_key, block._data);
                            send_block(block);
                            recv_block(block);
                            pub_key = BN_bin2bn(block._data, block._size, NULL);
                            DH_compute_key(_key, pub_key, dh);
                            BN_free(pub_key);
                            DH_free(dh);
                            start_shell();
                        }
                    }
                } while (response != __accept);
            }
        } while (_display_menu);
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

void Client::start_shell() {

    bool terminate = false, accept;
    int max_block_size = __max_block_size - AES_BLOCK_SIZE;
    long file_size, bytes_sent, rate, time_elapsed;
    unsigned char key[32], iv[32];
    std::string string, file_path, file_name;
    std::ofstream out_file;
    std::ifstream in_file;
    time_t start_time;
    Block block(0, NULL, 0);

    EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha256(), (unsigned char *) "SafeChat", _key, __key_length, 5, key, iv);
    EVP_CIPHER_CTX_init(&_encryption_ctx);
    EVP_CIPHER_CTX_init(&_decryption_ctx);
    EVP_EncryptInit_ex(&_encryption_ctx, EVP_aes_256_cbc(), NULL, key, iv);
    EVP_DecryptInit_ex(&_decryption_ctx, EVP_aes_256_cbc(), NULL, key, iv);
    _encryption = true;
    std::cout << "\nCommands:\n\n    <path> - Transfer file\n    <entr> - Disconnect\n\n" << std::flush;
    while (!terminate) {
        try {
            pthread_mutex_lock(&_mutex);
            std::cout << _name << ": " << std::flush;
            while (!_socket_data && !_cin_data)
                pthread_cond_wait(&_cond, &_mutex);
            if (_socket_data) {
                if (_block._cmd == __data && _block._data[0] == '/') {
                    file_name = (char *) _block._data;
                    file_name = file_name.substr(1);
                    _socket_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                    file_size = *(long *) recv_block(block)._data;
                    do {
                        std::cout << "\nAccept transfer of " << file_name << " (" << format_size(file_size) << ")? (y/n) " << std::flush;
                        cin_string(string);
                    } while (string != "y" && string != "Y" && string != "n" && string != "N");
                    if (string != "y" && string != "y") {
                        accept = false;
                        send_block(block.set(__data, &accept, sizeof accept));
                    } else {
                        file_path = _file_path + file_name;
                        out_file.open(file_path.c_str(), std::ofstream::binary);
                        if (!out_file) {
                            accept = false;
                            send_block(block.set(__data, &accept, sizeof accept));
                            throw std::runtime_error("can't write file");
                        }
                        accept = true;
                        send_block(block.set(__data, &accept, sizeof accept));
                        time(&start_time);
                        std::cout << "\r" << std::string(80, ' ') << "\rReceiving " << file_name << "..." << std::flush;
                        do {
                            recv_block(block);
                            out_file.write((char *) block._data, block._size);
                            bytes_sent = out_file.tellp();
                            time_elapsed = difftime(time(NULL), start_time);
                            std::cout << "\r" << std::string(80, ' ') << "\rReceiving " << file_name << "... " << std::fixed << std::setprecision(0) << (double) bytes_sent / file_size * 100 << "%" << std::flush;
                            if (time_elapsed) {
                                rate = bytes_sent / time_elapsed;
                                std::cout << " (" << format_time((file_size - bytes_sent) / rate) << " at " << format_size(rate) << "/s)" << std::flush;
                            }
                        } while (bytes_sent < file_size);
                        std::cout << "\n" << std::flush;
                        out_file.close();
                    }
                } else {
                    if (_block._cmd == __data)
                        std::cout << "\r" << _peer_name << ": " << _block._data << "\n" << std::flush;
                    else {
                        std::cout << "\n" << std::flush;
                        terminate = true;
                    }
                    _socket_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                }
            } else {
                file_path = trim_path(_string);
                if (file_path[0] == '/') {
                    _cin_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                    in_file.open(file_path.c_str(), std::ifstream::binary);
                    if (!in_file)
                        throw std::runtime_error("can't read file");
                    file_name = file_path.substr(file_path.rfind("/"));
                    send_block(block.set(__data, file_name.c_str(), file_name.size() + 1));
                    file_name = file_name.substr(1);
                    in_file.seekg(0, std::ifstream::end);
                    file_size = in_file.tellg();
                    in_file.seekg(0, std::ifstream::beg);
                    send_block(block.set(__data, &file_size, sizeof file_size));
                    std::cout << "Waiting for " << _peer_name << " to accept the file transfer..." << std::flush;
                    if (!*(bool *) recv_block(block)._data)
                        std::cout << "\n" << _peer_name << " declined the file transfer.\n" << std::flush;
                    else {
                        time(&start_time);
                        bytes_sent = 0;
                        std::cout << "\nSending " << file_name << "..." << std::flush;
                        do {
                            block._size = file_size - bytes_sent;
                            if (block._size > max_block_size)
                                block._size = max_block_size;
                            block.set(__data, NULL, block._size);
                            in_file.read((char *) block._data, block._size);
                            send_block(block);
                            bytes_sent = in_file.tellg();
                            time_elapsed = difftime(time(NULL), start_time);
                            std::cout << "\r" << std::string(80, ' ') << "\rSending " << file_name << "... " << std::fixed << std::setprecision(0) << (double) bytes_sent / file_size * 100 << "%" << std::flush;
                            if (time_elapsed) {
                                rate = bytes_sent / time_elapsed;
                                std::cout << " (" << format_time((file_size - bytes_sent) / rate) << " at " << format_size(rate) << "/s)" << std::flush;
                            }
                        } while (bytes_sent < file_size);
                        std::cout << "\n" << std::flush;
                    }
                    in_file.close();
                } else {
                    if (_string.size())
                        send_block(block.set(__data, _string.c_str(), _string.size() + 1));
                    else {
                        send_block(block.set(__disconnect, NULL, 0));
                        terminate = true;
                    }
                    _cin_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                }
            }
        } catch (const std::exception &exception) {
            std::cerr << "Error: " << exception.what() << ".\n";
        }
    }
    std::cout << "\nDisconnected.\n\n" << std::flush;
}

void *Client::keepalive() {

    int interval = __timeout / 3;
    Block block(0, NULL, 0);

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(interval);
        if (difftime(time(NULL), _time) > interval)
            send_block(block.set(__keepalive, NULL, 0));
    }
    return NULL;
}

void *Client::socket_listener() {

    int padding;
    Block recv_block(0, NULL, 0);

    try {
        signal(SIGTERM, thread_handler);
        while (true) {
            if (!recv(_socket, &recv_block._cmd, sizeof recv_block._cmd, MSG_WAITALL))
                throw std::runtime_error("dropped connection");
            if (!recv(_socket, &recv_block._size, sizeof recv_block._size, MSG_WAITALL))
                throw std::runtime_error("dropped connection");
            if (recv_block._size > __max_block_size)
                throw std::runtime_error("oversized block received");
            recv_block.set(recv_block._cmd, NULL, recv_block._size);
            if (recv_block._size)
                if (!recv(_socket, recv_block._data, recv_block._size, MSG_WAITALL))
                    throw std::runtime_error("dropped connection");
            if (recv_block._cmd == __protocol)
                if (*(int *) recv_block._data != (int) __version)
                    throw std::runtime_error("incompatible server version");
                else
                    continue;
            if (recv_block._cmd == __full)
                if (*(bool *) recv_block._data)
                    throw std::runtime_error("server is currently full");
                else
                    continue;
            if (!_encryption)
                _block.set(recv_block._cmd, recv_block._data, recv_block._size);
            else {
                _block.set(recv_block._cmd, NULL, recv_block._size + AES_BLOCK_SIZE);
                EVP_DecryptInit_ex(&_decryption_ctx, NULL, NULL, NULL, NULL);
                EVP_DecryptUpdate(&_decryption_ctx, _block._data, &_block._size, recv_block._data, recv_block._size);
                EVP_DecryptFinal_ex(&_decryption_ctx, _block._data + _block._size, &padding);
                _block._size += padding;
            }
            _socket_data = true;
            pthread_mutex_lock(&_mutex);
            pthread_cond_signal(&_cond);
            while (_socket_data)
                pthread_cond_wait(&_cond, &_mutex);
            pthread_mutex_unlock(&_mutex);
        }
    } catch (const std::exception &exception) {
        std::cerr << "\nError: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
    return NULL;
}

void *Client::cin_listener() {
    signal(SIGTERM, thread_handler);
    while (true) {
        std::getline(std::cin, _string);
        _cin_data = true;
        pthread_mutex_lock(&_mutex);
        pthread_cond_signal(&_cond);
        while (_cin_data)
            pthread_cond_wait(&_cond, &_mutex);
        pthread_mutex_unlock(&_mutex);
    }
    return NULL;
}

void Client::send_block(const Block &block) {

    int padding;
    Block send_block(0, NULL, 0);

    if (!_encryption)
        send_block.set(block._cmd, block._data, block._size);
    else {
        send_block.set(block._cmd, NULL, block._size + AES_BLOCK_SIZE);
        EVP_EncryptInit_ex(&_encryption_ctx, NULL, NULL, NULL, NULL);
        EVP_EncryptUpdate(&_encryption_ctx, send_block._data, &send_block._size, block._data, block._size);
        EVP_EncryptFinal_ex(&_encryption_ctx, send_block._data + send_block._size, &padding);
        send_block._size += padding;
    }
    send(_socket, &send_block._cmd, sizeof send_block._cmd, 0);
    send(_socket, &send_block._size, sizeof send_block._size, 0);
    if (send_block._size)
        send(_socket, send_block._data, send_block._size, 0);
    time(&_time);
}

Block &Client::recv_block(Block &block) {
    pthread_mutex_lock(&_mutex);
    while (!_socket_data)
        pthread_cond_wait(&_cond, &_mutex);
    block.set(_block._cmd, _block._data, _block._size);
    _socket_data = false;
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_mutex);
    return block;
}

std::string &Client::cin_string(std::string &string) {
    pthread_mutex_lock(&_mutex);
    while (!_cin_data)
        pthread_cond_wait(&_cond, &_mutex);
    string = _string;
    _cin_data = false;
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_mutex);
    return string;
}

std::string Client::trim_path(std::string path) {

    size_t pos;

    if (path.size()) {
        path = path.substr(0, path.find_last_not_of(" '\"") + 1);
        path = path.substr(path.find_first_not_of(" '\""));
        pos = path.find("\\");
        while (pos != path.npos) {
            path.erase(pos, 1);
            pos = path.find("\\");
        }
    }
    return path;
}

std::string Client::format_size(long bytes) {

    double gb = 1000 * 1000 * 1000, mb = 1000 * 1000, kb = 1000;
    std::string string;
    std::stringstream stream;

    gb = bytes / gb;
    mb = bytes / mb;
    kb = bytes / kb;
    if (gb >= 1) {
        stream << std::fixed << std::setprecision(1) << gb;
        string = stream.str() + " GB";
    } else if (mb >= 1) {
        stream << std::fixed << std::setprecision(1) << mb;
        string = stream.str() + " MB";
    } else if (kb >= 1) {
        stream << std::fixed << std::setprecision(0) << kb;
        string = stream.str() + " KB";
    } else {
        stream << bytes;
        string = stream.str() + " B";
    }
    return string;
}

std::string Client::format_time(long seconds) {

    int hrs = 60 * 60, min = 60;
    std::string string;
    std::stringstream stream;

    if (seconds / hrs >= 1) {
        stream << seconds / hrs;
        string = stream.str() + " hrs";
    } else if (seconds / min >= 1) {
        stream << seconds / min;
        string = stream.str() + " min";
    } else {
        stream << seconds;
        string = stream.str() + " sec";
    }
    return string;
}
