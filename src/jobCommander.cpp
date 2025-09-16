#include <netdb.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

using namespace std;
// Function for opening a socket. The name of server resolved to IP as needed
int open_socket(const char *serverName, int portNum)
{
    int sock = 0;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints); // to ensure that all the fields in the struct are set to a known value before they are used
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Convert the server name and port number to a socket address
    if (getaddrinfo(serverName, to_string(portNum).c_str(), &hints, &res) != 0)
    {
        cerr << "Failed to resolve server name";
        return -1;
    }

    // Create a socket
    if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
    {
        cerr << "Socket creation failed";
        freeaddrinfo(res);
        return -1;
    }

    // Connect to the server
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        cerr << "Connection failed";
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    return sock;
}

void close_socket(int sock)
{
    close(sock);
}

int main(int argc, char const *argv[])
{

    /*INPUT ERROR HANDLING */
    if (argc < 4)
    {
        cerr << "Usage: ./jobCommander [serverName] [portNum] [InputMessageToServer]..." << endl;
        return -1;
    }

    if (argv[1] == nullptr || strlen(argv[1]) == 0)
    {
        cerr << "Error: Server name is not provided or is empty." << endl;
        return -1;
    }

    char *endptr;
    int portNum = (int)strtol(argv[2], &endptr, 10);
    if (argv[2] == nullptr || strlen(argv[2]) == 0 || *endptr != '\0' || portNum <= 0 || portNum > 65535)
    {
        cerr << "Error: Invalid port number. The port number must be a positive integer between 1 and 65535." << endl;
        return -1;
    }
    /*ENDING OF INPUT ERROR HANDLING */
    int sock = open_socket(argv[1], atoi(argv[2]));
    if (sock < 0)
    {
        return -1;
    }
    // cout << "CLIENT SOCKET IS " << sock << endl;
    char buffer[1024] = {0};
    // Concatenate all the arguments from argv[3] to the end into a single string
    string messageToServer = argv[3];
    for (int i = 4; i < argc; i++)
    {
        messageToServer += " ";
        messageToServer += argv[i];
    }
    // Send the command to the server
    send(sock, messageToServer.c_str(), messageToServer.length(), 0);

    // Receive a response from the server

    read(sock, buffer, 1024);
    cout << "Message received: " << buffer << endl
         << endl;

    /* HERE ACTUALLY I ATTEMPTED RECEIVIN THE OUPUT OF THE JOB FROM SERVER BUT THE FUNCIONALITY AT THE SERVER DID NOT WORKING WELL */

    // if (strcmp(argv[3], "issueJob") == 0){
    //     memset(buffer, 0, 1024); // clear buffer
    //     read(sock, buffer, 1024);
    //     cout <<  "-----jobID output start------ " << endl <<endl<< buffer << endl<<"-----jobID output end------ "<<endl;
    // }
    close_socket(sock);

    return 0;
}
