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
// Server port
static const int SERVER_PORT = 33000 + SCHOOL_ID;
// Server hostname
static const char* HOSTNAME = "127.0.0.1";
// Max buffer size for read/write via socket
static const int BUFF_SIZE = 256;
// Result is None
static const int NONE = -1;
// Client id
static int CLIENT_ID = 1;
// Packet type constants
static const int REQUEST_COUNTRY_LIST = 1;
static const int REQUEST_RECOMENDATION = 2;

#pragma pack(push,1)
typedef struct {
    int type; // 1 - request for list of countries from main server
              // 2 - request to find recomendations
    char raw[BUFF_SIZE];
} packet_t;
#pragma pack(pop)

int main(int argc, char** argv) {
    struct sockaddr_in servaddr, cliaddr;
    int sockfd;
    packet_t packet;

    
    std::cout << "Enter client ID: ";
    std::string line;
    std::getline(std::cin, line);
    CLIENT_ID = atoll(line.c_str());

    std::cout << "Client" << CLIENT_ID << " is up and running" << std::endl;

    do {
        std::string userIDs, country;
        int32_t userID = NONE;

        std::cout << "Please enter the User ID: ";
        if(!std::getline(std::cin, userIDs)) break;
        std::cout << "Please enter the Country Name: ";
        if(!std::getline(std::cin, country)) break;
        userID = atoll(userIDs.c_str());


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
        servaddr.sin_port 		= htons(SERVER_PORT); // Main Server port

        if (connect(sockfd,(struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
            perror("socket connect failed");
            exit(EXIT_FAILURE);
        }

        // Create packet
        memset(&packet, 0, sizeof(packet_t));
        sprintf(packet.raw, "%d %d %s", CLIENT_ID, userID, country.c_str());

        // Send request
        if(send(sockfd, (char*)&packet, sizeof(packet_t), 0) < 0) {
            perror("send fail");
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        std::cout << "Client" << CLIENT_ID << " has sent User" << userID << " and " << country;
        std::cout << " to Main Server using TCP" << std::endl;

        memset(&packet, 0, sizeof(packet_t));

        if(recv(sockfd, (char*)&packet, sizeof(packet_t) , 0) < 0) {
            perror("recv fail");
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        if(packet.type == NONE) {
            std::cout << std::string(packet.raw) << std::endl;
        } else {
            std::cout << "Client" << CLIENT_ID << " has received results from Main Server: " << std::endl;
            std::istringstream iss(std::string(packet.raw));
            int32_t friendID;

            if(iss >> friendID) {
                if(friendID == NONE) {
                    std::cout << "User" << userID;
                    std::cout << " is already connected to all other users, no new recommendation" << std::endl;
                } else {
                    std::cout << "User" << friendID << " is/are possible friend(s) of User";
                    std::cout << userID << " in " << country << std::endl;
                }
            }
        }

    } while(1);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return 0;
}
