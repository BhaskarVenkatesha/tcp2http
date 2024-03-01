#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


void handle_client(int client_socket) {
    // Receive and print messages until the client closes the connection
    char buffer[1024] = "Tell me the names of the week";
    int bytes_received;

    // Server replies with names of the months
    const char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    for (int i = 0; i < 12; i++) {
        send(client_socket, months[i], strlen(months[i]), 0);
        sleep(1);  // Add a small delay for demonstration purposes
    }

 
    // Receive the client's response to the server's months
    send(client_socket, buffer, sizeof(buffer), 0);


    // Server asks for names of days in a week

    // Receive and print messages until the client closes the connection
    const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    for (int i = 0; i < 7; i++) {
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break;  // Client closed the connection
        }

        buffer[bytes_received] = '\0';  // Null-terminate the received data
        printf("Client replied: %s\n", buffer);
    }

    // Close the client socket
    close(client_socket);
}

int main() {




    char *serverPortStr = getenv("SERVER_PORT");

    if (serverPortStr == NULL) {
        fprintf(stderr, "SERVER_PORT environment variable not set\n");
        return 1;
    }
    int serverPort = atoi(serverPortStr);

    char *serverHostStr = getenv("SERVER_HOST");

    if (serverHostStr == NULL) {
        fprintf(stderr, "SERVER_PORT environment variable not set\n");
        return 1;
    }




    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create a socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(serverPort);
    //server_addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, serverHostStr, &(server_addr.sin_addr));

    // Bind the socket to the specified port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", serverPort);

    while (1) {
        // Accept a connection from a client
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Handle the client request
        handle_client(client_socket);
    }

    // Close the server socket (never reached in this example)
    close(server_socket);

    return 0;
}
