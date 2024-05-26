#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <errno.h>

bool send_to_socket_and_receive_response(const std::string &request, const char *path);

std::string formatChunk(const std::string &str);

void sendNetconfRequest(std::string req, int sockfd);

int main(int argc, char **argv) {

    std::cout << "my prog: " << argv[0] << std::endl;
    std::cout << "args is " << argc << std::endl;

    for (auto i = 0; i < argc; i++) {
        std::cout << "argv[" << i << "]" << argv[i] << std::endl;
    }
    std::string request = R"(
    <rpc xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="42" username="5d20a83c25c052460be5f1668f695f09">
    <get-config><source><candidate/></source></get-config>
    </rpc>)";

    std::cout << "request:\n" << request << std::endl;

    send_to_socket_and_receive_response(request, argv[1]);

    return 0;
}

bool send_to_socket_and_receive_response(const std::string &request, const char *path) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return -1;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        close(sockfd);
        std::cerr << "Failed to get socket flags: " << strerror(errno) << std::endl;
        return -1;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(sockfd);
        std::cerr << "Failed to set socket to non-blocking mode: " << strerror(errno) << std::endl;
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
        std::perror("Failed to connect to the Unix domain socket");
        return false;
    }

    sendNetconfRequest(request, sockfd);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    std::cout << "Received message from the server:" << std::endl;
    ssize_t count = 0;
    uint8_t repeat = 0;
    ssize_t bytes_read = 0;
    while (true) {
        bytes_read = read(sockfd, buffer, sizeof(buffer)-1);
        buffer[bytes_read] = '\0';
        count += bytes_read;
        if (bytes_read > 0) {
            std::cout << buffer;
        } else if (bytes_read == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                repeat++;
                sleep(1);
                if (repeat >= 3) {
                    break;
                }else
                    continue;
            }
            perror("recv");
            break;
        }
    }

    std::cout << std::endl;
    close(sockfd);

    return true;
}

/* так надо по rfc */
std::string formatChunk(const std::string &str) {
    return "\n#" + std::to_string(str.length()) + "\n" + str + "\n##\n";
}

void sendNetconfRequest(std::string req, int sockfd) {
    req = formatChunk(req);
    std::cout << "request after format:\n" << req << std::endl;
    if (write(sockfd, req.c_str(), req.length()) < 0) {
        perror("write");
    }
}