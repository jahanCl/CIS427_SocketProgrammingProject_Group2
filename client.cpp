/*Overall client flow:
 * 1. Validate the server hostname and port.
 * 2. Connect to the server through TCP.
 * 3. Display a menu based on login state.
 * 4. Collect and validate user input.
 * 5. Format and send a protocol request.
 * 6. Receive and display the server response.
 * 7. Repeat until the user sends QUIT.*/


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

using namespace std;

#define MAX_LINE 4096        // Maximum bytes in one protocol line.
#define RECEIVE_CHUNK 1024 // Number of bytes requested per recv().


// ============================================================
// Input Validator
// ============================================================

//Returns true if every character in a string is an ASCII character.
bool isASCii(const string& text){     
    for (unsigned char character : text) {   //loop through evrey byte in string 
        if (character > 127 || character < 0) {   //if the byte is not in the ASCII range
                return false;   //return false
        }
    }
    return true;
}


//Validates one user-provided field such as a username, title, or answer.
bool validField(const string& text) {
    return !text.empty() &&                  //Required fields cannot be empty.
           isASCii(text) &&                  //Only ASCII text is accepted.
           text.find('|') == string::npos && // | is only for seperating fields.
           text.find('\n') == string::npos ; //Newline not inside a protocol message, only at end.
}

// Converts a string into a positive integer.
 
bool parsePositiveInteger(const string& text,int& output) {
    try {
        size_t used = 0;                // Records how much of the string was parsed.
        int value = stoi(text, &used);  // Attempts to convert the string to an integer.

        if (used != text.size() ||      // Rejects trailing text, such as "12abc".
            value <= 0) {               // Rejects zero and negative numbers.
            return false;
        }

        output = value;                 // Stores the validated integer.
        return true;
    }
    catch (...) {                       // Catches invalid or out-of-range input.
        return false;
    }
}


// ============================================================
// Client Socket Handler
// ============================================================

//Sends every byte in a string.
bool sendAll( int socketNumber, const string& data) {
    size_t totalSent = 0;   //Tracks the number of bytes already sent.

    while (totalSent < data.size()) { // Continue until all bytes are sent.
        ssize_t bytesSent = send(
            socketNumber,             //Connected socket.
            data.data() + totalSent,  //Address of the first unsent byte.
            data.size() - totalSent,  //Number of remaining bytes.
            0                         //No special flags.
        );

        if (bytesSent < 0) {          //A negative value means send() failed.
            if (errno == EINTR) {     //The system call was interrupted.
                continue;             //Retry the send operation.
            }
            cerr << "Send failed: "<< strerror(errno)<< '\n';
            return false;             
        }
        if (bytesSent == 0) {         //No data was sent.
            return false;
        }
        totalSent += static_cast<size_t>(bytesSent); //Update the total number of bytes sent. 
    }
    return true;    //Every byte was sent successfully.
}


//Adds the required newline delimiter and sends one protocol line.
bool sendLine(int socketNumber,const string& line) {
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

//Finds a hostname or IP address and connects to the server.
int connectToServer(const string& host, const string& port) {
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

// ============================================================
// Receive Buffer and Message Framing
// ============================================================
/// Receives exactly one newline-delimited protocol line
///optinal becasue could be no line to return if the connection is closed or an error occurs.
optional<string> receiveLine(int socketNumber,string& receiveBuffer) {
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
            cerr << "400 unknown command.\n";
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


// ============================================================
// Protocol Formatter and Parser
// ============================================================

// Receives and displays one complete server response.
bool receiveResponse(int socketNumber,string& receiveBuffer,bool mayBeMultiline) {
    optional<string> firstLine =receiveLine(socketNumber,receiveBuffer);

    if (!firstLine.has_value()) {     // Network or framing error occurred.
        return false;
    }

    cout << *firstLine << '\n';   //Display line status, error, or COUNT. 
    // If the response is not a successful multiline response, return.
    if (!mayBeMultiline ||firstLine->rfind("200 OK", 0) != 0) {
        return true;                  
    }

    while (true) {                     
        // Continue through successful multiline data.
        optional<string> line =receiveLine( socketNumber,receiveBuffer);
        if (!line.has_value()) {return false;}

        cout << *line << '\n';   //Display COUNT, QUESTION, ANSWER, or END.

        if (*line == "END") {  //END marks the response boundary.
            return true;
        }
    }
}

// ============================================================
// Menu-Driven Command-Line Interface and Client State
// ============================================================
void displayConnectedMenu() {
    cout << "\n1. Login\n"
         << "2. Quit\n"
         << "Selection: ";
}

void displayAuthenticatedMenu() {
    cout << "\nDiscussion Board Menu:\n"
         << "1. View questions\n"
         << "2. View question and answers\n"
         << "3. Post question\n"
         << "4. Post answer\n"
         << "5. Logout\n"
         << "6. Quit\n"
         << "Selection: ";
}

// ============================================================
// Main Client Flow
// ============================================================

int main(int argc, char* argv[]) {
    // Expected command: ./client <hostname-or-IP> <port>
    if (argc != 3) {
        cerr << "Usage: "<< argv[0]<< " <hostname-or-IP> <port>\n";
        return 1;
    }
    int portNumber = 0;   //Stores the validated port.

    if (!parsePositiveInteger(argv[2],portNumber) ||portNumber > 65535) {
        cerr << "Invalid port.\n";
        return 1;
    }
    //argv[1]: host name or IP address, argv[2]: port number
    int socketNumber = connectToServer(argv[1], argv[2]);

    //Invalid socket number means the connection failed.
    if (socketNumber < 0) {
        cerr << "Unable to connect to server.\n";
        return 1;
    }

    cout << "Connected to server.\n";

    bool authenticated = false; //Controls which menu is displayed.
    bool running = true;        //Controls when the program exits.
    string receiveBuffer;       //Stores unused TCP response bytes.

    while (running) {
        if (authenticated) {
            displayAuthenticatedMenu();
        }
        else {
            displayConnectedMenu();
        }

        string choice;
        getline(cin, choice);  //Read the complete menu selection.

        //LOGIN
        if (!authenticated && choice == "1") {
            string username;
            string password;

            cout << "Username: ";
            getline(cin, username);

            cout << "Password: ";
            getline(cin, password);

            if (!validField(username) ||!validField(password)) {
                cout << "410 Wrong username or password\n";
                continue;
            }
            //Format of LOGIN request
            string request ="LOGIN|username=" + username +"|password=" + password;

            if (!sendLine(socketNumber, request)) {
                break;
            }

            optional<string> response =receiveLine(socketNumber,receiveBuffer);

            if (!response.has_value()) {
                break;
            }

            cout << *response << '\n';

            //Check server authetication.The client updates menu only after success.
            if (response->rfind("200 OK|userId=",0) == 0) {
                authenticated = true;
            }
        } 

        //QUIT before loging in
        else if (!authenticated && choice == "2") {  //Choose to quit
            if (sendLine(socketNumber, "QUIT")) {
                receiveResponse(socketNumber,receiveBuffer,false);
            }
            running = false;
        }

        //VIEW_QUESTIONS
        //Discusstion Board option 1
        else if (authenticated && choice == "1") {   //
            if (!sendLine(socketNumber,"VIEW_QUESTIONS") ||
                !receiveResponse(socketNumber,receiveBuffer,true)) {
                break;
            }
        }
        //VIEW_ANSWER
        else if (authenticated && choice == "2") {
            string questionText;     //Stores user input for question number.
            int questionNumber = 0;  //set to 0 to indicate no valid question number yet.

            cout << "Question number: ";
            getline(cin, questionText);

            if (!parsePositiveInteger(questionText,questionNumber)) {
                cout << "404 question not Found.\n";
                continue;
            }

            //whats sent to the server for VIEW_ANSWER request
            string request = "VIEW_ANSWER|questionNumber=" + to_string(questionNumber);

            if (!sendLine(socketNumber, request) ||
                !receiveResponse(socketNumber,receiveBuffer,true)) {
                break;
            }
        }
        //POST_QUESTION
        else if (authenticated && choice == "3") {
            string title;   //question title
            string body;    //question body

            cout << "Title: ";
            getline(cin, title);

            cout << "Question: ";
            getline(cin, body);

            if (!validField(title) ||!validField(body)) {
                cout << "300 message format error.\n";
                continue;
            }
            //whats sent to the server for POST_QUESTION request
            string request ="POST_QUESTION|title=" + title +"|body=" + body;

            if (!sendLine(socketNumber, request) ||
                !receiveResponse( socketNumber, receiveBuffer,false)) {
                break;
            }
        }

        //POST_ANSWER
        else if (authenticated && choice == "4") {
            string questionText;
            string answer;
            int questionNumber = 0;

            cout << "Question number: ";
            getline(cin, questionText);

            if (!parsePositiveInteger(questionText, questionNumber)) {
                cout << "Invalid question number.\n";
                continue;
            }

            cout << "Answer: ";
            getline(cin, answer);

            if (!validField(answer)) {
                cout << "Invalid answer format.\n";
                continue;
            }

            string request = "POST_ANSWER|questionNumber=" +to_string(questionNumber) +"|answer=" + answer;

            if (!sendLine(socketNumber, request) ||
                !receiveResponse(socketNumber, receiveBuffer,false)) {
                break;
            }
        }

        //LOGOUT
        else if (authenticated && choice == "5") {
            if (!sendLine(socketNumber, "LOGOUT")) {
                break;
            }

            optional<string> response =receiveLine(socketNumber, receiveBuffer);

            if (!response.has_value()) {
                break;
            }

            cout << *response << '\n';

            if (*response == "200 OK") {
                authenticated = false;
            }
        }
        //QUIT after logging in
        else if (authenticated && choice == "6") {
            if (sendLine(socketNumber, "QUIT")) {
                receiveResponse( socketNumber,receiveBuffer,false);
            }
            running = false;
        }
        //Invalid menu input
        else {
            cout << "300 message format error\n";
        }
    }

    close(socketNumber);  //Release the TCP socket.
    return 0;
}