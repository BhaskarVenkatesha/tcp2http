#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

const int MAX_CLIENTS = 1000;
const int BUFFER_SIZE = 1024;
const int HTTPS_PACKET_SIZE = 1024 * 2;
const int HTTPS_PACKET_SIZE1 = 1024 * 2;

using namespace std;

int receiveData(int socket, char *buffer, size_t totalBytes);

std::string to_string(int v) {
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

int writeToFile(std::string str) {
  ofstream myfile;
  std::string file = std::string("/tmp/DEBUG") + std::string(".txt");
  myfile.open(file.c_str(), std::ios::app);
  myfile << str << "\n";
  myfile.close();
  return 0;
}
int writeToFile1(std::string str) {
  ofstream myfile;

  std::string file = std::string("/tmp/DEBUG1") + std::string(".txt");
  myfile.open(file.c_str(), std::ios::app);
  myfile << str << "\n";
  myfile.close();
  return 0;
}

typedef struct {
  char *packets[50];
  int lengths[50];
  bool lastPacketIncomplete;
  int lastIncompleteLength;
  char *lastIncompletePointer;
  int totalCompletePackets;
} ParseResult;

ParseResult parseBuffer(char *buffer, int length) {
  ParseResult result = { 0 };
  result.lastPacketIncomplete = false;

  int currentIndex = 0;

  while (currentIndex < length) {

    const char *postStart = strstr(buffer + currentIndex, "POST /");
    if (!postStart) {
      result.lastPacketIncomplete = true;
      result.lastIncompleteLength = length - currentIndex;
      result.lastIncompletePointer = buffer + currentIndex;
      break; // No more POST requests found
    }

    const char *headerEnd = strstr(postStart, "\r\n\r\n");
    if (!headerEnd) {
      result.lastPacketIncomplete = true;
      result.lastIncompleteLength = length - currentIndex;
      result.lastIncompletePointer = buffer + currentIndex;
      break; // Incomplete header found
    }

    const char *contentLengthStart = strstr(postStart, "Content-Length:");
    if (!contentLengthStart || contentLengthStart > headerEnd) {
      result.lastPacketIncomplete = true;
      result.lastIncompleteLength = length - currentIndex;
      result.lastIncompletePointer = buffer + currentIndex;
      break; // Content-Length not found or found after headers
    }
    int contentLength = atoi(contentLengthStart + strlen("Content-Length:"));

    const char *dataStart = headerEnd + strlen("\r\n\r\n");

    if (dataStart + contentLength > (buffer + length)) {
      result.lastPacketIncomplete = true;
      result.lastIncompleteLength = length - currentIndex;
      result.lastIncompletePointer = buffer + currentIndex;
      break;
    }

    result.packets[result.totalCompletePackets] = (char *)dataStart;
    result.lengths[result.totalCompletePackets] = contentLength;
    result.totalCompletePackets++;

    currentIndex = dataStart + contentLength - buffer;
  }
  return result;
}

int sendData(int socket, const char *data, size_t totalBytes) {
  size_t bytesSent = 0;

  writeToFile("socket = ");
  writeToFile(to_string(socket));

  while (bytesSent < totalBytes) {
    writeToFile("bytesSent = ");
    writeToFile(to_string(bytesSent));
    writeToFile("socket = ");
    writeToFile(to_string(socket));
    writeToFile("totalBytes = ");
    writeToFile(to_string(totalBytes));
    ssize_t sent = send(socket, data + bytesSent, totalBytes - bytesSent, 0);

    writeToFile("sent = ");
    writeToFile(to_string(sent));
    if (sent == -1) {
      perror("send");
      return -1; // Error sending data
    }

    bytesSent += sent;
  }

  return bytesSent;
}

int main() {


  char *extra[MAX_CLIENTS] = { 0 };
  int sizes[MAX_CLIENTS] = { 0 };

  struct in_addr in;
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    perror("Error creating server socket");
    exit(EXIT_FAILURE);
  }

  int reuse = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse,
                 sizeof(reuse)) < 0) {
    perror("Error setting SO_REUSEADDR option");
    writeToFile("Error setting SO_REUSEADDR option");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  serverAddress.sin_port = htons(8086); // Change port as needed

  if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress),
           sizeof(serverAddress)) == -1) {
    perror("Error binding server socket");
    writeToFile("Error binding server socket");
    exit(EXIT_FAILURE);
  }

  if (listen(serverSocket, MAX_CLIENTS) == -1) {
    perror("Error listening on server socket");
    writeToFile("Error listening on server socket");
    exit(EXIT_FAILURE);
  }

  struct in_addr in1;
  int newserverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (newserverSocket == -1) {
    perror("Error creating server socket");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(newserverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse,
                 sizeof(reuse)) < 0) {
    perror("Error setting SO_REUSEADDR option");
    writeToFile("Error setting SO_REUSEADDR option");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  sockaddr_in serverAddress1;
  serverAddress1.sin_family = AF_INET;
  serverAddress1.sin_port = htons(8088); // Change port as needed

  if (bind(newserverSocket,
           reinterpret_cast<struct sockaddr *>(&serverAddress1),
           sizeof(serverAddress1)) == -1) {
    perror("Error binding server socket");
    writeToFile("Error binding server socket");
    exit(EXIT_FAILURE);
  }

  if (listen(newserverSocket, MAX_CLIENTS) == -1) {
    perror("Error listening on server socket");
    writeToFile("Error listening on server socket");
    exit(EXIT_FAILURE);
  }

  std::vector<pollfd> httpsfds(MAX_CLIENTS + 1); // +1 for the server socket
  httpsfds[0].fd = newserverSocket;
  httpsfds[0].events = POLLIN;
  for (int i = 1; i <= MAX_CLIENTS; ++i) {
    httpsfds[i].fd = -1;
    httpsfds[i].events = 0;
    httpsfds[i].revents = 0;
  }

  std::vector<int> clientSockets1(MAX_CLIENTS, -1); // -1 indicates unused slot

  std::string array[MAX_CLIENTS + 1];
  std::vector<pollfd> fds(MAX_CLIENTS + 1); // +1 for the server socket
  fds[0].fd = serverSocket;
  fds[0].events = POLLIN;
  for (int i = 1; i <= MAX_CLIENTS; ++i) {
    fds[i].fd = -1;
    fds[i].events = 0;
    fds[i].revents = 0;
  }
  pollfd fdlist[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    fdlist[i].fd = -1;
    fdlist[i].events = 0;
    fdlist[i].revents = 0;
  }
  std::vector<int> clientSockets(MAX_CLIENTS, -1); // -1 indicates unused slot

  writeToFile("Ready to accept connections");
  while (true) {
    int ready = poll(fds.data(), 2, 0); // Wait indefinitely for events

    if (ready == -1) {
      perror("Error in poll");
      writeToFile("Error in poll");

      exit(EXIT_FAILURE);
    }

    if (ready > 0) {
      if (fds[0].revents & POLLIN) {
        int newClient = accept(serverSocket, NULL, NULL);
        if (newClient == -1) {
          perror("Error accepting client");

        } else {
          char hostBuffer[1024] = { 0 };
          recv(newClient, hostBuffer, 4, 0);
          unsigned int hostLength = strtol(hostBuffer, NULL, 16);
          memset(hostBuffer, '\0', sizeof(hostBuffer));
          recv(newClient, hostBuffer, hostLength, 0);
          char portBuffer[1024] = { 0 };
          recv(newClient, portBuffer, 4, 0);
          unsigned int portLength = strtol(portBuffer, NULL, 16);
          memset(portBuffer, '\0', sizeof(portBuffer));
          recv(newClient, portBuffer, portLength, 0);
          unsigned int prt = strtol(portBuffer, NULL, 10);

          writeToFile("hostBuffer = ");
          writeToFile(hostBuffer);

          int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
          if (clientSocket == -1) {
            perror("Error creating client socket");
            writeToFile("Error creating client socket");
            exit(EXIT_FAILURE);
          }

          writeToFile("Connecting to proxyB");
          sockaddr_in proxyBAddress;
          proxyBAddress.sin_family = AF_INET;
          proxyBAddress.sin_port = htons(prt); // Change port as needed

          if (inet_pton(AF_INET, hostBuffer, &proxyBAddress.sin_addr) <= 0) {
            perror("Invalid IP address");
            exit(EXIT_FAILURE);
          }

          if (connect(clientSocket,
                      reinterpret_cast<struct sockaddr *>(&proxyBAddress),
                      sizeof(proxyBAddress)) == -1) {
            perror("Error connecting to server");
            writeToFile("Error connecting to server");

            exit(EXIT_FAILURE);
          }

          writeToFile("Connected to proxyB");

          int fd = -1;
          {
            const char *http_request = "GET / HTTP/1.1\r\nHost: "
                                       "opendeploy1.public.232."
                                       "mylab\r\nConnection: close\r\n\r\n";
            send(clientSocket, http_request, strlen(http_request), 0);
            char buffer1[1024] = { 0 };
            ssize_t recv_size;
            recv_size = recv(clientSocket, buffer1, sizeof(buffer1), 0);
            writeToFile("received the data\n");
            writeToFile(buffer1);
          }

          while (true) {
            fd = accept(newserverSocket, NULL, NULL);
            if (fd == -1) {
              perror("Accept failed:");
              writeToFile("Accept failed:");
              continue;
            }
            break;
          }
          {
            char buffer1[1024] = { 0 };
            ssize_t recv_size;
            recv_size = recv(fd, buffer1, sizeof(buffer1), 0);
            writeToFile("\nreceived the data: ");
            writeToFile(buffer1);
            const char *response = "HTTP/1.1 200 OK\r\nContent-Type: "
                                   "text/plain\r\n\r\nHello, World!";
            send(fd, response, strlen(response), 0);
          }

          writeToFile("\n\nThe value of fd for proxyB http client = ");
          writeToFile(to_string(fd));
          writeToFile("Accepted connection from proxyB");

          size_t emptySlot = 1;
          for (; emptySlot < clientSockets.size(); ++emptySlot) {
            if (clientSockets[emptySlot] == -1) {
              break;
            }
          }
          if (emptySlot < clientSockets.size()) {
            clientSockets[emptySlot] = newClient;
            fds[emptySlot].fd = newClient;
            fds[emptySlot].events = POLLIN;

            clientSockets1[emptySlot] = fd;
            httpsfds[emptySlot].fd = fd;
            httpsfds[emptySlot].events = POLLIN;
            //
            writeToFile("emptySlot = ");
            writeToFile(to_string(emptySlot));
            writeToFile("fds[emptySlot].fd = ");
            writeToFile(to_string(fds[emptySlot].fd));

            fdlist[emptySlot].fd = clientSocket;
            fdlist[emptySlot].events = POLLIN | POLLOUT;

            writeToFile("emptySlot = ");
            writeToFile("fdlist[emptySlot].fd = ");
            writeToFile(to_string(fdlist[emptySlot].fd));

            array[emptySlot] = hostBuffer;
          }
        }
        fds[0].revents = 0;
      }

      for (size_t i = 1; i < clientSockets.size(); ++i) {
        if (clientSockets[i] != -1 && (fds[i].revents & POLLIN)) {
          char buffer[BUFFER_SIZE] = { 0 };
          if (clientSockets[i] == -1)
            continue;
          errno = 0;
          int bytesRead = recv(clientSockets[i], buffer, sizeof(buffer), 0);
          if (bytesRead <= 0 && errno != 0) {
            perror("Failed to receive data");
            writeToFile("Failed to receive data");
            close(clientSockets[i]);
            clientSockets[i] = -1;
            fds[i].fd = -1;    // Update fds
            fds[i].events = 0; // Clear events
          } else if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            if (fdlist[i].fd == -1)
              continue;

            writeToFile("\n\n AAA says:");
            writeToFile(buffer);

            if (bytesRead > 4) {
              if (buffer[0] == 'a' && buffer[1] == 'b' && buffer[2] == 'c' &&
                  buffer[3] == 'd') {

                int kl = buffer[4];
                writeToFile("\n\nThe EOF character value is ");
                writeToFile(to_string(kl));
              }
            }

            char *httpRequest = (char *)malloc(HTTPS_PACKET_SIZE1);
            int headerLen = 0;
            if (httpRequest != NULL) {
              for (int k = 0; k < HTTPS_PACKET_SIZE1; ++k) {
                httpRequest[k] = 0;
              }

              char *requestBody =
                  httpRequest +
                  snprintf(httpRequest, HTTPS_PACKET_SIZE1,
                           "POST / HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Content-Type: application/octet-stream\r\n"
                           "Content-Length: %zu\r\n"
                           "\r\n",
                           array[i].c_str(), bytesRead /*, sz*/);
              headerLen = requestBody - httpRequest;
              memcpy(requestBody, buffer, bytesRead);
            } else {
              writeToFile("\n\n FAILED");
              exit(EXIT_FAILURE);
            }

            writeToFile("\n\n AAA says:");
            writeToFile(httpRequest);

            writeToFile("i = ");
            writeToFile(to_string(i));
            writeToFile("Sending data to fdlist[i].fd = ");
            writeToFile(to_string(fdlist[i].fd));

            int bytesSent =
                send(fdlist[i].fd, httpRequest, headerLen + bytesRead, 0);

            if (bytesSent == -1) {
              std::cerr << "Error sending data." << std::endl;
            } else {
              std::cout << "Sent " << bytesSent << " bytes." << std::endl;
            }

            free(httpRequest);

            if (bytesSent == -1) {
              perror("Error sending data to client");
              close(fdlist[i].fd);
              fdlist[i].fd = -1;    // Update fdlist
              fdlist[i].events = 0; // Clear events
            }
          }
          fds[i].revents = 0;
        }
      }
    }

    ready = poll(fdlist, 2, 0); // Wait indefinitely for events

    if (ready == -1) {
      perror("Error in poll");
      exit(EXIT_FAILURE);
    }
    if (ready > 0) {
      for (int k = 1; k < MAX_CLIENTS; ++k) {
        if (fdlist[k].revents & POLLIN) {
          char buffer[HTTPS_PACKET_SIZE1] = { 0 };
          if (fdlist[k].fd == -1)
            continue;
          int bytesRead = recv(fdlist[k].fd, buffer, sizeof(buffer), 0);

          if (bytesRead <= 0 && errno != 0) {
            close(fdlist[k].fd);
            fdlist[k].fd = -1;    // Update fdlist
            fdlist[k].events = 0; // Clear events
            break;
          } else if (bytesRead > 0) {
            buffer[bytesRead] = '\0';

            writeToFile("\n\n AAA DOESNOT sends BACK:");
            writeToFile(buffer);
          }

          fdlist[k].revents = 0;
        }
      }
    }

    ready = poll(httpsfds.data(), 2, 0); // Wait indefinitely for events

    if (ready == -1) {
      perror("Error in poll");
      writeToFile("Error in poll");

      exit(EXIT_FAILURE);
    }

    if (ready > 0) {

      for (size_t i = 1; i < 2 /*clientSockets1.size()*/; ++i) {
        writeToFile("\n\nclientSockets1[1] = ");
        writeToFile(to_string(clientSockets1[1]));
        if (clientSockets1[i] != -1 && (httpsfds[i].revents & POLLIN)) {
          char buffer[HTTPS_PACKET_SIZE1] = { 0 };

          writeToFile("About to receive the HTTP request from proxyB\n");
          if (clientSockets1[i] == -1)
            continue;

          int bytesRead = recv(clientSockets1[i], buffer, sizeof(buffer), 0);
          if (bytesRead <= 0 /*&& errno != 0*/) {
            perror("Failed to receive data");
            writeToFile("Failed to receive data");
            close(clientSockets1[i]);
            clientSockets1[i] = -1;
            httpsfds[i].fd = -1;    // Update fds
            httpsfds[i].events = 0; // Clear events
          } else if (bytesRead > 0) {
            buffer[bytesRead] = '\0';

            writeToFile("AAA received:");
            writeToFile(buffer);

            const char *status_line = "HTTP/1.1 200 OK\r\n";
            char content_length_header[50] = { 0 };
            sprintf(content_length_header, "Content-Length: %zu\r\n", 0);
            const char *content_type_header = "Content-Type: text/plain\r\n";
            char *double_linebreak = "\r\n";

            int sz = strlen(status_line) + strlen(content_type_header) +
                     strlen(content_length_header) + strlen(double_linebreak);

            char *response = (char *)malloc(HTTPS_PACKET_SIZE1);
            if (response != NULL) {
              for (int i = 0; i < HTTPS_PACKET_SIZE1; ++i) {
                response[i] = 0;
              }
            }
            if (!response) {
              perror("Memory allocation failed");
              exit(EXIT_FAILURE);
            }

            char *responseBody =
                response + snprintf(response, HTTPS_PACKET_SIZE1, "%s%s%s%s",
                                    status_line, content_type_header,
                                    content_length_header, double_linebreak);

            writeToFile("\n\n AAA sends BACK ACK:");
            writeToFile(response);

            int bytesSent = send(clientSockets1[i], response, sz, 0);
            free(response);
            if (bytesSent == -1) {
              perror("Error sending ACK back to proxyB");
              close(clientSockets1[i]);
              clientSockets1[i] = -1;
              httpsfds[i].fd = -1;    // Update fds
              httpsfds[i].events = 0; // Clear events
            }

            char *newBuf = buffer;
            int total = sizes[i] + bytesRead;
            bool shouldFree = false;
            if (sizes[i] > 0) {
              newBuf = (char *)malloc(sizes[i] + bytesRead + 1);
              memcpy(newBuf, extra[i], sizes[i]);
              memcpy(newBuf + sizes[i], buffer, bytesRead);
              newBuf[total] = { 0 };
              writeToFile("\nsending to processor: ");
              writeToFile(newBuf);
              free(extra[i]);
              shouldFree = true;
            }
            sizes[i] = 0;

            ParseResult result = parseBuffer((char *)newBuf, total);

            writeToFile("\nresult.totalCompletePackets: ");
            writeToFile(to_string(result.totalCompletePackets));
            for (int j = 0; j < result.totalCompletePackets; j++) {

              char temp[2050] = { 0 };
              memcpy(temp, result.packets[j], result.lengths[j]);
              temp[result.lengths[j]] = 0;
              writeToFile("\n\nData: ");
              writeToFile(temp);
              bytesSent = send(clientSockets[i], result.packets[j],
                               result.lengths[j], 0);

              if (bytesSent == -1) {
                perror("Error sending data to client");
                perror("Error sending data to TMicroDeploy");
                close(clientSockets[i]);
                clientSockets[i] = -1;
                httpsfds[i].fd = -1;    // Update fds
                httpsfds[i].events = 0; // Clear events
              }
            }

            writeToFile("result.lastIncompleteLength: ");
            writeToFile(to_string(result.lastIncompleteLength));

            if (result.lastPacketIncomplete) {
              sizes[i] = result.lastIncompleteLength;
              extra[i] = (char *)malloc(result.lastIncompleteLength);
              memcpy(extra[i], result.lastIncompletePointer,
                     result.lastIncompleteLength);
              writeToFile("\nEXTRA extracted: ");
              writeToFile(extra[i]);

            } else {
              sizes[i] = 0;
            }
          }
        }
      }
    }
  }
  close(serverSocket);
  return 0;
}

int receiveData(int socket, char *buffer, size_t totalBytes) {
  size_t bytesRead = 0;

  writeToFile1("socket = ");
  writeToFile1(to_string(socket));

  while (bytesRead < totalBytes) {
    writeToFile1("bytesRead = ");
    writeToFile1(to_string(bytesRead));
    writeToFile1("socket = ");
    writeToFile1(to_string(socket));
    writeToFile1("totalBytes = ");
    writeToFile1(to_string(totalBytes));
    ssize_t received =
        recv(socket, buffer + bytesRead, totalBytes - bytesRead, 0);

    writeToFile1("received = ");
    writeToFile1(to_string(received));

    if (received <= 0) {
      perror("recv");
      return -1; // Error or connection closed
    }

    bytesRead += received;
  }

  writeToFile1("Returning bytesRead = ");
  writeToFile1(to_string(bytesRead));
  return bytesRead;
}
