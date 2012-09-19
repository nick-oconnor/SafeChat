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

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#define __timeout 30
#define __block_size 1024
#define __null 0
#define __keep_alive 1
#define __set_name 2
#define __set_available 3
#define __get_hosts 4
#define __try_host 5
#define __decline_client 6
#define __accept_client 7
#define __send_data 8
#define __key_length 256

class Client {
private:

    class Block {
    public:

        unsigned char _data[__block_size];

        Block(int cmd) {
            memset(_data, '\0', __block_size);
            _data[0] = (unsigned char) cmd;
        }

        Block(int cmd, const std::string &string) {
            _data[0] = (unsigned char) cmd;
            strncpy((char *) &_data[1], string.c_str(), __block_size - 1);
        }

        Block(int cmd, const char *c_string) {
            _data[0] = (unsigned char) cmd;
            strncpy((char *) &_data[1], c_string, __block_size - 1);
        }

        Block(int cmd, const long num) {

            std::stringstream string_stream;

            string_stream << num;
            _data[0] = (unsigned char) cmd;
            strncpy((char *) &_data[1], string_stream.str().c_str(), __block_size - 1);
        }

        int cmd() const {
            return (int) _data[0];
        }

        const unsigned char *c_str() const {
            return &_data[1];
        }

        const std::string str() const {
            return std::string((char *) &_data[1]);
        }

        long num() const {
            return atol((char *) &_data[1]);
        }
    };

    int _port, _socket, _key_size;
    unsigned char *_key;
    std::string _config_path, _name, _server, _download_path, _peer_name;
    pthread_t _keep_alive, _inbound, _outbound;
    pthread_mutex_t _mutex;
    pthread_attr_t _attr;

public:

    Client(int argc, char *argv[]) {

        std::string line;

        pthread_mutex_init(&_mutex, NULL);
        pthread_attr_init(&_attr);
        pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_JOINABLE);
        _config_path = std::string(getenv("HOME")) + "/.safechat";
        std::ifstream config_stream(_config_path.c_str());
        if (config_stream) {
            while (std::getline(config_stream, line)) {
                if (line.substr(0, 11) == "local_name=") {
                    _name = line.substr(11);
                } else if (line.substr(0, 7) == "server=") {
                    _server = line.substr(7);
                } else if (line.substr(0, 5) == "port=") {
                    _port = atoi(line.substr(5).c_str());
                } else if (line.substr(0, 14) == "download_path=") {
                    _download_path = line.substr(14);
                }
            }
            config_stream.close();
        }
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-n") && i + 1 < argc) {
                _name = argv[++i];
            } else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
                _server = argv[++i];
            } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
                _port = atoi(argv[++i]);
            } else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
                _download_path = argv[++i];
            } else if (!strcmp(argv[i], "--help")) {
                std::cerr << "SafeChat - (c) 2012 Nicholas Pitt \nhttps://www.xphysics.net/\n\n\t-n <name> Name forwarded to the safechat server.\n\t\t\t(if the name includes spaces, use quotes)\n\t-s <serv> DNS name or IP address of a safechat server.\n\t-p <port> Port the safechat server is running on.\n\t\t\t(from 1 to 65535)\n\t-d <path> Downloads directory.\n\t\t\t(path format is system specific, use quotes)\n\t--help    Displays this message.\n";
                exit(EXIT_SUCCESS);
            } else {
                std::cerr << "Invalid argument (" << argv[i] << "). Enter \"safechat --help\" for details.\n";
                exit(EXIT_SUCCESS);
            }
        }
        if (_name.size() < 1) {
            std::cerr << "Local name required (-n). Enter \"safechat --help\" for details.\n";
            exit(EXIT_SUCCESS);
        }
        if (_server.size() < 1) {
            std::cerr << "Server required (-s). Enter \"safechat --help\" for details.\n";
            exit(EXIT_SUCCESS);
        }
        if (_port < 1 || _port > 65535) {
            std::cerr << "Invalid port number (-p). Enter \"safechat --help\" for details.\n";
            exit(EXIT_SUCCESS);
        }
        if (_download_path.size() < 1) {
            std::cerr << "Downloads directory required (-d). Enter \"safechat --help\" for details.\n";
            exit(EXIT_SUCCESS);
        }
        if (_download_path[_download_path.size() - 1] != '/') {
            _download_path += "/";
        }
    }

    ~Client() {
        close(_socket);
        pthread_mutex_unlock(&_mutex);
        pthread_mutex_destroy(&_mutex);
        pthread_attr_destroy(&_attr);
        std::ofstream config_stream(_config_path.c_str());
        if (!config_stream) {
            std::cerr << "Error writing configuration file to " << _config_path << ".\n";
        } else {
            config_stream << "Configuration file for SafeChat\n\nlocal_name=" << _name << "\nserver=" << _server << "\nport=" << _port << "\ndownload_path=" << _download_path;
            config_stream.close();
        }
    }

    void start_host() {

        long id;
        std::string choice;
        DH *dh;
        BIGNUM *public_key = BN_new();
        Block block(__null);

        std::cout << "\nConnecting to " << _server << " on port " << _port << "... " << std::flush;
        Socket();
        pthread_create(&_keep_alive, NULL, &Client::keep_alive, this);
        Send(Block(__set_name, _name));
        Send(Block(__set_available));
        std::cout << "Done" << std::flush;
        do {
            std::cout << "\nWaiting for client to connect to " << _name << "... " << std::flush;
            id = Recv(block).num();
            _peer_name = Recv(block).str();
            std::cout << "Done" << std::flush;
            do {
                std::cout << "\nAccept connection from " << _peer_name << "? (y/n) " << std::flush;
                std::getline(std::cin, choice);
                if (choice == "n" || choice == "n") {
                    Send(Block(__decline_client, id));
                } else if (choice == "y" || choice == "Y") {
                    Send(Block(__accept_client, id));
                    std::cout << "Generating session key... " << std::flush;
                    dh = DH_generate_parameters(__key_length, 5, NULL, NULL);
                    Send(Block(__send_data, BN_bn2hex(dh->p)));
                    DH_generate_key(dh);
                    Send(Block(__send_data, BN_bn2hex(dh->pub_key)));
                    BN_hex2bn(&public_key, (char *) Recv(block).c_str());
                    _key_size = DH_size(dh);

                    _key = new unsigned char[_key_size];

                    DH_compute_key(_key, public_key, dh);
                    initstate(1, (char *) _key, _key_size);
                    std::cout << "Done" << std::flush;
                    console();

                    delete [] _key;

                }
            } while (choice != "y" && choice != "Y" && choice != "n" && choice != "N");
        } while (choice != "y" && choice != "Y");
        pthread_kill(_keep_alive, SIGTERM);
    }

    void start_client() {

        int hosts_num, socket, choice, response;
        std::string string;
        std::vector< std::pair<int, std::string> > hosts;
        DH *dh;
        BIGNUM *public_key = BN_new();
        Block block(__null);

        std::cout << "\nConnecting to " << _server << " on port " << _port << "... " << std::flush;
        Socket();
        pthread_create(&_keep_alive, NULL, &Client::keep_alive, this);
        Send(Block(__set_name, _name));
        std::cout << "Done" << std::flush;
        do {
            Send(Block(__get_hosts));
            hosts_num = Recv(block).num();
            if (!hosts_num) {
                std::cout << "\n\nAvailable hosts:\n\n" << std::flush;
                std::cout << "\t(none)\n\n" << std::flush;
                return;
            } else {
                hosts.clear();
                for (int i = 0; i < hosts_num; i++) {
                    socket = Recv(block).num();
                    hosts.push_back(std::make_pair(socket, Recv(block).str()));
                }
                do {
                    std::cout << "\n\nAvailable hosts:\n\n" << std::flush;
                    for (int i = 0; i < hosts_num; i++) {
                        std::cout << "\t" << i + 1 << ") " << hosts[i].second << "\n" << std::flush;
                    }
                    std::cout << "\nChoice: ";
                    std::getline(std::cin, string);
                    choice = atoi(string.c_str());
                } while (choice < 1 || choice > hosts_num);
                _peer_name = hosts[choice - 1].second;
                Send(Block(__try_host, hosts[choice - 1].first));
                std::cout << "\nWaiting for " << _peer_name << " to accept your connection... " << std::flush;
                response = Recv(block).cmd();
                if (response == __decline_client) {
                    std::cout << _peer_name << " declined your connection." << std::flush;
                } else if (response == __accept_client) {
                    std::cout << "Done\nGenerating session key... " << std::flush;
                    dh = DH_generate_parameters(__key_length, 5, NULL, NULL);
                    BN_hex2bn(&dh->p, (char *) Recv(block).c_str());
                    DH_generate_key(dh);
                    BN_hex2bn(&public_key, (char *) Recv(block).c_str());
                    Send(Block(__send_data, BN_bn2hex(dh->pub_key)));
                    _key_size = DH_size(dh);

                    _key = new unsigned char[_key_size];

                    DH_compute_key(_key, public_key, dh);
                    initstate(1, (char *) _key, _key_size);
                    std::cout << "Done" << std::flush;
                    console();

                    delete [] _key;

                }
            }
        } while (response != __accept_client);
        pthread_kill(_keep_alive, SIGTERM);
    }

    static void *keep_alive(void *client) {
        return ((Client *) client)->keep_alive();
    }

    static void *inbound(void *client) {
        return ((Client *) client)->inbound();
    }

    static void *outbound(void *client) {
        return ((Client *) client)->outbound();
    }

    static void exit(int signal) {
        pthread_exit(NULL);
    }

private:

    void Socket() {

        sockaddr_in address;
        hostent *host;

        _socket = socket(AF_INET, SOCK_STREAM, 0);
        host = gethostbyname(_server.c_str());
        if (!host) {
            std::cerr << "Error resolving " << _server << ".\n";
            raise(SIGINT);
        }
        address.sin_family = AF_INET;
        memcpy(&address.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
        address.sin_port = htons(_port);
        if (connect(_socket, (sockaddr *) & address, sizeof (address))) {
            std::cerr << "Error connecting to " << _server << ".\n";
            raise(SIGINT);
        }
    }

    void *keep_alive() {
        signal(SIGTERM, exit);
        while (true) {
            sleep(__timeout / 3);
            Send(Block(__keep_alive));
        }
        return NULL;
    }

    void console() {
        std::cout << "\n\nSupported commands:\n\n\t/file - send file\n\t/quit - disconnect\n\n" << std::flush;
        pthread_create(&_outbound, &_attr, &Client::outbound, this);
        pthread_create(&_inbound, &_attr, &Client::inbound, this);
        pthread_join(_outbound, NULL);
        pthread_join(_inbound, NULL);
    }

    void *outbound() {

        long file_size, bytes_sent, rate;
        std::string file_name, file_path, string;
        FILE *file_stream;
        time_t start_time, last_update;
        Block block(__null);

        signal(SIGTERM, exit);
        while (true) {
            std::cout << _name << ": " << std::flush;
            std::getline(std::cin, string);
            pthread_mutex_lock(&_mutex);
            if (string == "/file") {
                std::cout << "Enter file path: " << std::flush;
                std::getline(std::cin, file_path);
                for (int i = 0; file_path[i] == ' ' || file_path[i] == '\'' || file_path[i] == '\"'; i++) {
                    file_path.erase(file_path.begin() + i);
                }
                for (int i = file_path.size() - 1; file_path[i] == ' ' || file_path[i] == '\'' || file_path[i] == '\"'; i--) {
                    file_path.erase(file_path.begin() + i);
                }
                for (size_t i = 0; i < file_path.size(); i++) {
                    if (file_path[i] == '\\') {
                        file_path.erase(file_path.begin() + i);
                    }
                }
                file_stream = fopen(file_path.c_str(), "rb");
                if (!file_stream) {
                    std::cerr << "Error opening " << file_path << ".\n";
                } else {
                    block = Block(__send_data, string);
                    Send(Encrypt(block));
                    file_name = file_path.substr(file_path.rfind("/") + 1);
                    block = Block(__send_data, file_name);
                    Send(Encrypt(block));
                    fseek(file_stream, 0, SEEK_END);
                    file_size = ftell(file_stream);
                    block = Block(__send_data, file_size);
                    Send(Encrypt(block));
                    fseek(file_stream, 0, SEEK_SET);
                    time(&start_time);
                    time(&last_update);
                    std::cout << "Sending " << file_name << "..." << std::flush;
                    do {
                        block = Block(__send_data);
                        bytes_sent = ftell(file_stream);
                        if (difftime(time(NULL), last_update) >= 1) {
                            rate = bytes_sent / difftime(time(NULL), start_time);
                            std::cout << "\r" << std::string(file_name.size() + 36, ' ') << "\rSending " << file_name << "... " << std::fixed << std::setprecision(0) << (float) bytes_sent / file_size * 100 << "% (" << format_time((file_size - bytes_sent) / rate) << " @ " << format_rate(rate) << ")" << std::flush;
                            time(&last_update);
                        }
                        if (file_size - ftell(file_stream) >= __block_size - 1) {
                            fread((void *) block.c_str(), 1, __block_size - 1, file_stream);
                        } else {
                            fread((void *) block.c_str(), 1, file_size - ftell(file_stream), file_stream);
                        }
                        Send(Encrypt(block));
                    } while (bytes_sent < file_size);
                    std::cout << "\r" << std::string(file_name.size() + 36, ' ') << "\rSending " << file_name << "... Done" << std::flush;
                    fclose(file_stream);
                    std::cout << "\n" << std::flush;
                }
            } else if (string == "/quit") {
                Send(Block(__null));
                std::cout << "\r" << std::string(_name.size() + 1, ' ') << "\nDisconnected.\n\n" << std::flush;
                pthread_kill(_inbound, SIGTERM);
                pthread_exit(NULL);
            } else if (string[0] == '/') {
                std::cerr << "Invalid command " << string << ".\n";
            } else {
                block = Block(__send_data, string);
                Send(Encrypt(block));
            }
            pthread_mutex_unlock(&_mutex);
        }
        return NULL;
    }

    void *inbound() {

        long file_size, bytes_received, rate;
        std::string file_name, file_path;
        FILE *file_stream;
        time_t start_time, last_update;
        Block block(__null);

        signal(SIGTERM, exit);
        while (true) {
            Encrypt(Recv(block));
            if (block.cmd() == __send_data && block.str() == "/file") {
                pthread_mutex_lock(&_mutex);
                file_name = Encrypt(Recv(block)).str();
                file_path = _download_path + file_name;
                file_stream = fopen(file_path.c_str(), "wb");
                if (!file_stream) {
                    std::cerr << "Error creating " << file_path << ".\n";
                    raise(SIGINT);
                } else {
                    file_size = Encrypt(Recv(block)).num();
                    time(&start_time);
                    time(&last_update);
                    std::cout << "\r" << std::string(_name.size() + 1, ' ') << "\rReceiving " << file_name << "..." << std::flush;
                    do {
                        bytes_received = ftell(file_stream);
                        if (difftime(time(NULL), last_update) >= 1) {
                            rate = bytes_received / difftime(time(NULL), start_time);
                            std::cout << "\r" << std::string(file_name.size() + 38, ' ') << "\rReceiving " << file_name << "... " << std::fixed << std::setprecision(0) << (float) bytes_received / file_size * 100 << "% (" << format_time((file_size - bytes_received) / rate) << " @ " << format_rate(rate) << ")" << std::flush;
                            time(&last_update);
                        }
                        Recv(block);
                        if (block.cmd() == __send_data) {
                            if (file_size - ftell(file_stream) >= __block_size - 1) {
                                fwrite((void *) Encrypt(block).c_str(), 1, __block_size - 1, file_stream);
                            } else {
                                fwrite((void *) Encrypt(block).c_str(), 1, file_size - ftell(file_stream), file_stream);
                            }
                        } else {
                            std::cout << "\n\n" << _peer_name << " disconnected.\n\n" << std::flush;
                            pthread_kill(_outbound, SIGTERM);
                            pthread_exit(NULL);
                        }
                    } while (bytes_received < file_size);
                    std::cout << "\r" << std::string(file_name.size() + 38, ' ') << "\rReceiving " << file_name << "... Done\n" << _name << ": " << std::flush;
                    fclose(file_stream);
                }
                pthread_mutex_unlock(&_mutex);
            } else if (block.cmd() == __send_data) {
                pthread_mutex_lock(&_mutex);
                std::cout << "\r" << std::string(_name.size() + 1, ' ') << "\r" << _peer_name << ": " << block.str() << "\n" << _name << ": " << std::flush;
                pthread_mutex_unlock(&_mutex);
            } else {
                std::cout << "\n\n" << _peer_name << " disconnected.\n\n" << std::flush;
                pthread_kill(_outbound, SIGTERM);
                pthread_exit(NULL);
            }
        }
        return NULL;
    }

    void Send(const Block &input) {
        if (!send(_socket, input._data, __block_size, 0)) {
            std::cerr << "Error communicating with " << _server << ".\n";
            raise(SIGINT);
        }
    }

    Block &Recv(Block &output) {
        if (!recv(_socket, (void *) output._data, __block_size, MSG_WAITALL)) {
            std::cerr << "Error communicating with " << _server << ".\n";
            raise(SIGINT);
        }
        return output;
    }

    Block &Encrypt(Block &block) {
        for (int i = 1; i < __block_size - 1; i++) {
            block._data[i] ^= random() % 255;
        }
        return block;
    }

    std::string format_rate(long rate) {

        std::string string;
        std::stringstream string_stream;

        if (rate / (1024 * 1024) >= 1) {
            string_stream << rate / (1024 * 1024);
            string += string_stream.str() + "MB/s";
        } else if (rate / 1024 >= 1) {
            string_stream << rate / 1024;
            string += string_stream.str() + "KB/s";
        } else {
            string_stream << rate;
            string += string_stream.str() + "B/s";
        }
        return string;
    }

    std::string format_time(long seconds) {

        std::string string;
        std::stringstream string_stream;

        if (seconds / (60 * 60) >= 1) {
            string_stream << seconds / (60 * 60);
            string += string_stream.str() + "hrs";
        } else if (seconds / 60 >= 1) {
            string_stream << seconds / 60;
            string += string_stream.str() + "min";
        } else {
            string_stream << seconds;
            string += string_stream.str() + "sec";
        }
        return string;
    }

};

Client *client;

void save_config_wrapper(int signal) {
    delete client;
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    int choice;
    std::string string;
    client = new Client(argc, argv);

    signal(SIGINT, save_config_wrapper);
    do {
        std::cout << "\nMain Menu\n\n\t1) Start new host\n\t2) Connect to host\n\nChoice: " << std::flush;
        std::getline(std::cin, string);
        choice = atoi(string.c_str());
        if (choice == 1) {
            client->start_host();
        } else if (choice == 2) {
            client->start_client();
        }
    } while (choice < 1 || choice > 2);
    delete client;
    return 0;
}
