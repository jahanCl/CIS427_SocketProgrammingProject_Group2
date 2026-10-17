#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>

//#include <strings.h>
//#include <sys/socket.h>
//#include <sys/types.h>

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
bool validField(const string& text){
    char divider= '|';
    bool valid=false;
    size_t firstDivider= text.find(divider);
    if(firstDivider != string::npos){  //if the divider is found in the string
        string command= text.substr(0, firstDivider);  //get the command
        if(command == "LOGIN"){
            regex pattern(R"(^LOGIN\|username=([^|]+)\|password=([^\n]+)\n$)");
            valid= regex_match(text, pattern);
        }
        else if(command == "LOGOUT"){
            regex pattern(R"(^LOGOUT\n$)");
            valid= regex_match(text, pattern);
        }
        else if(command == "VIEW_QUESTIONS"){
            regex pattern(R"(^VIEW_QUESTIONS\n$)");
            valid= regex_match(text, pattern);
        }
        else if(command == "VIEW_ANSWER"){
            regex pattern(R"(^VIEW_ANSWER\|questionNumber=([^|]+)\n$)");
            valid= regex_match(text, pattern);
        }
        else if(command == "POST_QUESTION"){
            regex pattern(R"(^POST_QUESTION\|title=([^|]+)\|body=([^\n]+)\n$)");
            valid= regex_match(text, pattern);
        }
        else if(command == "POST_ANSWER"){
            regex pattern(R"(^POST_ANSWER\|questionNumber=([^|]+)\|answer=([^\n]+)\n$)");
            valid= regex_match(text, pattern);
        }
        else if(command == "QUIT"){
            regex pattern(R"(^QUIT\n$)");
            valid= regex_match(text, pattern);
        }
    }
    else
    {
        valid= false;  //if the divider is not found, return false
    }
    return valid;
}

//Reads and validates when a positive integer is input into the terminal.
bool parsePositiveInteger(int& outputValue){
    string input;
    cin >> input;

    regex integerPattern("^[0-9]+$");
    if(regex_match(input, integerPattern)){
        int value= atoi(input.c_str());
        if(value > 0){
            outputValue= value;
            return true;
        }
    }
    return false;
}



// ============================================================
// Client Socket Handler
// ============================================================

//Sends all bytes in data.
bool sendAll(SOCKET socketNumber, const string& data){
    size_t total_sent=0;
    size_t bytes_left= data.size();
    const char* ptr= data.data();
    while(total_sent < data.size()){
        int n= send(socketNumber, ptr +total_sent, (int)bytes_left, 0);
        if(n == SOCKET_ERROR){
            cerr << "Send failed: " << WSAGetLastError() << endl;
            return false;
        }
        total_sent+= n;
        bytes_left-=n;
    }
    return false;
}

// Connects to the server using a hostname/IP address and port.
SOCKET connectToServer(const char* host, int port){
    struct hostent* hp;
    struct sockaddr_in sin;

    hp= gethostbyname(host);
    if(hp == NULL){
        cerr << "Unknown host: " << host << endl;
        return INVALID_SOCKET;
    }
    bzero(&sin, sizeof(sin));
    sin.sin_family= AF_INET;
    bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
    sin.sin_port= htons(port);

    SOCKET s=socket(AF_INET, SOCK_STREAM, 0);
    if(s == INVALID_SOCKET){
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        return INVALID_SOCKET;
    }
    if(connect(s, (struct sockaddr*)&sin, sizeof(sin))== SOCKET_ERROR){
        cerr << "Connection failed: " << WSAGetLastError() << endl;
        closesocket(s);
        return INVALID_SOCKET;
    }
    cout << "Connected to server" << endl;
    return s;
}



// ============================================================
// Receive Buffer and Message Framing
// ============================================================
//Sends one protocol line.
bool sendProtocolMessage(SOCKET socketNumber, const string& line) {
    string message= line;
    if(message.empty() || message.back() != '\n'){
        message += '\n';
    }
    return sendAll(socketNumber, message);  //ensures a command entered ends with \n to provide a new line
}
            
//Return one complete protocol line without its newline.
string receiveLine(SOCKET socketNumber){
    string line= "";
    char buffer[RECEIVE_CHUNK];
    while(true){
        int bytesReceived= recv(socketNumber, buffer, 1, 0);
        if(bytesReceived <= 0){
            break;
        }
        line.append(buffer, bytesReceived);
        size_t newLinePosition= line.find('\n');
        if(newLinePosition != string::npos){
            string completeLine= line.substr(0, newLinePosition);
            return completeLine;
        }
    }
    return "";
}   //string may or may not exist 


// ============================================================
// Protocol Formatter and Parser
// ============================================================

//Receives and displays one complete server response.
bool receiveResponse(SOCKET socketNumber){
    string responseCode= receiveLine(socketNumber);
    if(responseCode.empty()){
        return false;
    }
    size_t divider= responseCode.find('|');
    string code= "";
    string message= "";
    if(divider != string::npos){
        code= responseCode.substr(0, divider);
        message= responseCode.substr(divider + 1);
    }
    else{
        code=responseCode;
    }
     if (code == "200") {
        cout << "\n[200 OK] " << message << endl;
        return true;
    } 
    else if(code == "300"){
        cout << "\n[300 message format error]" << message << endl;
        return false;
    }
    else if(code== "400"){
        cout << "\n[400 unknown command]" << message << endl;
        return false;
    }
    else if(code== "401"){
        cout << "\n[401 you are not currently logged in, login first]" << message << endl;
        return false;
    }
    else if(code== "404"){
        cout << "\n[404 question not found]" << message << endl;
        return false;
    }
    else if(code== "409"){
        cout << "\n[409 command not allowed in current state]" << message << endl;
        return false;
    }
    else if(code== "410"){
        cout << "\n[410 wrong username or password]" << message << endl;
        return false;
    }
    else if(code== "413"){
        cout << "\n[413 content too large]" << message << endl;
        return false;
    }
    else if(code== "415"){
        cout << "\n[415 non-ASCII content]" << message << endl;
        return false;
    }
    else if(code== "500"){
        cout << "\n[500 internal server error]" << message << endl;
        return false;
    }
    else {
        // Catch-all for unexpected codes or raw multi-line payloads
        cout << "\n[" << code << "] " << message << endl;
        return true;
    }
}





  

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
    if(argc != 3){
        cerr << "Usage: client.exe <hostname> <port>" << endl;  //check the arguements
        return 1;
    }
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
        cerr << "Winsock initialization failed." << endl;
        return 1;
    }

    const char* host= argv[1];
    int port= atoi(argv[2]);
    SOCKET s= connectToServer(host,port);   //connect to the server

    if(s==INVALID_SOCKET){
        WSACleanup();
        return 1;
    }
    bool isUserAuthenticated= false;
    bool shouldQuit= false;             //initialize state

    while(!shouldQuit){
        if(!isUserAuthenticated){
            displayConnectedMenu();
            int choice= 0;
            if(!parsePositiveInteger(choice)){
                cout << "Invalid input. Please enter a valid menu number.\n" << endl;
                cin.clear();
                cin.ignore(MAX_LINE, '\n');
                continue;
            }
            if(choice == 1){    //LOGIN
                string username;
                string password;
                cout << "Username: ";
                cin >> username;
                cout << "Password: ";
                cin >> password;

                string cmd= "LOGIN|username= " + username + "|password= " + password;
                if(sendProtocolMessage(s, cmd)){
                    if(receiveResponse(s)){
                        isUserAuthenticated= true;
                    }
                }
            }
            else if(choice== 2){    //QUIT
                sendProtocolMessage(s, "QUIT");
                shouldQuit= true;
            }
            else{
                cout << "Choice out of range.\n" << endl;
            }
        }
        else{
            displayAuthenticatedMenu();
            int choice= 0;
            if (!parsePositiveInteger(choice)) {
                cout << "Invalid input. Please enter a valid menu number.\n" << endl;
                cin.clear();
                cin.ignore(MAX_LINE, '\n');
                continue;
            }
            else if(choice == 1){   //View Questions
                if(sendProtocolMessage(s, "VIEW_QUESTIONS")){
                    receiveResponse(s);
                }
            }
            else if(choice ==2){        //View Question and answers
                cout << "Enter Question Number: ";
                int qNum = 0;
                if (parsePositiveInteger(qNum)) {
                    string cmd = "VIEW_ANSWER|questionNumber=" + to_string(qNum);
                    if (sendProtocolMessage(s, cmd)) {
                        receiveResponse(s);
                    }
                } else {
                    cout << "Invalid question number format.\n" << endl;
                }
            }
            else if(choice == 3){   //Post question
                string title;
                string body;
                cout << "Enter Title: ";
                getline(cin, title);
                cout << "Enter Body: ";
                getline(cin, body);
                string cmd= "POST_QUESTION|title= " + title + "|body= " + body;
                if(sendProtocolMessage(s, cmd)){
                    receiveResponse(s);
                }
            }
            else if(choice == 4){   //Post Answer
                cout << "Enter Question Number: ";
                int qNum=0;
                if(parsePositiveInteger(qNum)){
                    string answer;
                    cout << "Enter Answer: ";
                    getline(cin, answer);
                    string cmd= "POST_ANSWER|questionNumber= " + to_string(qNum) + "|answer= " + answer;
                    if(sendProtocolMessage(s, cmd)){
                        receiveResponse(s);
                    }
                }
                else{
                    cout << "Invalid Question Number\n" << endl;
                }
            }
            else if(choice == 5){   //LOGOUT
                if(sendProtocolMessage(s, "LOGOUT")){
                    receiveResponse(s);
                    isUserAuthenticated= false;
                }
            }
            else if(choice == 6){   //QUIT
                if(sendProtocolMessage(s, "QUIT")){
                    receiveResponse(s);
                    shouldQuit= true;
                }
            }
            else{
                cout << "Choice out of range.\n" << endl;
            }
        }
    }


    closesocket(s);
    WSACleanup();
    return 0;
}





