#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>


//////// Contants ////////
// Last 3 digits of school ID
static const int SCHOOL_ID = 691;
// Main Server port(s)
static const int UDP_PORT = 32000 + SCHOOL_ID;
static const int TCP_PORT = 33000 + SCHOOL_ID;
// BackEnd port(s)
static const int UDP_SERVER_A = 30000 + SCHOOL_ID;
static const int UDP_SERVER_B = 31000 + SCHOOL_ID;
// Server hostname
static const char* HOSTNAME = "127.0.0.1";
// Max buffer size for read/write via socket
static const int BUFF_SIZE = 256;
// Result is None
static const int NONE = -1;
// Packet type constants
static const int REQUEST_COUNTRY_LIST = 1;
static const int REQUEST_RECOMENDATION = 2;
// ID's for each backend server
static const int BACKEND_SERVER_A_ID = 0;
static const int BACKEND_SERVER_B_ID = 1;

#pragma pack(push,1)
typedef struct {
    int type; // 1 - request for list of countries from main server
              // 2 - request to find recomendations
    char raw[BUFF_SIZE];
} packet_t;
#pragma pack(pop)



packet_t queryBackend(const char* hostname, int port, packet_t packet) {
    int sockfd, len;
    struct sockaddr_in servaddr, cliaddr; 
      
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family		= AF_INET; // IPv4 
    servaddr.sin_addr.s_addr 	= inet_addr(HOSTNAME);
    servaddr.sin_port 		= htons(UDP_PORT); // Main Server port
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 )  { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    len = sizeof(cliaddr);  

    // Send request
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family        = AF_INET;
    cliaddr.sin_addr.s_addr   = inet_addr(hostname);
    cliaddr.sin_port          = htons(port); // Remote server port

    // Send request
    sendto(sockfd, (const char *)&packet, sizeof(packet_t), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
    // Clear packet
    memset(&packet, 0, sizeof(packet_t));
    // Read response
    recvfrom(sockfd, (char *)&packet, sizeof(packet_t), MSG_WAITALL, ( struct sockaddr *) &cliaddr, (socklen_t*)&len);
    // Cleanup
    close(sockfd);

    return packet;    
}

void getCountryList(std::map<std::string, int>& country_backend_mapping, const char* hostname, int port, int serverID) {
    packet_t packet;
    memset(&packet, 0, sizeof(packet_t));
    packet.type = REQUEST_COUNTRY_LIST;
    packet = queryBackend(hostname, port, packet);
    if(packet.type == REQUEST_COUNTRY_LIST) {
        std::string response(packet.raw);
        std::istringstream iss(response);
        std::string country;

        while(iss >> country) {
            country_backend_mapping[country] = serverID;
        }
    }
}

void handleClient(std::map<std::string, int> country_backend_mapping, int sockfd, struct sockaddr_in& cliaddr, int clilen) {
    packet_t packet;
    memset(&packet, 0, sizeof(packet_t));

    if(recv(sockfd, (char*)&packet, sizeof(packet_t) , 0) < 0) {
        perror("recv fail");
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    std::istringstream iss(std::string(packet.raw));

    int32_t clientID, userID;
    std::string country;

    iss >> clientID >> userID >> country;

    std::cout << "The Main server has received the request on User " << userID;
    std::cout << " in " << country << " from client" << clientID << " using TCP";
    std::cout << " over port " << TCP_PORT << std::endl;

    if(country_backend_mapping.find(country) == country_backend_mapping.end()) {
        std::cout << country << " does not show up in server A&B" << std::endl;

        // Send response
        memset(&packet, 0, sizeof(packet_t));
        packet.type = NONE;
        sprintf(packet.raw, "%s", "Country Name: Not found");
        send(sockfd, (char*)&packet, sizeof(packet_t), 0);

        std::cout << "The Main Server has sent \"Country Name: Not found\" to ";
        std::cout << "client" << clientID << " using TCP over port " << TCP_PORT << std::endl;
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return;
    }

    // Pick backend server
    int32_t backEndServer = country_backend_mapping[country];

    memset(&packet, 0, sizeof(packet_t));
    packet.type = REQUEST_RECOMENDATION;
    sprintf(packet.raw, "%d %s\n", userID,  country.c_str());

    // Send query to the backend
    if(backEndServer == BACKEND_SERVER_A_ID) {
        std::cout << country << " shows up in server A" << std::endl;
        std::cout << "The Main Server has sent request from User " << userID;
        std::cout << " to server A using UDP over port " << UDP_SERVER_A  << std::endl;
        packet = queryBackend(HOSTNAME, UDP_SERVER_A, packet);
    } else if(backEndServer == BACKEND_SERVER_B_ID) {
        std::cout << country << " shows up in server B" << std::endl;
        std::cout << "The Main Server has sent request from User " << userID;
        std::cout << " to server B using UDP over port " << UDP_SERVER_B << std::endl;
        packet = queryBackend(HOSTNAME, UDP_SERVER_B, packet);
    }

    if(packet.type == NONE) {
        std::cout << "The Main server has received \"User ID: Not found\" from server ";
        std::cout << (backEndServer == BACKEND_SERVER_A_ID ? "A" : "B") << std::endl;

        std::cout << "The Main Server has sent error to client using TCP over " << TCP_PORT << std::endl;
        // Send response
        send(sockfd, (char*)&packet, sizeof(packet_t), 0);    

        // Cleanup
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return;
    }

    std::cout << "The Main Server has sent searching result(s) to client ";
    std::cout << " using TCP over port " << TCP_PORT << std::endl;
  
    // Send response
    send(sockfd, (char*)&packet, sizeof(packet_t), 0);    

    // Cleanup
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

int main(int argc, char** argv) {
    // Which server responses for which country name
    std::map<std::string, int> country_backend_mapping;
    
    // Bootup...
    //// TCP Server starts
    struct sockaddr_in servaddr, cliaddr;
    int sockfd, connfd;
    pid_t pid;

    // Creating socket file descriptor 
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family		= AF_INET; // IPv4 
    servaddr.sin_addr.s_addr 	= inet_addr(HOSTNAME);
    servaddr.sin_port 		= htons(TCP_PORT); // Main Server port
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  sizeof(servaddr)) < 0 )  { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }

    if(listen(sockfd, 0) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "The Main server is up and running. " << std::endl;
    // Request backend server A
    getCountryList(country_backend_mapping, HOSTNAME, UDP_SERVER_A, BACKEND_SERVER_A_ID);
    std::cout << "The Main server has received the country list from server ";
    std::cout << "A using UDP over port " << UDP_PORT << std::endl;

    // Request backend server B    
    getCountryList(country_backend_mapping, HOSTNAME, UDP_SERVER_B, BACKEND_SERVER_B_ID);
    std::cout << "The Main server has received the country list from server ";
    std::cout << "B using UDP over port " << UDP_PORT << std::endl;

    // Show country list(s)
    std::vector<std::string> serverListA, serverListB;
    std::map<std::string, int>::iterator it;
    for(it = country_backend_mapping.begin(); it != country_backend_mapping.end(); ++it) {
        if(it->second == BACKEND_SERVER_A_ID) serverListA.push_back(it->first);
        if(it->second == BACKEND_SERVER_B_ID) serverListB.push_back(it->first);
    }
    
    size_t maxSize = std::max(serverListA.size(), serverListB.size());
    std::cout << std::setw(18) << std::left << "Server A" << " | " << std::setw(18) << std::left << "Server B" << std::endl;
    for(size_t i = 0; i < maxSize; ++i) {
        std::cout << std::setw(18) << std::left << (i < serverListA.size() ? serverListA[i] : " ") ;
        std::cout << " | ";
        std::cout << std::setw(18) << std::left << (i < serverListB.size() ? serverListB[i] : " ") ;
        std::cout << std::endl;
    }

    ///////////////// Waiting for client(s) request(s) /////////////////
    while(1) {
        int clilen = sizeof(cliaddr);
        connfd = accept(sockfd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);

        // fork, handle client
        if( (pid = fork()) < 0) {
            perror("fork() failure");
            exit(EXIT_FAILURE);
        } else if(pid == 0) { // Child process
            // Close parent socket
            close(sockfd);
            // Handle client request
            handleClient(country_backend_mapping, connfd, cliaddr, clilen);
            exit(EXIT_SUCCESS);
        }
        // Parent close new sock
        close(connfd);
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
