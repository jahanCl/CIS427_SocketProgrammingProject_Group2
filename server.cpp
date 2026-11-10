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

#include <csignal>    // SIGINT and sig_atomic_t.
#include <sys/select.h> // select() for checking socket readiness.
#include <sys/time.h> // Provides timeval.
#include <atomic>

#include <unistd.h>
#include <vector>
#include"utilities.h"
#include"serverLogic.h"
#include <mutex> //for locks

using namespace std;

#define MAX_PENDING 5

// A signal-safe flag indicating that the server should stop.
volatile sig_atomic_t shutdownRequested = 0;

// Ctrl+C calls this function. Only set the flag here.
void requestShutdown(int) {
    shutdownRequested = 1;
}


// ============================================================
// Server Socket Handler
// ============================================================
// Handles one client until QUIT, disconnection, or server shutdown.
void handleClient(
    int clientSocket,
    const string& clientId,
    DiscussionBoard& board,
    ServerLogger& logger,
    mutex& boardMutex
) {
    Session session;
    string receiveBuffer;

    char incoming[RECEIVE_CHUNK];

    bool gracefulDisconnect = false;
    bool discardingOversizedLine = false;

    // Wait at most one second per receive or send operation.
    // A receive timeout lets the handler recheck shutdownRequested.
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt( clientSocket,SOL_SOCKET,SO_RCVTIMEO,&timeout,
        sizeof(timeout)) < 0 ||
        setsockopt(clientSocket,SOL_SOCKET,SO_SNDTIMEO, &timeout,
        sizeof(timeout)) < 0) {

        logger.log("ERROR",clientId + " could not set socket timeouts");
        close(clientSocket);
        return;
    }

    // ONE outer receive loop.
    while (!shutdownRequested && !gracefulDisconnect) {
        ssize_t bytesReceived = recv(
            clientSocket,
            incoming,
            sizeof(incoming),
            0
        );

        if (bytesReceived == 0) {
            logger.log("WARN",clientId + " disconnected unexpectedly");
            break;
        }

        if (bytesReceived < 0) {
            if (errno == EINTR) {
                continue; // Recheck shutdownRequested.
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, not a disconnected client.
            }

            logger.log(
                "WARN", clientId + " receive error=" + string(strerror(errno))
            );
            break;
        }

        // Retain bytes between recv() calls.
        receiveBuffer.append(
            incoming,
            static_cast<size_t>(bytesReceived)
        );

        // Process complete lines already in the buffer.
        while (!shutdownRequested) {
            size_t newline = receiveBuffer.find('\n');

            // Discard the remainder of a previously oversized line.
            if (discardingOversizedLine) {
                if (newline == string::npos) {
                    receiveBuffer.clear();
                    break;
                }

                receiveBuffer.erase(0, newline + 1);
                discardingOversizedLine = false;
                continue;
            }

            if (newline == string::npos) {
                if (receiveBuffer.size() >= MAX_LINE) {
                    logger.log(
                        "WARN", clientId + " protocol error=message too large"
                    );

                    if (!sendAll(clientSocket,"413 message too large\n")) {
                        session.logout();
                        close(clientSocket);
                        logger.log("WARN", clientId + " closed after send failure");
                        return;
                    }
                    receiveBuffer.clear();
                    discardingOversizedLine = true;
                }

                break; // Wait for more bytes.
            }

            size_t framedSize = newline + 1;
            string request = receiveBuffer.substr(0, newline);
            receiveBuffer.erase(0, framedSize);

            Response response;
            string command = "INVALID";

            if (framedSize > MAX_LINE) {
                response = makeError("413 message too large");
            }
            else if (!isASCii(request)) {
                response = makeError("415 non-ASCII content");
            }
            else {
                command = request.empty()? "EMPTY": split(request, '|').front();
                // Don't log the entire request: LOGIN contains a password.
                logger.log("INFO", clientId + " request=" + command);

                try {
                    // Lock only board processing and response construction.
                    lock_guard<mutex> lock(boardMutex);

                    response = processRequest(request, session, board);
                }
                catch (...) {
                    response = makeError("500 internal server error");
                }
                // boardMutex has been released here.
            }

            // Do not hold the board mutex during network sending.
            if (!sendResponse(clientSocket, response)) {
                logger.log("WARN", clientId + " send failed");
                session.logout();
                close(clientSocket);
                logger.log("WARN", clientId + " closed after send failure");
                return;
            }

            string logMessage = clientId + " command=" + command +
                " outcome=" + (response.successful ? "success" : "error");

            if (!response.successful && !response.lines.empty()) {
                logMessage += " response=" + response.lines.front();
            }

            logger.log(response.successful ? "INFO" : "WARN", logMessage
            );

            if (response.closeConnection) {
                gracefulDisconnect = true;
                break;
            }
        }
    }

    // Release this client's connection resources.
    session.logout();
    close(clientSocket);

    if (shutdownRequested) {
        logger.log("INFO",clientId + " closed during server shutdown"
        );
    }
    else if (gracefulDisconnect) {
        logger.log("INFO",clientId + " disconnected gracefully"
        );
    }
}


int main(int argc, char* argv[]) {
    //Signal Handler for Ctrl+C
    struct sigaction action{};
    action.sa_handler = requestShutdown;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0; // Do not automatically restart interrupted system calls.

    if (sigaction(SIGINT, &action, nullptr) < 0) {
        cerr << "Unable to install shutdown handler: "
             << strerror(errno) << '\n';
        return 1;
    }
    // Failed sends should return errors rather than terminate the server.
    signal(SIGPIPE, SIG_IGN);

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
    mutex boardMutex; // Shared by every client-handler thread.

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
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr =htonl(INADDR_ANY);

    serverAddress.sin_port = htons(static_cast<uint16_t>(port));

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
    //Keep threads so they can be joined before destroying shared objects.
    vector<thread> clientThreads;

    while (!shutdownRequested) {
        // Wait for a connection for up to one second.
        fd_set readySockets;
        FD_ZERO(&readySockets);

        // select() cannot represent descriptors outside this range.
        if (serverSocket >= FD_SETSIZE) {
            logger.log("ERROR", "Listening socket exceeds select() capacity");
            break;
        }

        FD_SET(serverSocket, &readySockets);
        timeval waitTime{};
        waitTime.tv_sec = 1;
        waitTime.tv_usec = 0;

        int ready = select(
            serverSocket + 1,
            &readySockets,
            nullptr,
            nullptr,
            &waitTime
        );

        if (ready < 0) {
            if (errno == EINTR) {
                continue; // Ctrl+C may interrupt select().
            }

            logger.log("ERROR","select failed=" + string(strerror(errno)));
            break;
        }

        if (ready == 0 || shutdownRequested) {
            continue; // Timeout or shutdown: recheck the loop condition.
        }

        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength
        );

        if (clientSocket < 0) {
            if (errno == EINTR) {
                continue;
            }

            logger.log("ERROR","accept failed=" + string(strerror(errno)));
            break;
        }

        // Shutdown might have been requested during accept().
        if (shutdownRequested) {
            close(clientSocket);
            break;
        }

        string clientId = "client-" + to_string(nextClientNumber++);

        char addressText[INET_ADDRSTRLEN]{};
        if (inet_ntop(
                AF_INET,
                &clientAddress.sin_addr,
                addressText,
                sizeof(addressText)) == nullptr) {
            strcpy(addressText, "unknown");
        }

        logger.log("INFO", clientId + " connected from " + string(addressText));

        try {
            clientThreads.emplace_back(
                handleClient,
                clientSocket,
                clientId,
                ref(board),
                ref(logger),
                ref(boardMutex)
            );
        }
        catch (const exception& error) {
            close(clientSocket);
            logger.log("ERROR","Could not create client thread: " + string(error.what()));
        }
    }

    //Stop handlers if the accept loop ended because of an error.
    shutdownRequested = 1;

    close(serverSocket); // Stop accepting connections.
    logger.log("INFO", "Server shutdown requested");

    // Wait for handlers to finish before board/logger/mutex destruction.
    for (thread& clientThread : clientThreads) {
        if (clientThread.joinable()) {
            clientThread.join();
        }
    }

    logger.log("INFO", "Server shutdown");
    cout << "\nServer shut down gracefully.\n";

    return 0;
}


