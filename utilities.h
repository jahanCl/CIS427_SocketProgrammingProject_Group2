//Shared functions of server and client

#ifndef DISCUSSU_UTILITIES_H
#define DISCUSSU_UTILITIES_H

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <mutex> // Provides mutex and lock_guard.


using namespace std;

#define MAX_LINE 4096        // Maximum bytes in one protocol line.
#define RECEIVE_CHUNK 1024 // Number of bytes requested per recv().

// ============================================================
// Input Validators
// ============================================================
//Returns true if every character in a string is an ASCII character.
inline bool isASCii(const string& text){     
    for (unsigned char character : text) {   //loop through evrey byte in string 
        if (character > 127 ) {   //if the byte is not in the ASCII range
                return false;   //return false
        }
    }
    return true;
}

//Validates one user-provided field such as a username, title, or answer.
inline bool validField(const string& text) {
    return !text.empty() &&                  //Required fields cannot be empty.
           isASCii(text) &&                  //Only ASCII text is accepted.
           text.find('|') == string::npos && // | is only for seperating fields.
           text.find('\n') == string::npos ; //Newline not inside a protocol message, only at end.
}

inline bool parsePositiveInteger(const string& text,int& output) {
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

//Sends every byte in a string.
inline bool sendAll( int socketNumber, const string& data) {
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

#endif