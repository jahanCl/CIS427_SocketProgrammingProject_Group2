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
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace std;

#define MAX_PENDING 5
#define MAX_LINE 4096
#define RECEIVE_CHUNK 1024

// ============================================================
// Server Data Structures
// ============================================================

struct UserAccount {
    int userId;
    string username;
    string password;
};

struct Answer {
    int answerId;
    string author;
    string body;
};

struct Question {
    int questionNumber;
    string author;
    string title;
    string body;
    vector<Answer> answers;
};

struct Session {
    bool authenticated = false;
    int userId = 0;
    string username;

    void logout() {
        authenticated = false;
        userId = 0;
        username.clear();
    }
};

struct Response {
    vector<string> lines;
    bool successful = false;
    bool closeConnection = false;
    int resourceId = 0;
};

class DiscussionBoard {
private:
    map<string, UserAccount> accountsList;  //Stores list of user accounts, search by usernam
    map<int, Question> questionsList;       //Stores list of questions, search by question num
    int nextQuestionNumber = 1;             //Stores the next question number to be assigned
    int nextAnswerId = 1;                   //Stores the next answer ID to be assigned

public:
    DiscussionBoard() {
        //Predefined user accounts for testing purposes.
        accountsList["Jahan"] = UserAccount{1,"Jahan","password123"};
        accountsList["Kieran"] = UserAccount{ 2,"Kieran", "Cookies21"};
    }
    //Checks a username and password and returns the matching account.
    const UserAccount* authenticate(const string& username,const string& password) const {
        auto iterator = accountsList.find(username); //looks for username in 

                git switch -c my-feature-branch
        git add .
        git commit -m "Describe your changes"
        git push -u origin my-feature-branch
    }

    Question& addQuestion(
        const string& author,
        const string& title,
        const string& body
    ) {
        Question question{
            nextQuestionNumber++,
            author,
            title,
            body,
            {}
        };

        int number = question.questionNumber;

        questionsList[number] = question;

        return questionsList[number];
    }

    Question* findQuestion(int questionNumber) {
        auto iterator =
            questionsList.find(questionNumber);

        if (iterator == questionsList.end()) {
            return nullptr;
        }

        return &iterator->second;
    }

    Answer addAnswer(
        Question& question,
        const string& author,
        const string& body
    ) {
        Answer answer{
            nextAnswerId++,
            author,
            body
        };

        question.answers.push_back(answer);

        return answer;
    }

    const map<int, Question>& getQuestions() const {
        return questionsList;
    }
};

class ServerLogger {
private:
    ofstream file;

    string timestamp() {
        time_t now = time(nullptr);
        tm localTime{};

        localtime_r(&now, &localTime);

        char output[32];

        strftime(
            output,
            sizeof(output),
            "%Y-%m-%dT%H:%M:%S",
            &localTime
        );

        return output;
    }

public:
    explicit ServerLogger(
        const string& fileName
    ) {
        file.open(fileName, ios::app);
    }

    void log(
        const string& level,
        const string& message
    ) {
        file << timestamp()
             << ' '
             << level
             << ' '
             << message
             << '\n';

        file.flush();
    }
};


// ============================================================
// Protocol Validation and Parsing
// ============================================================
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
Response makeSuccess(const string& line, int resourceId = 0) {
    Response response;

    response.lines.push_back(line);
    response.successful = true;
    response.resourceId = resourceId;

    return response;
}

Response makeSuccess(const vector<string>& lines) {
    Response response;

    response.lines = lines;
    response.successful = true;

    return response;
}

Response makeError(const string& line) {
    Response response;

    response.lines.push_back(line);

    return response;
}

// ============================================================
// Server Socket Output
// ============================================================
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
Response processRequest( const string& request, Session& session,DiscussionBoard& board) {
    if (request.empty()) {
        return makeError("300 message format error");
    }

    vector<string> parts = split(request, '|');

    string command = parts.front();

    if (command == "LOGIN") {
        if (session.authenticated) {
            return makeError("409 command not allowed in current session state");
        }

        auto fields = parseFields(parts);

        if (!fields.has_value() ||parts.size() != 3 ||fields->size() != 2 ||
            fields->find("username") ==fields->end() ||
            fields->find("password") ==fields->end()) {
            return makeError("300 message format error");
        }

        string username = fields->at("username");
        string password =fields->at("password");

        if (!validField(username) || !validField(password)) {
            return makeError("300 message format error");
        }

        const UserAccount* account =board.authenticate( username, password);

        if (account == nullptr) {
            return makeError( "410 Wrong username or password." );
        }

        session.authenticated = true;
        session.userId = account->userId;
        session.username = account->username;

        return makeSuccess(  "200 OK|userId=" + to_string(account->userId) );
    }
    //LOGOUT
    if (command == "LOGOUT") {
        if (parts.size() != 1) {
            return makeError( "300 message format error");
        }

        if (!session.authenticated) {
            return makeError("401 You are not currently logged in, login first.");
        }
        session.logout();
        return makeSuccess("200 OK");
    }
    //VIEW_QUESTIONS
    if (command == "VIEW_QUESTIONS") {
        if (parts.size() != 1) {
            return makeError(
                "300 message format error"
            );
        }

        if (!session.authenticated) {
            return makeError(
                "401 You are not currently logged in, login first."
            );
        }

        vector<string> lines;

        lines.push_back("200 OK");

        lines.push_back(
            "COUNT|value=" +
            to_string(
                board.getQuestions().size()
            )
        );

        for (const auto& pair :
             board.getQuestions()) {

            const Question& question =
                pair.second;

            lines.push_back(
                "QUESTION|questionNumber=" +
                to_string(
                    question.questionNumber
                ) +
                "|author=" +
                question.author +
                "|title=" +
                question.title
            );
        }

        lines.push_back("END");

        return makeSuccess(lines);
    }

    if (command == "VIEW_ANSWER") {
        if (!session.authenticated) {
            return makeError(
                "401 You are not currently logged in, login first."
            );
        }

        auto fields =
            parseFields(parts);

        if (!fields.has_value() ||
            parts.size() != 2 ||
            fields->size() != 1 ||
            fields->find("questionNumber") ==
                fields->end()) {

            return makeError(
                "300 message format error"
            );
        }

        int questionNumber = 0;

        if (!parsePositiveInteger(
                fields->at(
                    "questionNumber"
                ),
                questionNumber)) {

            return makeError(
                "300 message format error"
            );
        }

        Question* question =
            board.findQuestion(
                questionNumber
            );

        if (question == nullptr) {
            return makeError(
                "404 question not found"
            );
        }

        vector<string> lines;

        lines.push_back("200 OK");

        lines.push_back(
            "QUESTION|questionNumber=" +
            to_string(
                question->questionNumber
            ) +
            "|author=" +
            question->author +
            "|title=" +
            question->title +
            "|body=" +
            question->body +
            "|answerCount=" +
            to_string(
                question->answers.size()
            )
        );

        for (const Answer& answer :
             question->answers) {

            lines.push_back(
                "ANSWER|answerId=" +
                to_string(
                    answer.answerId
                ) +
                "|author=" +
                answer.author +
                "|body=" +
                answer.body
            );
        }

        lines.push_back("END");

        return makeSuccess(lines);
    }

    if (command == "POST_QUESTION") {
        if (!session.authenticated) {
            return makeError(
                "401 You are not currently logged in, login first."
            );
        }

        auto fields =
            parseFields(parts);

        if (!fields.has_value() ||
            parts.size() != 3 ||
            fields->size() != 2 ||
            fields->find("title") ==
                fields->end() ||
            fields->find("body") ==
                fields->end()) {

            return makeError(
                "300 message format error"
            );
        }

        string title =
            fields->at("title");

        string body =
            fields->at("body");

        if (!validField(title) ||
            !validField(body)) {

            return makeError(
                "300 message format error"
            );
        }

        Question& question =
            board.addQuestion(
                session.username,
                title,
                body
            );

        return makeSuccess(
            "200 OK|questionNumber=" +
            to_string(
                question.questionNumber
            ),
            question.questionNumber
        );
    }

    if (command == "POST_ANSWER") {
        if (!session.authenticated) {
            return makeError(
                "401 You are not currently logged in, login first."
            );
        }

        auto fields =
            parseFields(parts);

        if (!fields.has_value() ||
            parts.size() != 3 ||
            fields->size() != 2 ||
            fields->find("questionNumber") ==
                fields->end() ||
            fields->find("answer") ==
                fields->end()) {

            return makeError(
                "300 message format error"
            );
        }

        int questionNumber = 0;

        if (!parsePositiveInteger(
                fields->at(
                    "questionNumber"
                ),
                questionNumber)) {

            return makeError(
                "300 message format error"
            );
        }

        string answerBody =
            fields->at("answer");

        if (!validField(answerBody)) {
            return makeError(
                "300 message format error"
            );
        }

        Question* question =
            board.findQuestion(
                questionNumber
            );

        if (question == nullptr) {
            return makeError(
                "404 question not found"
            );
        }

        Answer answer =
            board.addAnswer(
                *question,
                session.username,
                answerBody
            );

        return makeSuccess(
            "200 OK|answerId=" +
            to_string(
                answer.answerId
            ) +
            "|questionNumber=" +
            to_string(
                questionNumber
            ),
            answer.answerId
        );
    }

    if (command == "QUIT") {
        if (parts.size() != 1) {
            return makeError(
                "300 message format error"
            );
        }

        session.logout();

        Response response =
            makeSuccess("200 OK");

        response.closeConnection = true;

        return response;
    }

    return makeError(
        "400 unknown command"
    );
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

    ServerLogger logger(
        "server.log"
    );

    DiscussionBoard board;

    int serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (serverSocket < 0) {
        cerr << "socket failed: "
             << strerror(errno)
             << '\n';

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

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_addr.s_addr =
        htonl(INADDR_ANY);

    serverAddress.sin_port =
        htons(
            static_cast<uint16_t>(
                port
            )
        );

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)) < 0) {

        cerr << "bind failed: "
             << strerror(errno)
             << '\n';

        close(serverSocket);

        return 1;
    }

    if (listen(
            serverSocket,
            MAX_PENDING) < 0) {

        cerr << "listen failed: "
             << strerror(errno)
             << '\n';

        close(serverSocket);

        return 1;
    }

    cout << "Server listening on port "
         << port
         << '\n';

    logger.log(
        "INFO",
        "Server startup"
    );

    logger.log(
        "INFO",
        "Server listening on "
        "0.0.0.0:" +
        to_string(port)
    );

    int nextClientNumber = 1;

    while (true) {
        sockaddr_in clientAddress{};

        socklen_t clientAddressLength =
            sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(
                &clientAddress
            ),
            &clientAddressLength
        );

        if (clientSocket < 0) {
            if (errno == EINTR) {
                continue;
            }

            logger.log(
                "ERROR",
                "accept failed=" +
                string(
                    strerror(errno)
                )
            );

            break;
        }

        string clientId =
            "client-" +
            to_string(
                nextClientNumber++
            );

        char addressText[
            INET_ADDRSTRLEN
        ]{};

        inet_ntop(
            AF_INET,
            &clientAddress.sin_addr,
            addressText,
            sizeof(addressText)
        );

        logger.log(
            "INFO",
            clientId +
            " connected from " +
            addressText
        );

        handleClient(
            clientSocket,
            clientId,
            board,
            logger
        );
    }

    close(serverSocket);

    logger.log(
        "INFO",
        "Server shutdown"
    );

    return 0;
}
