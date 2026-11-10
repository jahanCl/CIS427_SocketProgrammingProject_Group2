#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#include <string>
#include "utilities.h"
#include "clientConnection.h"

using namespace std;

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


// Receives and displays one complete server response.
bool receiveResponse(int socketNumber,string& receiveBuffer,bool mayBeMultiline) {
    optional<string> firstLine =receiveLine(socketNumber,receiveBuffer);

    if (!firstLine.has_value()) {     // Network or framing error occurred.
        return false;
    }

    // Display a friendly message instead of the raw successful response.
    if (firstLine->rfind("200 OK|answerId=", 0) == 0) {
        cout << "Answer posted successfully.\n";
        }
    else if (firstLine->rfind("200 OK|questionNumber=", 0) == 0) {
        cout << "Question posted successfully.\n";
    }
    else if (*firstLine != "200 OK") {
        cout << *firstLine << '\n'; // Continue displaying error responses.
    }

    // If the response is not a successful multiline response, return.
    if (!mayBeMultiline ||firstLine->rfind("200 OK", 0) != 0) {
        return true;                  
    }

    while (true) {                     
        // Continue through successful multiline data.
        optional<string> line =receiveLine( socketNumber,receiveBuffer);
        if (!line.has_value()) {return false;}

        cout << *line << '\n';   //Display COUNT, QUESTION, ANSWER, or END.

        if (*line == "END") {  //------- marks the response boundary.
            return true;
        }
    }
}
#endif