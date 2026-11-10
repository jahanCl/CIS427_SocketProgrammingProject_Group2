DiscussU — Class Discussion Board

*Project Name and Purpose*
DiscussU is a command-line TCP client-server application that lets students ask and answer course-related questions. Students can log in, post questions, view existing discussions, and contribute answers.The server maintains the shared discussion board and uses one handler thread per connected client. Shared board operations and log-file writes are protected by mutexes.

*Team Members*:
Kieran Fix 
Jahan Clare

*Required Software*: 
-Linux or a compatible POSIX environment 
-A C++17-compatible compiler, such as g++
-GNU MakePython 3 if running Python test scripts included with the submission

Check the build tools:
g++ --version
make --version

*Build Instructions*
1.Open a terminal in the project’s root directory.
2.Remove old executables and object files:
make clean
3.Build the client and server:
make
4.The build creates:
client
server

*Server Startup Instructions*
1.Start the server with a configurable listening port:
./server <port>
Example:./server 6543
Expected output: Server listening on port 6543

2.Leave this terminal open while using the application.To shut down the server, press Ctrl+C in the server terminal. 

*Client Startup Instructions*
1.Open another terminal in the same project directory but differnt terminal
2.Start the client using the server hostname or IP address and matching port:  
./client <hostname-or-IP> <port>
Example:./client localhost 6543
For three simultaneous clients, open three client terminals and run the command in each. 

Command-Line Arguments
Server./server <port>: 
port is an integer from 1 through 65535. 

Client./client <hostname-or-IP> <port>
hostname-or-IP: Address of the computer running the server.
port: The server’s configured listening port.

User accounts are predefined in serverDataStruct.h. Use the classroom test credentials configured there. Usernames and passwords are case-sensitive.

*Example Interaction*
After connecting, the client displays:
1. Login
2. Quit

Selection:Select Login and enter a configured account’s username and password.
(User accounts are predefined in serverDataStruct.h. Use the test credentials configured there. Usernames and passwords are case-sensitive.)

After successful login, client displays:
Discussion Board Menu:
1. View questions
2. View question and answers
3. Post question
4. Post answer
5. Logout
6. Quit
Selection:

Example question posting:
Selection: 3
Title: TCP framing
Question: Why can one message arrive in multiple reads?
Question posted successfully.

Select option 1 to find the assigned question number. Select option 4 to answer it, then option 2 to view the question and its answers.LOGOUT ends authentication but leaves the TCP connection open. QUIT closes the client connection.The protocol’s internal END marker identifies the end of a multiline response and is not displayed to the user.

*Test Instructions*
Normal Operations
-Start the server.
-Connect a client and log in.
-Post a question.
-View the question list.
-Post an answer to an existing question.View that question and its answers.
-Log out.
-Quit.

Concurrent Operations
-Connect and log in through three client terminals.
-Leave one client idle while the others perform operations.
-Have two clients post different questions close together.
-Confirm that both questions are retained with different IDs.
-Have two clients answer the same question close together.
-Confirm that both answers are retained and the answer count is correct.

Error Recovery
Test:
-Incorrect login credentials
-Invalid menu choices
-Invalid question numbers
-Requests for nonexistent questions
-Empty required inputInvalid port arguments
-Connection attempts while the server is stopped

Disconnection and Shutdown
-Terminate one client unexpectedly and verify that other clients continue working.-Press Ctrl+D at client prompts and verify that the client does not loop indefinitely.
-Shut down the server with three clients connected.
-Restart the server on the same port and verify that a new client can connect.

Framing and Load Testing
-Build the server, then run the included protocol tests from the project directory:
make
tests/protocol_tests.py
-The script starts its own local server and checks fragmented requests, multiple
requests sent together, and concurrent question posts. It uses only Python's
standard library and does not modify the project's server log.

Server Logging
-The server writes timestamped events to: server.log
-Entries record startup, listening address and port, client identifiers, connections, requests, outcomes, errors, disconnections, and shutdown.Inspect recent entries with: tail -n 30 server.log

*Known Limitations*
-Questions and answers are stored in memory and are lost when the server restarts.-Accounts are predefined; registration and password changes are not supported.-Each protocol line is limited to 4,096 bytes, including its newline.
-Updates use request-response communication; users must request the latest questions and answers.
-One thread is created per client, which consumes more resources as connection counts increase.
-Board operations use a shared mutex, so clients briefly take turns accessing board data.
-Completed handler thread objects are retained until server shutdown.
-Socket timeouts apply to individual operations and do not provide a strict total shutdown-time guarantee.

*Attribution*
DiscussU uses: 
-The C++17 standard libraryPOSIX TCP socket APIs provided by the operating system. 
-Course provided socket programming examples as references
-U-M GPT for explanations, initial code assistance, and documentation support.The team reviewed, modified, and tested AI-assisted code for the project. 