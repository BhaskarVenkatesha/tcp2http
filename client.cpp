#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8086
#define SERVER_IP "10.96.37.17"  // Change to the actual server IP address

void receive_and_print(int client_socket) {
    char buffer[1024];

    // Receive and print messages until the server closes the connection


for(int i = 0 ; i < 13; i++) 
{
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break;  // Server closed the connection
        }

        buffer[bytes_received] = '\0';  // Null-terminate the received data
        printf("%s\n", buffer);
}

const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

        for (int i = 0; i < 7; i++) {
            send(client_socket, days[i], strlen(days[i]), 0);
            sleep(1);  // Add a small delay for demonstration purposes
        }

        // Close the client socket
        close(client_socket);
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Create a socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

	char *buf = "10.96.37.17";
int s = strlen(buf);
    send(client_socket, "000b", 4, 0);
    send(client_socket, buf, strlen(buf), 0);
    send(client_socket, "0004", 4, 0);
    send(client_socket, "8087", 4, 0);

    printf("Connected to the server\n");

    // Receive and print messages from the server
    receive_and_print(client_socket);

    // Close the client socket
    close(client_socket);

    return 0;
}
