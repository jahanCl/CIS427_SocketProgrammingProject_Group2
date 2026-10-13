#include <stdio.h>
#include <iostream>
#include <strings.h>
//#include <unistd.h>
#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <netdb.h>
#include <cstdlib>
#include <cstring>


using namespace std;

#define MAX_LINE 4096           //Total bytes allowed in a line
#define RECEIVE_CHUNK 1024      //Bytes to receive at a time

// ============================================================
// Input Validator
// ============================================================

// Returns true if every character in a string is an ASCII character.
bool isASCii(const string& text){     
     if (unsigned char character : text) {   //loop through evrey byte in string 
            if (character > 127 || character < 0) {   //if the byte is not in the ASCII range
                return false;   //return false
            }
        }
    return true;
}

//Validates a user provided field, like username, title, question body, or answer.
bool validField(const string& text){}

//Reads and validates when a positive integer is input into the terminal.
bool readPositiveNumber(){}


// ============================================================
// Client Socket Handler
// ============================================================

//Sends all bytes in data.
bool sendAll(int socketNumber, const string& data){}

// Connects to the server using a hostname/IP address and port.
int connectToServer(){}



// ============================================================
// Receive Buffer and Message Framing
// ============================================================
//Sends one protocol line.
bool sendLine(int socketNumber, const string& line) {}
            
//Reads exactly one newline ending protocol line from the server.
bool receiveLine(){}


// ============================================================
// Protocol Formatter and Parser
// ============================================================

//Receives and displays one complete server response.
bool receiveResponse(){}





  

// ============================================================
// Menu-Driven Command-Line Interface and Client State
// ============================================================

int main(int argc, char * argv[]) {


}





