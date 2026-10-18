Executable Intructions
1.Open a terminal in the project directory and remove old compiled files:
make clean 

2.In same terminal, compile client and server:
make

3.In same terminal start server:
./server 6543

The server should display:
Server listening on port 6543

4.Open a second terminal and start client:
./client localhost 6543

The client should display:
1. Login
2. Quit
Selection: