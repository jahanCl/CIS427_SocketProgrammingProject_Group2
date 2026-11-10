#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "utilities.h"

using namespace std;

//Finds a hostname or IP address and connects to the server.
inline int connectToServer(const string& host, const string& port) {
    addrinfo hints{};                //Stores address search requirements.
    hints.ai_family = AF_UNSPEC;     //Allows IPv4 or IPv6.
    hints.ai_socktype = SOCK_STREAM; //Requests a TCP stream socket.
    hints.ai_protocol = IPPROTO_TCP; //Explicitly requests TCP.

    addrinfo* results = nullptr;     //Will point to resolved addresses.

    //getaddrinfo(host or IP, port, address requirements, list of addresses).
    int status = getaddrinfo(host.c_str(), port.c_str(),&hints,&results);

    //If Hostname lookup failed.
    if (status != 0) {               
        cerr << "Address lookup failed: "<< gai_strerror(status)<< '\n';
        return -1;        //-1 represents no connected socket.
    }
    int connectedSocket = -1;       //No connection has succeeded yet.

        //Start at the first resolved address, continue while addresses remain, move to next address.
    for (addrinfo* current = results; current != nullptr; current = current->ai_next) {

        //scocket(IPv4 or IPv6, TCP stream socket, TCP protocol).
        int candidate = socket( current->ai_family, current->ai_socktype, current->ai_protocol); 

        //If socket creation failed for this address, try the next address.
        if (candidate < 0) {        
            continue;        
        }

        //If the connection attempt succeeds, save the socket and stop trying.
        if (connect(candidate, current->ai_addr, current->ai_addrlen) == 0) {
            connectedSocket = candidate; //Save  successful socket.
            break;                       //Stop trying addresses.
        }
        close(candidate);      //Release the failed candidate socket.
    }
    freeaddrinfo(results);     //Release memory from getaddrinfo()

    return connectedSocket;     //Return connected socket or -1.
}

//Adds the required newline delimiter and sends one protocol line.
inline bool sendLine(int socketNumber,const string& line) {
    string framedLine = line + "\n";

    if (!isASCii(framedLine)) {      //Checks if the framed line contains only ASCII characters.
        cout << "Only ASCII input is allowed.\n";
        return false;
    }

    if (framedLine.size() > MAX_LINE) {    // Checks if the framed line exceeds the maximum allowed bytes.
        cout << "Message exceeds 4,096 bytes.\n";
        return false;
    }
    return sendAll(socketNumber, framedLine);  //Sends the complete framed line using sendAll().
}

// Receives exactly one newline-delimited protocol line
///optinal becasue could be no line to return if the connection is closed or an error occurs.
inline optional<string> receiveLine(int socketNumber,string& receiveBuffer) {
    //Search buffered data for a complete line.
    while (true) {
        size_t newline = receiveBuffer.find('\n');  //Search buffered data for a complete line.

        // A complete line is available.
        if (newline != string::npos) {
            size_t framedSize =
                newline + 1;           //Include the newline in the size check.

            if (framedSize > MAX_LINE) {
                cerr << "413 Content too Large.\n";
                return nullopt;        //No valid line can be returned.
            }

            string line = receiveBuffer.substr(0,newline);  //Extract the line without the newline.
            receiveBuffer.erase(0,framedSize);              //Removes only the completed line.
        
            if (!isASCii(line)) {      //Validate server response .
                cerr << "415 non-ASCII content.\n";
                return nullopt;
            }
            return line;       //Return one complete protocol line.
        }
        //If no newline arrived before the limit, check if the buffer is already too large.
        if (receiveBuffer.size() >= MAX_LINE) {             
            cerr << "413 Content too Large.\n";
            return nullopt;
        }

        char buffer[RECEIVE_CHUNK];     //Temporary storage for newly received bytes.
        //bytesRecieved = recv(socketNumber, buffer, sizeof(buffer), 0);
        ssize_t bytesReceived = recv(socketNumber,buffer,sizeof(buffer),0);

        //Got nothing from the server, which prob means the connection was closed.
        if (bytesReceived == 0) {
            cerr << "The server disconnected.\n";
            return nullopt;
        }
        //If recv() failed, check if it was interrupted by a signal and retry if so.
        if (bytesReceived < 0) {        
            if (errno == EINTR) {       
                continue;             
            }
            cerr << "Receive failed: "<< strerror(errno)<< '\n';
            return nullopt;
        } 
        //Add the bytes received.
        receiveBuffer.append(buffer,static_cast<size_t>(bytesReceived));
    }
}

#endif