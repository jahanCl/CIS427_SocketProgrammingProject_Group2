#ifndef DISCUSSU_SERVER_DATA_STRUCT_H
#define DISCUSSU_SERVER_DATA_STRUCT_H


#include <arpa/inet.h>
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
#include <vector>
#include"utilities.h"

using namespace std;

#define MAX_PENDING 5

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

//What server needs to know about a response before sending it to client.
struct Response {
    vector<string> lines;         //what lines to send
    bool successful = false;      //was command sucessfull
    bool closeConnection = false;  //wheter to close client connection or no(QUIT)
    int resourceId = 0;             //question or answer id of operation
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

        //Predefined questions for testing purposes

    }
    //Checks a username and password and returns the matching account.
    const UserAccount* authenticate(const string& username,const string& password) const {
       //Search by the map's string key.
    auto iterator = accountsList.find(username);

    //Username does not exist.
    if (iterator == accountsList.end()) {
        return nullptr;
    }
    const UserAccount& account = iterator->second; //The map's value is found in UserAccount.

    //Compare the stored password with the submitted password.
    if (account.password != password) {
        return nullptr;
    }
    return &account;  //Return the matching account.
   
    };

    //Add question to question list and assignes it unique number
    Question& addQuestion(const string& author,const string& title,const string& body) {
        //Create a question using the current id, then increment the id and 
        //add user provided fields and leave {} empty for answer
        Question question{ nextQuestionNumber++, author,title,body,{} };

        int number = question.questionNumber;  //get the question number
        questionsList[number] = question;      //add question in map at that number
        return questionsList[number];         //return the question now referenced in map    
    };

    //Allows to look for question in list
    Question* findQuestion(int questionNumber) {
       // Find the map entry whose key matches the question number.
        auto iterator = questionsList.find(questionNumber);

        //If no matching question is found
        if (iterator == questionsList.end()) {
            return nullptr;
        }
    // The question was found. Return a pointer to the Question object
    return &iterator->second;
    }

    //Add answer to question
    Answer addAnswer(Question& question,const string& author,const string& body) {
        //Increment current id so teh answer recive diffent id num
        Answer answer{nextAnswerId++,author,body};

        question.answers.push_back(answer); //Add answer to question answer list
        return answer;    
    }
    //Shows user waht was recored in question
    const map<int, Question>& getQuestions() const {
        return questionsList;
    }
};

//Writes timestamped server events to a log file.
//Example: 2026-10-18T14:30:00 INFO Server listening on 0.0.0.0:6543
class ServerLogger {
private:
    ofstream file; //Used to Write entries to the log file.

    // Creates the current local time in YYYY-MM-DDTHH:MM:SS format.
    string timestamp() {
        time_t now = time(nullptr);   //Get the current calendar time.
        tm localTime{};  //Create structure for the date and time fields.
        localtime_r(&now, &localTime);  //Convert the current time into local time.
        char output[32];   //Temporarily holds formated timestamp

        //Format the local time as, 2026-10-18T14:30:00.
        strftime(output,sizeof(output),"%Y-%m-%dT%H:%M:%S",&localTime);
        return output;  //Convert the charc array into a C++ string and return it.
    }

public:
    //Opens the selected log file when a ServerLogger object is created.
    explicit ServerLogger(const string& fileName) {
        file.open(fileName, ios::app); //Adds new entired
    }
    // Writes one complete log entry.
    void log(const string& level,const string& message) {
        file << timestamp()<< ' '<< level << ' '<< message<< '\n';
        file.flush();  //Force the entry to be written immediately instead of remaining in buffer
    }
};

#endif



