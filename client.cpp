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

    std::string str;
    std::ifstream config_file;

    try {
        _display_menu = true;
        _socket_data = _cin_data = _encryption = false;
        time(&_time);
        pthread_cond_init(&_cond, NULL);
        pthread_mutex_init(&_mutex, NULL);
        _config_path = std::string(getenv("HOME")) + "/.safechat";
        config_file.open(_config_path.c_str());
        if (config_file) {
            while (std::getline(config_file, str))
                if (str.substr(0, 11) == "local_name=")
                    _name = str.substr(11);
                else if (str.substr(0, 7) == "server=")
                    _server = str.substr(7);
                else if (str.substr(0, 5) == "port=")
                    _port = atoi(str.substr(5).c_str());
                else if (str.substr(0, 10) == "file_path=")
                    _file_path = str.substr(10);
            config_file.close();
        }
        for (int i = 1; i < argc; i++) {
            str = argv[i];
            if (str == "-n" && i + 1 < argc)
                _name = argv[++i];
            else if (str == "-s" && i + 1 < argc)
                _server = argv[++i];
            else if (str == "-p" && i + 1 < argc)
                _port = atoi(argv[++i]);
            else if (str == "-f" && i + 1 < argc)
                _file_path = argv[++i];
            else if (str == "-v") {
                std::cout << "SafeChat version " << __version << "\n" << std::flush;
                exit(EXIT_SUCCESS);
            } else
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
        std::cout << "SafeChat (version " << __version << ") - (c) 2012 Nicholas Pitt\nhttps://www.xphysics.net/\n\n    -n <name> Specifies the name forwarded to the SafeChat server (use quotes)\n    -s <serv> Specifies the DNS name or IP address of a SafeChat server\n    -p <port> Specifies the port the SafeChat server is running on\n    -f <path> Specifies the file transfer path (use quotes)\n    -v Displays the version\n\n" << std::flush;
        std::cerr << "SafeChat: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

Client::~Client() {

    std::ofstream config_file;
    Block block(0, NULL, 0);

    try {
        pthread_kill(_keep_alive, SIGTERM);
        pthread_kill(_socket_listener, SIGTERM);
        pthread_kill(_cin_listener, SIGTERM);
        send_block(block.set(__disconnect, NULL, 0));
        close(_socket);
        pthread_mutex_unlock(&_mutex);
        pthread_cond_destroy(&_cond);
        pthread_mutex_destroy(&_mutex);
        config_file.open(_config_path.c_str());
        if (!config_file)
            throw std::runtime_error("can't write " + _config_path);
        config_file << "Config file for SafeChat\n\nlocal_name=" << _name << "\nserver=" << _server << "\nport=" << _port << "\nfile_path=" << _file_path;
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
}

void Client::start() {

    int client_id, hosts_size, host_id, host_choice, response;
    unsigned char *key;
    std::string str;
    std::vector< std::pair<int, std::string> > hosts;
    hostent *host;
    sockaddr_in addr;
    DH *dh;
    BIGNUM *public_key = BN_new();
    Block block(0, NULL, 0);

    try {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        host = gethostbyname(_server.c_str());
        if (!host)
            throw std::runtime_error("can't resolve " + _server);
        addr.sin_family = AF_INET;
        memcpy(&addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
        addr.sin_port = htons(_port);
        if (connect(_socket, (sockaddr *) & addr, sizeof addr))
            throw std::runtime_error("can't connect to " + _server);
        pthread_create(&_keep_alive, NULL, &Client::keep_alive, this);
        pthread_create(&_socket_listener, NULL, &Client::socket_listener, this);
        pthread_create(&_cin_listener, NULL, &Client::cin_listener, this);
        if (*(short *) recv_block(block)._data != __version)
            throw std::runtime_error("wrong server version");
        if (*(bool *) recv_block(block)._data)
            throw std::runtime_error(_server + " is currently full");
        send_block(block.set(__set_name, _name.c_str(), _name.size() + 1));
        do {
            do {
                std::cout << "\nMain Menu\n\n    1) Start new host\n    2) Connect to host\n\nChoice: " << std::flush;
                cin_str(str);
            } while (str != "1" && str != "2");
            if (str == "1") {
                _display_menu = false;
                send_block(block.set(__set_available, NULL, 0));
                do {
                    std::cout << "\nWaiting for client to connect..." << std::flush;
                    client_id = *(int *) recv_block(block)._data;
                    _peer_name = recv_block(block)._data;
                    std::cout << "\n" << std::flush;
                    do {
                        std::cout << "Accept connection from " << _peer_name << "? (y/n) " << std::flush;
                        cin_str(str);
                    } while (str != "y" && str != "Y" && str != "n" && str != "N");
                    if (str != "y" && str != "Y")
                        send_block(block.set(__decline_client, &client_id, sizeof client_id));
                    else {
                        send_block(block.set(__accept_client, &client_id, sizeof client_id));
                        dh = DH_generate_parameters(__key_bits, 5, NULL, NULL);
                        send_block(block.set(__send_data, BN_bn2hex(dh->p), strlen(BN_bn2hex(dh->p)) + 1));
                        DH_generate_key(dh);
                        send_block(block.set(__send_data, BN_bn2hex(dh->pub_key), strlen(BN_bn2hex(dh->p)) + 1));
                        BN_hex2bn(&public_key, recv_block(block)._data);
                        key = new unsigned char[DH_size(dh)];
                        DH_compute_key(key, public_key, dh);
                        initstate(1, (char *) key, DH_size(dh));
                        console();
                        delete[] key;
                    }
                } while (str != "y" && str != "Y");
            } else {
                _display_menu = false;
                do {
                    send_block(block.set(__get_hosts, NULL, 0));
                    hosts_size = *(int *) recv_block(block)._data;
                    if (!hosts_size) {
                        std::cout << "\nNo available hosts\n" << std::flush;
                        _display_menu = true;
                        break;
                    } else {
                        hosts.clear();
                        for (int i = 0; i < hosts_size; i++) {
                            host_id = *(int *) recv_block(block)._data;
                            hosts.push_back(std::make_pair(host_id, recv_block(block)._data));
                        }
                        do {
                            std::cout << "\nHosts:\n\n" << std::flush;
                            for (int i = 0; i < hosts_size; i++)
                                std::cout << "    " << i + 1 << ") " << hosts[i].second << "\n" << std::flush;
                            std::cout << "\nChoice: " << std::flush;
                            cin_str(str);
                            host_choice = atoi(str.c_str());
                        } while (host_choice < 1 || host_choice > hosts_size);
                        _peer_name = hosts[host_choice - 1].second;
                        send_block(block.set(__try_host, &hosts[host_choice - 1].first, sizeof hosts[host_choice - 1].first));
                        std::cout << "\nWaiting for " << _peer_name << " to accept your connection..." << std::flush;
                        response = recv_block(block)._cmd;
                        if (response != __accept_client)
                            std::cout << "\n" << _peer_name << " declined your connection.\n" << std::flush;
                        else {
                            std::cout << "\n" << std::flush;
                            dh = DH_generate_parameters(__key_bits, 5, NULL, NULL);
                            BN_hex2bn(&dh->p, recv_block(block)._data);
                            DH_generate_key(dh);
                            send_block(block.set(__send_data, BN_bn2hex(dh->pub_key), strlen(BN_bn2hex(dh->p)) + 1));
                            BN_hex2bn(&public_key, recv_block(block)._data);
                            key = new unsigned char[DH_size(dh)];
                            DH_compute_key(key, public_key, dh);
                            initstate(1, (char *) key, DH_size(dh));
                            console();
                            delete[] key;
                        }
                    }
                } while (response != __accept_client);
            }
        } while (_display_menu);
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

void Client::console() {

    bool terminate = false, accept;
    std::string str;
    Block block(0, NULL, 0);

    _encryption = true;
    std::cout << "\nCommands:\n\n    <path> - Transfer file\n    D - Disconnect\n\n" << std::flush;
    while (!terminate) {
        try {
            pthread_mutex_lock(&_mutex);
            std::cout << _name << ": " << std::flush;
            while (!_socket_data && !_cin_data)
                pthread_cond_wait(&_cond, &_mutex);
            if (_socket_data) {
                if (_block._cmd == __send_data && _block._data[0] == '/') {

                    long file_size, bytes_sent = 0, rate, time_elapsed;
                    std::string file_path, file_name;
                    std::ofstream file;
                    time_t start_time;

                    file_name = std::string(_block._data).substr(1);
                    _socket_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                    file_size = *(long *) recv_block(block)._data;
                    do {
                        std::cout << "\r" << std::string(80, ' ') << "\rAccept transfer of " << file_name << " (" << format_size(file_size) << ")? (y/n) " << std::flush;
                        cin_str(str);
                    } while (str != "y" && str != "Y" && str != "n" && str != "N");
                    if (str != "y" && str != "y") {
                        accept = false;
                        send_block(block.set(__send_data, &accept, sizeof accept));
                    } else {
                        file_path = _file_path + file_name;
                        file.open(file_path.c_str(), std::ofstream::binary);
                        if (!file) {
                            accept = false;
                            send_block(block.set(__send_data, &accept, sizeof accept));
                            throw std::runtime_error("can't write " + file_path);
                        }
                        accept = true;
                        send_block(block.set(__send_data, &accept, sizeof accept));
                        time(&start_time);
                        std::cout << "\r" << std::string(80, ' ') << "\rReceiving " << file_name << "..." << std::flush;
                        do {
                            recv_block(block);
                            file.write(block._data, block._size);
                            bytes_sent = file.tellp();
                            time_elapsed = difftime(time(NULL), start_time);
                            if (time_elapsed) {
                                rate = bytes_sent / time_elapsed;
                                std::cout << "\r" << std::string(80, ' ') << "\rReceiving " << file_name << "... " << std::fixed << std::setprecision(0) << (double) bytes_sent / file_size * 100 << "% (" << format_time((file_size - bytes_sent) / rate) << " at " << format_size(rate) << "/s)" << std::flush;
                            } else
                                std::cout << "\r" << std::string(80, ' ') << "\rReceiving " << file_name << "... " << std::fixed << std::setprecision(0) << (double) bytes_sent / file_size * 100 << "%" << std::flush;
                        } while (bytes_sent < file_size);
                        std::cout << "\n" << std::flush;
                        file.close();
                    }
                } else {
                    if (_block._cmd == __send_data)
                        std::cout << "\r" << _peer_name << ": " << _block._data << "\n" << std::flush;
                    else {
                        std::cout << "\r" << std::string(80, ' ') << "\n" << _peer_name << " disconnected.\n\n" << std::flush;
                        terminate = true;
                    }
                    _socket_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                }
            } else {
                str = trim_path(_str);
                if (str[0] == '/') {

                    long file_size, bytes_sent = 0, rate, time_elapsed;
                    std::string file_path = str, file_name;
                    std::ifstream file;
                    time_t start_time;

                    _cin_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                    file.open(file_path.c_str(), std::ifstream::binary);
                    if (!file)
                        throw std::runtime_error("can't read " + file_path);
                    file_name = file_path.substr(file_path.rfind("/"));
                    send_block(block.set(__send_data, file_name.c_str(), file_name.size() + 1));
                    file_name = file_name.substr(1);
                    file.seekg(0, std::ifstream::end);
                    file_size = file.tellg();
                    file.seekg(0, std::ifstream::beg);
                    send_block(block.set(__send_data, &file_size, sizeof file_size));
                    std::cout << "Waiting for " << _peer_name << " to accept the file transfer..." << std::flush;
                    if (!*(bool *) recv_block(block)._data)
                        std::cout << "\n" << _peer_name << " declined the file transfer.\n" << std::flush;
                    else {
                        time(&start_time);
                        std::cout << "\nSending " << file_name << "..." << std::flush;
                        do {
                            block._size = file_size - bytes_sent;
                            if (block._size > __max_block_size)
                                block._size = __max_block_size;
                            block.set(__send_data, NULL, block._size);
                            file.read(block._data, block._size);
                            send_block(block);
                            bytes_sent = file.tellg();
                            time_elapsed = difftime(time(NULL), start_time);
                            if (time_elapsed) {
                                rate = bytes_sent / time_elapsed;
                                std::cout << "\r" << std::string(80, ' ') << "\rSending " << file_name << "... " << std::fixed << std::setprecision(0) << (double) bytes_sent / file_size * 100 << "% (" << format_time((file_size - bytes_sent) / rate) << " at " << format_size(rate) << "/s)" << std::flush;
                            } else
                                std::cout << "\r" << std::string(80, ' ') << "\rSending " << file_name << "... " << std::fixed << std::setprecision(0) << (double) bytes_sent / file_size * 100 << "%" << std::flush;
                        } while (bytes_sent < file_size);
                        std::cout << "\n" << std::flush;
                    }
                    file.close();
                } else {
                    if (_str == "D") {
                        send_block(block.set(__disconnect, NULL, 0));
                        std::cout << "\r" << std::string(80, ' ') << "\nDisconnected.\n\n" << std::flush;
                        terminate = true;
                    } else
                        send_block(block.set(__send_data, _str.c_str(), _str.size() + 1));
                    _cin_data = false;
                    pthread_cond_signal(&_cond);
                    pthread_mutex_unlock(&_mutex);
                }
            }
        } catch (const std::exception &exception) {
            std::cerr << "Error: " << exception.what() << ".\n";
        }
    }
}

void *Client::keep_alive() {

    int interval = __time_out / 3;
    Block block(0, NULL, 0);

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(interval);
        if (difftime(time(NULL), _time) > interval)
            send_block(block.set(__keep_alive, NULL, 0));
    }
    return NULL;
}

void *Client::socket_listener() {

    try {
        signal(SIGTERM, thread_handler);
        while (true) {
            if (!recv(_socket, &_block._cmd, sizeof _block._cmd, MSG_WAITALL))
                throw std::runtime_error("dropped connection");
            if (!recv(_socket, &_block._size, sizeof _block._size, MSG_WAITALL))
                throw std::runtime_error("dropped connection");
            if (_block._size > __max_block_size)
                throw "oversized block received";
            _block.set(_block._cmd, NULL, _block._size);
            if (_block._size)
                if (!recv(_socket, _block._data, _block._size, MSG_WAITALL))
                    throw std::runtime_error("dropped connection");
            if (_encryption)
                encrypt(_block);
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
        std::getline(std::cin, _str);
        _cin_data = true;
        pthread_mutex_lock(&_mutex);
        pthread_cond_signal(&_cond);
        while (_cin_data)
            pthread_cond_wait(&_cond, &_mutex);
        pthread_mutex_unlock(&_mutex);
    }
    return NULL;
}

void Client::send_block(Block &block) {
    if (_encryption)
        encrypt(block);
    send(_socket, &block._cmd, sizeof block._cmd, 0);
    send(_socket, &block._size, sizeof block._size, 0);
    if (block._size)
        send(_socket, block._data, block._size, 0);
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

std::string &Client::cin_str(std::string &str) {
    pthread_mutex_lock(&_mutex);
    while (!_cin_data)
        pthread_cond_wait(&_cond, &_mutex);
    str = _str;
    _cin_data = false;
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_mutex);
    return str;
}

Block &Client::encrypt(Block &block) {
    for (long i = 0; i < block._size; i++)
        block._data[i] ^= random() % 255;
    return block;
}

std::string Client::trim_path(std::string str) {

    size_t pos;

    str = str.substr(0, str.find_last_not_of(" '\"") + 1);
    str = str.substr(str.find_first_not_of(" '\""));
    pos = str.find("\\");
    while (pos != str.npos) {
        str.erase(pos, 1);
        pos = str.find("\\");
    }
    return str;
}

std::string Client::format_size(long size) {

    double gb = 1000 * 1000 * 1000, mb = 1000 * 1000, kb = 1000;
    std::string str;
    std::stringstream stream;

    gb = size / gb;
    mb = size / mb;
    kb = size / kb;
    if (gb >= 1) {
        stream << std::fixed << std::setprecision(1) << gb;
        str = stream.str() + " GB";
    } else if (mb >= 1) {
        stream << std::fixed << std::setprecision(1) << mb;
        str = stream.str() + " MB";
    } else if (kb >= 1) {
        stream << std::fixed << std::setprecision(0) << kb;
        str = stream.str() + " KB";
    } else {
        stream << size;
        str = stream.str() + " B";
    }
    return str;
}

std::string Client::format_time(long seconds) {

    std::string str;
    std::stringstream stream;

    if (seconds / (60 * 60) >= 1) {
        stream << seconds / (60 * 60);
        str = stream.str() + " hrs";
    } else if (seconds / 60 >= 1) {
        stream << seconds / 60;
        str = stream.str() + " min";
    } else {
        stream << seconds;
        str = stream.str() + " sec";
    }
    return str;
}
