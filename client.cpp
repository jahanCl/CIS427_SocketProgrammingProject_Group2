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
bool parsePositiveInteger(){}



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
bool sendProtocolMessage(int socketNumber, const string& line) {}
            
//Return one complete protocol line without its newline.
optional<string> receiveLine(){}   //string may or may not exist 


// ============================================================
// Protocol Formatter and Parser
// ============================================================

//Receives and displays one complete server response.
bool receiveResponse(){}





  

// ============================================================
// Menu-Driven Command-Line Interface and Client State
// ============================================================
void displayConnectedMenu() {
    cout << "CLASS DISCUSSION BOARD\n"
         << "1. Login\n"
         << "2. Quit\n"
         << "Selection:\n";
}

void displayAuthenticatedMenu() {
    cout << "CLASS DISCUSSION BOARD\n"
         << "1. View questions\n"
         << "2. View question and answers\n"
         << "3. Post a question\n"
         << "4. Post an answer\n"
         << "5. Logout\n"
         << "6. Quit\n"
         << "Selection: \n";
}




// command line arguments,  ./client localhost 5050  = 3
int main(int argc, char * argv[]) {
    //TO DO: Implement the main function to handle the client logic as described in the comments below.
    /*Check arguments
      
    Connect to server
      
    Initialize client state
      
    Display appropriate menu
      
    Read and validate user choice
      
    Send request
      
    Receive and display response
      
    Update login state
      
    Repeat until QUIT
      
    Close socket and exit*/
}





