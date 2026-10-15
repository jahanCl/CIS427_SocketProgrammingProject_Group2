#include <cstdlib>
#include <cstring>
#include <iostream>

//#include <strings.h>
//#include <sys/socket.h>
#include <sys/types.h>

#include <stdio.h>

//#include <arpa/inet.h>
//#include <netdb.h>
//#include <netinet/in.h>
//#include <unistd.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define bzero(b, len) memset((b), 0, (len))
#define close(s) closesocket(s)
#define bcopy(src, dest, len) memmove((dest), (src), (len))

using namespace std;

#define SERVER_PORT 6543
#define MAX_LINE 4096           //Total bytes allowed in a line
#define RECEIVE_CHUNK 1024      //Bytes to receive at a time


// ============================================================
// Input Validator
// ============================================================

// Returns true if every character in a string is an ASCII character.
bool isASCii(const string& text){     
     for (unsigned char character : text) {   //loop through evrey byte in string 
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
string receiveLine(){}   //string may or may not exist 


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
    FILE *fp;
    struct hostent *hp;
    struct sockaddr_in sin;
    char *host;
    char buf[MAX_LINE];
    int s;
    int len;

    if(argc == 2){
        host=argv[1];
    }
    else{
        fprintf(stderr, "usage: simplex-talk host\n");
        exit(1);
    }
    hp= gethostbyname(host);
    if(!hp){
        fprintf(stderr, "simplex-talk: unknown host: %s\n", host);
        exit(1);
    }
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family=AF_INET;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port=htons(SERVER_PORT);
    if((s=socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("simplex-talk: socket");
        exit(1);
    }
    if(connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        perror("simplex-talk: connect");
        close(s);
        exit(1);
    }
    while(fgets(buf, sizeof(buf), stdin)){
        buf[MAX_LINE-1]='\0';
        len=strlen(buf) + 1;
        send(s, buf, len, 0);
    }
}





