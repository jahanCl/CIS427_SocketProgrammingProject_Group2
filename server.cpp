#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <thread>
#include <functional>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include"utilities.h"
#include"serverDataStruct.h"

using namespace std;

#define MAX_PENDING 5
#define MAX_LINE 4096
#define RECEIVE_CHUNK 1024


//Seperates a string into substrings based on a delimiter character.
vector<string> split(const string& text,char delimiter) {
    vector<string> results;  //Stores the resulting substrings.
    string current;        //Stores the current substring being built.

    for (char character : text) {     //Iterates through each character in the input string.
        if (character == delimiter) {  //If the current character is the delimiter,  substring is complete.
            results.push_back(current);  //Adds the completed substring to the result vector.
            current.clear();           //Reset the current substring for the next one.
        }
        else {
            current.push_back(character);
        }
    }
    results.push_back(current);      //Adds the last substring to the result vector.
    return results;                  //Returns the vector of substrings.
}

//Receives one complete protocol line from the server.
optional<map<string, string>> parseFields(const vector<string>& parts) {
    map<string, string> fields;
    //Iterates through the parts of the protocol message, starting from index 1 to skip the command.
    for (size_t index = 1;index < parts.size(); ++index) {
        size_t equalsPosition = parts[index].find('='); //Find postion of = 

        //If '=' not found or some error in format
        if (equalsPosition ==string::npos ||equalsPosition == 0 ||
            equalsPosition ==parts[index].size() - 1) {
            return nullopt;
        }
        //Extracts the field name fromthe protocol message.
        string name =parts[index].substr(0,equalsPosition);

        //Extracts the field value fromthe protocol message.
        string value =parts[index].substr(equalsPosition + 1);

        //If the field name already exists in the map, return nullopt to indicate an error.
        if (fields.find(name) !=fields.end()) {
            return nullopt;
        }
        fields[name] = value;  //Adds field name and value to the map of fields.
    }
    return fields;
}

// ============================================================
// Structured Response Helpers
// ============================================================
//Creates a successful response for one protocol lines.
Response makeSuccess(const string& line, int resourceId = 0) {
    Response response;   //crear responce object 
    response.lines.push_back(line);  //Add the protocol line to the response's list of outgoing lines.

    response.successful = true;  //The application operation was successful
    response.resourceId = resourceId; //Store the related question or answer ID. 
    return response;        //Return the completed response to the command handler.
} 
//Creates a successful response containing multiple protocol lines.
Response makeSuccess(const vector<string>& lines) {
    Response response;
    response.lines = lines;  //Copy all protocol lines into the response.
    response.successful = true;
    return response;
}

//Creates an unsuccessful single line response.
Response makeError(const string& line) {
    Response response;
    response.lines.push_back(line);  //Adds the error protocol line that will be sent to the client.
    return response;
}

// ============================================================
// Server Socket Output
// =========================================================


//Search responce for socket
bool sendResponse(int socketNumber,const Response& response) {
    for (const string& line :response.lines) {
        if (!sendAll(socketNumber, line + "\n")) {
            return false;
        }
    }

    return true;
}


// ============================================================
// Discussion-Board Command Processing
// ============================================================
//Processes one complete client request.
Response processRequest( const string& request, Session& session,DiscussionBoard& board) {
    //If request sent by client empty, error
    if (request.empty()) {
        return makeError("300 message format error");
    }
    // Split the request at each | character.
    vector<string> parts = split(request, '|');
    string command = parts.front(); //Get the command name, Like LOGIN

    if (command == "LOGIN") {
        //A client cannot log in again while already authenticated.
        if (session.authenticated) {
            return makeError("409 command not allowed in current session state");
        }
        //Parse username=... and password=... fields.
        auto fields = parseFields(parts);
        //Checks for missing fields
        if (!fields.has_value() ||parts.size() != 3 ||fields->size() != 2 ||
            fields->find("username") ==fields->end() ||
            fields->find("password") ==fields->end()) {
            return makeError("300 message format error");
        }
        //Get the submitted field values.
        string username = fields->at("username");
        string password =fields->at("password");

        //Checks for foramts errors 
        if (!validField(username) || !validField(password)) {
            return makeError("415 non-ASCII content");
        }

        //Verifies the account credentials.
        const UserAccount* account = board.authenticate( username, password);
        //null pointer means the username or password was incorrect.
        if (account == nullptr) {
            return makeError( "410 Wrong username or password." );
        }

        //Store the authenticated identity in this connection's session.
        session.authenticated = true;
        session.userId = account->userId;
        session.username = account->username;

        //Return the authenticated user's ID.
        return makeSuccess(  "200 OK|userId=" + to_string(account->userId) );
    }
    //LOGOUT
    if (command == "LOGOUT") {
        //Not fields allowed in LOGOUT
        if (parts.size() != 1) {
            return makeError( "300 message format error");
        }
        //The client cannot log out before logging in.
        if (!session.authenticated) {
            return makeError("401 You are not currently logged in, login first.");
        }
        //Clear login information but leave the TCP connection open.
        session.logout();
        return makeSuccess("200 OK");
    }
    //VIEW_QUESTIONS
    if (command == "VIEW_QUESTIONS") {
        //NO fields found
        if (parts.size() != 1) {
            return makeError("300 message format error");
        }
        //Viewing discussion data requires login.
        if (!session.authenticated) {
            return makeError("401 You are not currently logged in, login first.");
        }

        vector<string> lines;  //Builds a multiline response.
        lines.push_back("200 OK"); //First line is the success status.

        //Second line tells the client how many questions follow.
        lines.push_back("COUNT|value=" +to_string( board.getQuestions().size()));

        //Add one summary line for each stored question.
        for (const auto& pair :board.getQuestions()) {
            const Question& question = pair.second;
            lines.push_back("Question Number: " + to_string(question.questionNumber));
            lines.push_back("Author: " + question.author);
            lines.push_back("Question: " + question.title);
            lines.push_back("=======");
        }
        //------- tells the client where the multiline response finishes.
        lines.push_back("-------");
        return makeSuccess(lines);
    }
    //VIEW_ANSWER
    if (command == "VIEW_ANSWER") {
        //Viewing a discussion requires login.
        if (!session.authenticated) {
            return makeError( "401 You are not currently logged in, login first.");
        }
        // Parse questionNumber=...
        auto fields =parseFields(parts);

        if (!fields.has_value() || parts.size() != 2 ||
            fields->size() != 1 ||fields->find("questionNumber") == fields->end()) {

            return makeError("300 message format error");
        }

        int questionNumber = 0;
         //the question number is positive integer.
        if (!parsePositiveInteger(fields->at("questionNumber"),questionNumber)) {
            return makeError("300 message format error");
        }
        //Search server data for the selected question.
        Question* question = board.findQuestion(questionNumber);
        //nullptr == question not found
        if (question == nullptr) {
            return makeError( "404 question not found");
        }

        vector<string> lines; //Build a multiline response containing the question and answers.

        lines.push_back("200 OK");
        //Add complete question information and answer count.
        lines.push_back("Author: " + question->author);
        lines.push_back("Title: " + question->title);
        lines.push_back("Question: " + question->body);
        lines.push_back("Number of Answers: " + to_string(question->answers.size()));
        if(question->answers.size() != 0){
            lines.push_back("Answers: ");
        }
        //Add one response line for every associated answer.
        for (const Answer& answer :question->answers) {
            lines.push_back("=======");
            lines.push_back("Author: " + answer.author);
            lines.push_back("Answer: "+ answer.body);
            lines.push_back("=======");
            
        }
        // Mark the end of the complete response.
        lines.push_back("-------");
        return makeSuccess(lines);
    }
    //POST_QUESTION
    if (command == "POST_QUESTION") {
        if (!session.authenticated) {
            return makeError("401 You are not currently logged in, login first.");
        }

        auto fields = parseFields(parts);

        if (!fields.has_value() ||parts.size() != 3 ||fields->size() != 2 ||
            fields->find("title") ==fields->end() ||
            fields->find("body") ==fields->end()) {
            return makeError("300 message format error" );
        }

        string title = fields->at("title");
        string body = fields->at("body");

        if (!validField(title) || !validField(body)) {
            return makeError("300 message format error" );
        }

        Question& question = board.addQuestion(session.username,title,body);
        return makeSuccess( "200 OK|questionNumber=" + to_string(question.questionNumber),
        question.questionNumber);
    }
    //POST_ANSWER
    if (command == "POST_ANSWER") {
        if (!session.authenticated) {
            return makeError( "401 You are not currently logged in, login first.");
        }

        auto fields = parseFields(parts);

        if (!fields.has_value() ||parts.size() != 3 ||fields->size() != 2 ||
            fields->find("questionNumber") ==fields->end() ||
            fields->find("answer") ==fields->end()) {
            return makeError("300 message format error");
        }

        int questionNumber = 0;

        if (!parsePositiveInteger(fields->at("questionNumber"),questionNumber)) {

            return makeError("300 message format error" );
        }

        string answerBody = fields->at("answer");
 
        if (!validField(answerBody)) {
            return makeError("300 message format error");
        }

        Question* question = board.findQuestion(questionNumber);
        if (question == nullptr) {
            return makeError("404 question not found");
        }

        Answer answer = board.addAnswer(*question,session.username,answerBody);

        return makeSuccess("200 OK|answerId=" +to_string(answer.answerId) +
            "|questionNumber=" +to_string(questionNumber), answer.answerId);
    }
    //QUIT
    if (command == "QUIT") {
        if (parts.size() != 1) {
            return makeError("300 message format error");
        }
        session.logout();
        Response response =makeSuccess("200 OK");
        response.closeConnection = true;
        return response;
    }

    return makeError( "400 unknown command");
}

// ============================================================
// Server Socket Handler
// ============================================================
void handleClient(int clientSocket,const string& clientId,DiscussionBoard& board,
    ServerLogger& logger) {
    Session session;        //creat a session
    string receiveBuffer;  
    char incoming[RECEIVE_CHUNK]; //incoming message/bytes

    bool gracefulDisconnect = false;      
    bool discardingOversizedLine = false;

    while (true) {
        //Recives incoming data
        ssize_t bytesReceived = recv(clientSocket,incoming,sizeof(incoming),0);

        //Given nothing, exit server 
        if (bytesReceived == 0) {
            logger.log("WARN",clientId +" disconnected unexpectedly" );
            break;
        }

        if (bytesReceived < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger.log("WARN", clientId +" receive error=" +strerror(errno));
            break;
        }

        receiveBuffer.append(incoming,static_cast<size_t>( bytesReceived)
        );

        while (true) {
            size_t newline =receiveBuffer.find('\n');

            if (discardingOversizedLine) {
                if (newline ==string::npos) {
                    receiveBuffer.clear();
                    break;
                }

                receiveBuffer.erase(0, newline + 1);
                discardingOversizedLine =false;
                continue;
            }

            if (newline ==string::npos) {

                if (receiveBuffer.size() >= MAX_LINE) {
                    sendAll(clientSocket,"413 message too large\n");

                    logger.log("WARN",clientId +" protocol error="+"message too large");
                    receiveBuffer.clear();
                    discardingOversizedLine =true;
                }
                break;
            }

            size_t framedSize = newline + 1;
            string request = receiveBuffer.substr( 0,  newline );
            receiveBuffer.erase( 0,  framedSize );

            if (framedSize > MAX_LINE) {
                sendAll( clientSocket, "413 message too large\n"  );
                logger.log( "WARN",  clientId +" protocol error="+"message too large" );
                continue;
            }

            if (!isASCii(request)) {
                sendAll(clientSocket,"415 non-ASCII content\n" );
                logger.log( "WARN",clientId +" protocol error="+"non-ASCII content");
                continue;
            }

            string command =split(request,'|' ).front();
            logger.log("INFO",clientId +" request=" +command);

            Response response;
            try {
                response =processRequest(request,session,board);
            }
            catch (...) {
                response =makeError("500 internal server error" );
            }

            if (!sendResponse(clientSocket,response)) {
                logger.log("WARN",clientId +" send failed" );
                close(clientSocket);
                return;
            }

            logger.log(response.successful? "INFO": "WARN",clientId +" command=" +
                command +" outcome=" +(response.successful? "success": "error") );

            if (response.closeConnection) {
                gracefulDisconnect = true;
                break;
            }
        }

        if (gracefulDisconnect) {
            break;
        }
    }

    close(clientSocket);

    if (gracefulDisconnect) {logger.log( "INFO",clientId +" disconnected gracefully");
    }
}





int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: "<< argv[0] << " <port>\n";
        return 1;
    }

    int port = 0;

    if (!parsePositiveInteger(argv[1],port) ||port > 65535) {

        cerr << "Invalid port.\n";

        return 1;
    }

    ServerLogger logger("server.log");

    DiscussionBoard board;

    int serverSocket = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
    if (serverSocket < 0) {cerr << "socket failed: "<< strerror(errno)<< '\n';
        return 1;
    }

    int reuseAddress = 1;
    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuseAddress,
        sizeof(reuseAddress)
    );

    sockaddr_in serverAddress{};

    serverAddress.sin_family =AF_INET;

    serverAddress.sin_addr.s_addr =htonl(INADDR_ANY);

    serverAddress.sin_port =
        htons(static_cast<uint16_t>(port));

    if (bind(serverSocket,reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)) < 0) {

        cerr << "bind failed: "<< strerror(errno)<< '\n';
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket,MAX_PENDING) < 0) {

        cerr << "listen failed: "<< strerror(errno)<< '\n';
        close(serverSocket);
        return 1;
    }

    cout << "Server listening on port " << port<< '\n';

    logger.log("INFO","Server startup");

    logger.log("INFO","Server listening on ""0.0.0.0:" +to_string(port));

    int nextClientNumber = 1;

    while (true) {sockaddr_in clientAddress{};

        socklen_t clientAddressLength =sizeof(clientAddress);

        int clientSocket = accept(serverSocket,reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength);

        if (clientSocket < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger.log("ERROR","accept failed=" +string(strerror(errno)));
            break;
        }

        string clientId = "client-" + to_string(nextClientNumber++);
        char addressText[
            INET_ADDRSTRLEN
        ]{};

        inet_ntop(
            AF_INET,
            &clientAddress.sin_addr,
            addressText,
            sizeof(addressText)
        );

        logger.log("INFO", clientId +" connected from " + addressText);
        thread clientThread(handleClient, clientSocket, clientId, ref(board), ref(logger));
        clientThread.detach();
    }

    close(serverSocket);
    logger.log("INFO","Server shutdown");
    return 0;
};
