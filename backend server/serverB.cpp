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


typedef std::vector<std::vector<bool>> AdjacencyMatrix;

//////// Contants ////////
// Last 3 digits of school ID
static const int SCHOOL_ID = 691;
// Server port
static const int UDP_PORT = 31000 + SCHOOL_ID;
// Server hostname
static const char* HOSTNAME = "127.0.0.1";
// Max buffer size for read/write via socket
static const int BUFF_SIZE = 256;
// Result is None
static const int NONE = -1;
// Max possible users per country
static const int MAX_ORDER = 100;
// Local storage (text file database)
static const char* DB_FILENAME = "data2.txt";
static const char* SERVER_NAME = "B";
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

struct Graph {
    // Adjacency matrix
    AdjacencyMatrix matrix;
    // Reindexation for user id
    std::map<int32_t, int32_t> userIDMapping;
    // Number of unique users
    int32_t usersNum;


    Graph(int order=MAX_ORDER) {
        setOrder(order);
        usersNum = 0;
    }

    void addUser(int32_t ID) {
        if(getUserIndex(ID) == -1) userIDMapping[ID] = usersNum++;
    }

    void connectUsers(int32_t userID, int32_t friendID) {
        // Ensure users reindexation exists
        addUser(userID); 
        addUser(friendID);

        int32_t userIndex = getUserIndex(userID);
        int32_t friendIndex = getUserIndex(friendID);

        matrix[userIndex][friendIndex] = matrix[friendIndex][userIndex] = true;  
    }

    std::vector<int32_t> getUsers() {
        std::vector<int32_t> users;

        std::map<int32_t, int32_t>::iterator it;
        for(it = userIDMapping.begin(); it != userIDMapping.end(); ++it) {
            users.push_back(it->first);
        }

        return users;
    }

    std::vector<int32_t> getConnections(int32_t userID) {
        std::vector<int32_t> connections;

        int32_t userIndex = getUserIndex(userID);
        if(userIndex == -1) return connections;

        for(size_t friendIndex = 0; friendIndex < userIDMapping.size(); ++friendIndex) {
            if((size_t)userIndex != friendIndex && matrix[userIndex][friendIndex] == true) {
                // Reverse find, userID by userIndex
                int friendID = getUserID(friendIndex);
                connections.push_back(friendID);
            }
        }

        return connections;
    }

    // Returns list of unconnected to 'userID' user
    std::vector<int32_t> getUnconnected(int32_t userID) {
        std::vector<int32_t> connections;

        int32_t userIndex = getUserIndex(userID);
        if(userIndex == -1) return connections;

        for(size_t friendIndex = 0; friendIndex < userIDMapping.size(); ++friendIndex) {
            // Collect non friend id's
            if((size_t)userIndex != friendIndex && matrix[userIndex][friendIndex] == false) {
                // Reverse find, userID by userIndex
                int32_t friendID = getUserID(friendIndex);
                connections.push_back(friendID);
            }
        }

        return connections;
    }


    bool isUserExists(int32_t userID) { return getUserIndex(userID) != -1; }

    bool isConnected(int32_t userID1, int32_t userID2) {
        int32_t userIndex1 = getUserIndex(userID1);
        int32_t userIndex2 = getUserIndex(userID2);

        return matrix[userIndex1][userIndex2];
    }

    int32_t getTotalEdges() {
        int32_t edges = 0;
        for(size_t i = 0; i < userIDMapping.size(); ++i) {
            for(size_t j = i + 1; j < userIDMapping.size(); ++j) {
                edges += (matrix[i][j]);
            }
        }
        return edges;
    }

private:
    int32_t getUserID(int32_t userIndex) {
        std::map<int32_t, int32_t>::iterator it;
        for(it = userIDMapping.begin(); it != userIDMapping.end(); ++it) {
            if(it->second == userIndex) return it->first;
        }
        return -1;
    }

    int32_t getUserIndex(int32_t userID) {
        if(userIDMapping.find(userID) == userIDMapping.end()) return -1;
        return userIDMapping[userID];
    }

    void setOrder(int order) {
        matrix.resize(order);
        for(int i = 0; i < order; ++i) matrix[i].resize(order);
    }
};




int32_t getRecommendation(int32_t userID, Graph& graph) {
    std::vector<int32_t> users   = graph.getUsers();
    std::vector<int32_t> friends = graph.getConnections(userID);

    // 1. u already connected to all the other users in country
    if(users.size() == friends.size() + 1) return NONE;

    // 2. 
    std::vector<int32_t> unconnected = graph.getUnconnected(userID);
    std::map<int32_t, int32_t> common;
    std::map<int32_t, int32_t> degrees;
    bool hasCommon = false;
    int32_t maxCommonPos = NONE, maxDegreePos = NONE;
    int32_t maxCommonValue = 0, maxDegreeValue = 0;
    for(size_t i = 0; i < unconnected.size(); ++i) {
        // count the number of common neighbors between 'u' and 'n'
        int32_t n = unconnected[i];

        int32_t count = 0;
        std::vector<int32_t> neighbors = graph.getConnections(n);
        for(size_t j = 0; j < neighbors.size(); ++j) {
            count += graph.isConnected(userID, (int32_t)neighbors[j]);
        }        
        degrees[n] = neighbors.size();
        common[n] = count;
        if(count > 0) hasCommon = true;

        if(maxCommonValue < count) {
            maxCommonValue = count;
            maxCommonPos = i;
        }

        if(maxDegreePos == NONE || maxDegreeValue < (int32_t)neighbors.size()) {
            maxDegreeValue = neighbors.size();
            maxDegreePos = i;
        }
    }

    // 2. a. noone shares connection with user
    if(!hasCommon) {
        int32_t resultID = unconnected[maxDegreePos];
        // Check if we have tie for highest degree value
        for(size_t i = 0; i < unconnected.size(); ++i) { 
            // smallest ID
            if(degrees[unconnected[i]] == maxDegreeValue 
                && unconnected[i] < resultID) resultID = unconnected[i];
        }
        return resultID;
    }

    if(maxCommonPos == NONE) return NONE;

    // 2. b. 
    int32_t resultID = unconnected[maxCommonPos];
    // Check if we have tie for highest degree value
    for(size_t i = 0; i < unconnected.size(); ++i) { 
        // smallest ID
        if(common[unconnected[i]] == maxCommonValue 
            && unconnected[i] < resultID) resultID = unconnected[i];
    }
    return resultID;


}

void serverStart(std::map<std::string, Graph>& db) {
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
    servaddr.sin_port 		= htons(UDP_PORT); 
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 )  { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    std::cout << "The server " << SERVER_NAME << " is up and running using UDP on port " << UDP_PORT << std::endl;
    len = sizeof(cliaddr);  
    while(1) {
        int32_t resultID = NONE, userID = NONE;
        std::string country;
        packet_t packet;

        // Read request
        memset(&packet, 0, sizeof(packet_t));
        recvfrom(sockfd, (char *)&packet, sizeof(packet_t), MSG_WAITALL, ( struct sockaddr *) &cliaddr, (socklen_t*)&len);

        if(packet.type == REQUEST_COUNTRY_LIST) {
            memset(&packet.raw, 0, sizeof(char) * BUFF_SIZE);

            std::map<std::string, Graph>::iterator it;
            for(it = db.begin(); it != db.end(); ++it) {
                strcat(packet.raw, it->first.c_str());
                strcat(packet.raw, " ");
            }

            std::cout << "The server " << SERVER_NAME << " has sent a country list to Main Server" << std::endl;
            
            
        } else if(packet.type == REQUEST_RECOMENDATION) {
            std::string query(packet.raw);
            memset(&packet.raw, 0, sizeof(char) * BUFF_SIZE);
            std::istringstream iss(query);
            if(iss >> userID >> country) {            
                std::cout << "The server " << SERVER_NAME << " has received request for finding possible ";
                std::cout << "friends of User" << userID << " in " << country << std::endl;

                if(db.find(country) != db.end()) {
                    if(db[country].isUserExists(userID) == false) { 
                        std::cout << "User" << userID << " does not show up in " << country << std::endl;
                        std::cout << "The server " << SERVER_NAME;
                        std::cout << " has sent \"User" << userID << " not found\" to Main Server" << std::endl;
                        packet.type = NONE;
                        sprintf(packet.raw, "User%d not found", userID);
                    } else {
                        std::cout << "The server " << SERVER_NAME;
                        std::cout << " is searching possible friends for User" << userID << "..." << std::endl;
                        resultID = getRecommendation(userID, db[country]);
                        std::cout << "Here are the results: ";
                        if(resultID == -1) {
                            std::cout << "None" << std::endl;

                        } else {
                            std::cout << "User" << resultID << std::endl;
                        }
                        sprintf(packet.raw, "%d", resultID);
                    }
                }

                std::cout << "The server " << SERVER_NAME << " has sent the result(s) to Main Server" << std::endl;
            } 
        }


        sendto(sockfd, (const char *)&packet, sizeof(packet), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);

    }
}

int main(int argc, char** argv) {
    // Adjacency matrix for each country
    std::map<std::string, Graph> db;

    std::string line, country;

    // Read file
    std::ifstream input(DB_FILENAME);
    while(std::getline(input, line)) {
        if(!line.empty() && isalpha(line[0])) {
            // Create new graph
            country = line;
            db.insert({country, Graph()});
        } else if(!country.empty()) {
            std::istringstream iss(line);
            int userID, friendID;
            
            if(iss >> userID)
                db[country].addUser(userID);
            while(iss >> friendID) 
                db[country].connectUsers(userID, friendID);
        }
    }
    input.close();


    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << std::setw(30) << "country name" << std::setw(15) << "num users" 
              << std::setw(15) << "num relations" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;
    for(std::map<std::string, Graph>::iterator it = db.begin(); it != db.end(); ++it) {
        std::vector<int32_t> users = it->second.getUsers();
        std::cout << std::setw(30) << it->first
            << std::setw(15) << users.size()
            << std::setw(15) << it->second.getTotalEdges()
            << std::endl;        
    }
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << "num graphs" << std::setw(20) << db.size() << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    // Start server and wait for incomming requests
    serverStart(db);

    return EXIT_SUCCESS;
}
