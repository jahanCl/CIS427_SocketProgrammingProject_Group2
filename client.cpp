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
#include"utilities.h"
#include"clientConnection.h"
#include"clientInterface.h"

using namespace std;


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
        //To not loop forever if the user ends input (Ctrl+D), check if getline() succeeded.
        if (!getline(cin, choice)) {
            cout << "\nInput ended. Closing client.\n";
            break;
        }

        bool validChoice = false; // Assume the choice is invalid.
        if (authenticated) {    //Logged in users have more menu options.
        validChoice =
            choice == "1" || choice == "2" ||
            choice == "3" || choice == "4" ||
            choice == "5" || choice == "6";
        }
        else {     //Not logged in users have only two menu options.
            validChoice = choice == "1" || choice == "2";
        }       

        if (!validChoice) {
            cout << "Invalid menu selection. Please try again.\n";
            continue;
        }
        
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

            if (request.size() + 1 > MAX_LINE) {
                cout << "Question is too long. Please enter shorter text.\n";
                continue; // Return to the main menu.
            }
            
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
            // Reject oversized input locally without closing the connection.
            // Add 1 to include the newline that sendLine() adds.
            if (request.size() + 1 > MAX_LINE) {
                cout << "Question is too long. Please enter shorter text.\n";
                continue; // Return to the main menu.
            }

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
            if (request.size() + 1 > MAX_LINE) {
                cout << "Question is too long. Please enter shorter text.\n";
                continue; // Return to the main menu.
            }

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