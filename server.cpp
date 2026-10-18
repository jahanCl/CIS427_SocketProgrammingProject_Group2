#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#pragma comment(lib, "Ws2_32.lib")
#define bzero(b, len) memset((b), 0, (len))
#define close(s) closesocket(s)

#define SERVER_PORT 6543
#define MAX_PENDING 5
#define MAX_LINE 4096
using namespace std;

struct Question{
    int id;
    string title;
    string body;
    vector<string> answers;
};
vector<Question> all_questions;
int next_question_id=1;
map<string, string> user_database={
    {"Jahan", "password123"},
    {"Kieran", "Cookies21"},
    {"Professor", "Teach100"}
};
bool isASCii(const string& text){     
     for (unsigned char character : text) {   //loop through evrey byte in string 
            if (character > 127 || character < 0) {   //if the byte is not in the ASCII range
                return false;   //return false
            }
        }
    return true;
}
bool sendAll(SOCKET socketNumber, const string& data) {
    size_t total_sent = 0;
    size_t bytes_left = data.size();
    const char* ptr = data.data();
    while (total_sent < data.size()) {
        int n = send(socketNumber, ptr + total_sent, (int)bytes_left, 0);
        if (n == SOCKET_ERROR) return false;
        total_sent += n;
        bytes_left -= n;
    }
    return true;
}
bool sendProtocolMessage(SOCKET socketNumber, const string& line) {
    string framed_message = line;
    if (framed_message.empty() || framed_message.back() != '\n') {
        framed_message += '\n';
    }
    return sendAll(socketNumber, framed_message);
}

bool authenticateUser(const string& username, const string& password){
    auto it= user_database.find(username);
    if(it != user_database.end() && it->second == password){
        return true;
    }
    return false;
}
string ViewAllQuestions(){
    if(all_questions.empty()){
        return "SUCCESS|No questions have been posted yet.";
    }
    string result= "SUCCESS|";
    for(const auto& q : all_questions){
        result+= "=======================================\n";
        result+= "Question ID: " + to_string(q.id) + "\n";
        result += "Title: " + q.title + "\n";
        result+= "Body: " + q.body + "\n";
        result+= "Total Answers: " + to_string(sizeof(q.answers)) + "\n";
    }
    result += "========================================";
    return result;
}
string postQuestion(const string& title, const string& body){
    Question newQ;
    newQ.id= next_question_id++;
    newQ.title= title;
    newQ.body= body;
    all_questions.push_back(newQ);
    return "SUCCESS|Question posted successfully with ID of " + to_string(newQ.id);
}
string ViewQuestionandAnswers(int questionID){
    for(const auto& q : all_questions){
        if(q.id == questionID){
            string result= "SUCCESS|Title: " + q.title + "\nBody: " + q.body + "\nAnswers: \n";
            if(q.answers.empty()){
                result += "No answers posted yet.";
            }
            else{
                for(int i=0; i < q.answers.size(); i++){
                    result += "- " + q.answers[i] + "\n";
                }
            }
            return result;
        }
    }
    return "Error|Question ID " + to_string(questionID) + " not found.";
}
string PostAnswerToQuestion(int questionID, const string& answerText){
    for(auto& q : all_questions){
        if(q.id == questionID){
            q.answers.push_back(answerText);
            return "SUCCESS|Answer added to question " + to_string(questionID);
        }
    }
    return "Error|Question ID not found.";
}

int main(){
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
        fprintf(stderr, "Winsock initialization failed.\n");
        return 1;
    }
    struct sockaddr_in sin;
    SOCKET s, new_s;

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family= AF_INET;
    sin.sin_addr.s_addr=INADDR_ANY;
    sin.sin_port= htons(SERVER_PORT);

    if((s= socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET){
        fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }
    if((bind(s, (struct sockaddr *)&sin, sizeof(sin))) == SOCKET_ERROR){
        fprintf(stderr, "Bind failed: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        exit(1);
    }

    listen(s,MAX_PENDING);
    cout << "Server is listening on port " << SERVER_PORT << "..." << endl;

    while(1){
        int addr_len= sizeof(sin);
        if((new_s= accept(s,(struct sockaddr *)&sin, &addr_len)) == INVALID_SOCKET){
            fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
            continue;
        }
        cout << "\n[LOG] Client connected successfully." << endl;

        bool isClientLoggedIn= false;
        string loggedInUser="";
        string currentLine= "";
        char character;
        int buf_len;

        while((buf_len= recv(new_s, &character, 1, 0)) > 0){
            if(character == '\n'){
                cout << "[REQUEST RECEIVED] " << currentLine << endl;
                if(currentLine.size() > MAX_LINE){
                    sendProtocolMessage(new_s, "413 Content too Large");
                    currentLine= "";
                    continue;
                }
                if(!isASCii(currentLine)){
                    sendProtocolMessage(new_s, "415 non-ASCII content");
                    continue;
                }
                if(currentLine.rfind("LOGIN|", 0) == 0){
                    if(isClientLoggedIn){
                        sendProtocolMessage(new_s, "409 command not allowed in current session state");
                        currentLine="";
                        continue;
                    }
                    regex pattern(R"(^LOGIN\|username=([^|]+)\|password=([^\n]+)$)");
                    smatch matches;
                    if (regex_match(currentLine, matches, pattern)) {
                        string user = matches[1].str();
                        string pass = matches[2].str();

                        if(authenticateUser(user, pass)){
                            isClientLoggedIn=true;
                            loggedInUser= user;
                            sendProtocolMessage(new_s, "200 OK");
                        }
                        else{
                            sendProtocolMessage(new_s, "410 Wrong username or password");
                        }
                    }else {
                        sendProtocolMessage(new_s, "300 message format error");
                    }
                }
                else if(currentLine != "QUIT" && !isClientLoggedIn){
                    sendProtocolMessage(new_s, "401 You are not currently logged in, login first");
                }
                else if(currentLine == "VIEW_QUESTIONS"){
                    string payload= ViewAllQuestions();
                    if(payload.rfind("SUCCESS|", 0) == 0){
                        string cleanData= payload.substr(8);
                        sendProtocolMessage(new_s, "200 OK\n" + cleanData);
                    } else{
                        sendProtocolMessage(new_s, "500 internal server error");
                    }
                }
                else if(currentLine.rfind("VIEW_ANSWER|", 0) == 0){
                    regex pattern(R"(^VIEW_ANSWER\|questionNumber=([0-9]+)$)");
                    smatch matches;
                    if (regex_match(currentLine, matches, pattern)) {
                        int qId = atoi(matches[1].str().c_str());
                        string payload= ViewQuestionandAnswers(qId);
                        if(payload.rfind("SUCCESS|", 0) == 0){
                            sendProtocolMessage(new_s, "200 OK\n" + payload.substr(8));
                        }else {
                            sendProtocolMessage(new_s, "404 question not found");
                        }
                    }else{
                        sendProtocolMessage(new_s, "300 message format error");
                    }
                }
                else if (currentLine.rfind("POST_QUESTION|", 0) == 0) {
                    regex pattern(R"(^POST_QUESTION\|title=([^|]+)\|body=([^\n]+)$)");
                    smatch matches;
                    if (regex_match(currentLine, matches, pattern)) {
                        string title = matches[1].str();
                        string body = matches[2].str();
                        
                        string payload = postQuestion(title, body);
                        sendProtocolMessage(new_s, "200 OK - " + payload.substr(8));
                    }else {
                        sendProtocolMessage(new_s, "300 message format error");
                    }
                }
                else if (currentLine.rfind("POST_ANSWER|", 0) == 0) {
                    regex pattern(R"(^POST_ANSWER\|questionNumber=([0-9]+)\|answer=([^\n]+)$)");
                    smatch matches;
                    if (regex_match(currentLine, matches, pattern)) {
                        int qId = atoi(matches[1].str().c_str());
                        string ansText = matches[2].str();

                        string payload = PostAnswerToQuestion(qId, ansText);
                        if (payload.rfind("SUCCESS|", 0) == 0) {
                            sendProtocolMessage(new_s, "200 OK");
                        } else {
                            sendProtocolMessage(new_s, "404 question not Found");
                        }
                    } else{
                        sendProtocolMessage(new_s, "300 message format error");
                    }
                }
                else if(currentLine == "LOGOUT"){
                    isClientLoggedIn= false;
                    loggedInUser="";
                    sendProtocolMessage(new_s, "200 OK - Successfully logged out");
                }
                else if(currentLine == "QUIT"){
                    cout << "[LOG] Client requested connection termination." << endl;
                    break;
                }
                else{
                    sendProtocolMessage(new_s, "400 unknown command");
                }
                currentLine= "";
            }else{
                currentLine += character;
            }
        }
        if(buf_len == 0){
            cout << "[LOG] Client disconnected normally." << endl;
        } else if(buf_len == SOCKET_ERROR){
            cout << "[LOG] Client dropped connection abruptly. Error : " << WSAGetLastError() << endl;
        }
        close(new_s);
        cout << "[LOG] Standby mode. Awaiting next connection query..." << endl;
    }
    closesocket(s);
    WSACleanup();
    return 0;
}
