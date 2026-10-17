#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <map>
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
    struct sockaddr_in sin;
    char buf[MAX_LINE];
    int buf_len;
    socklen_t addr_len;
    int s, new_s;

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family= AF_INET;
    sin.sin_addr.s_addr=INADDR_ANY;
    sin.sin_port=htons(SERVER_PORT);

    if((s=socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("simplex-talk: socket");
        exit(1);
    }
    if((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0){
        perror("simplex-talk: bind");
        exit(1);
    }
    listen(s, MAX_PENDING);

    while(1){
        if((new_s= accept(s,(struct sockaddr *)&sin, &addr_len)) < 0){
            perror("simplex-talk: accept");
            exit(1);
        }
        while(buf_len=recv(new_s, buf, sizeof(buf), 0))
            fputs(buf, stdout);
        close(new_s);
    }
}
