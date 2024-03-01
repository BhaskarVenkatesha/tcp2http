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

int main() {

    char *extServerPortStr = getenv("EXTERNAL_SERVER_PORT");

    if (extServerPortStr == NULL) {
        fprintf(stderr, "EXTERNAL_SERVER_PORT environment variable not set\n");
        return 1;
    }

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
    char *extServerHostStr = getenv("EXTERNAL_SERVER_HOST");

    if (extServerHostStr == NULL) {
        fprintf(stderr, "EXTERNAL_SERVER_PORT environment variable not set\n");
        return 1;
    }







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
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
 // serverAddress.sin_addr.s_addr = INADDR_ANY;
  inet_pton(AF_INET, extServerHostStr, &(serverAddress.sin_addr));
  serverAddress.sin_port = htons(serverPort); // Change port as needed

  if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress),
           sizeof(serverAddress)) == -1) {
    perror("Error binding server socket");
    exit(EXIT_FAILURE);
  }

  if (listen(serverSocket, MAX_CLIENTS) == -1) {
    perror("Error listening on server socket");
    exit(EXIT_FAILURE);
  }
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

  pollfd fdlist1[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    fdlist1[i].fd = -1;
    fdlist1[i].events = 0;
    fdlist1[i].revents = 0;
  }

  std::vector<int> clientSockets(MAX_CLIENTS, -1); // -1 indicates unused slot

  writeToFile1("Started listening on server socket");
  int external_server_socket = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in external_server_address;
  external_server_address.sin_family = AF_INET;
  external_server_address.sin_port = htons(8088);
  inet_pton(AF_INET, "10.96.37.17", &(external_server_address.sin_addr));

  while (true) {
    int ready = poll(fds.data(), 2, 0); // Wait indefinitely for events

    if (ready == -1) {
      perror("Error in poll");
      writeToFile1("Error in poll");
      exit(EXIT_FAILURE);
    }

    if (ready > 0) {
      if (fds[0].revents & POLLIN) {
        writeToFile1("Received connection request");
        int newClient = accept(serverSocket, NULL, NULL);
        writeToFile1("Accepted\n");
        writeToFile1("newClient = ");
        writeToFile1(to_string(newClient));
        if (newClient == -1) {
          perror("Error accepting client");
        } else {

          {
            char buffer1[1024] = { 0 };
            ssize_t recv_size;
            recv_size = recv(newClient, buffer1, sizeof(buffer1), 0);
            writeToFile1("\nreceived the data: ");
            writeToFile1(buffer1);
            const char *response = "HTTP/1.1 200 OK\r\nContent-Type: "
                                   "text/plain\r\n\r\nHello, World!";
            send(newClient, response, strlen(response), 0);
          }

          writeToFile1("About  to  connect to external server");

          while (connect(external_server_socket,
                         (struct sockaddr *)&external_server_address,
                         sizeof(external_server_address)) == -1) {
            perror("Error connecting to external server");
            writeToFile1("Error connecting to external server");
            sleep(1);
            //        exit(EXIT_FAILURE);
          }

          {
            const char *http_request = "GET / HTTP/1.1\r\nHost: "
                                       "opendeploy1.public.232."
                                       "mylab\r\nConnection: close\r\n\r\n";
            send(external_server_socket, http_request, strlen(http_request), 0);
            char buffer1[1024] = { 0 };
            ssize_t recv_size;
            recv_size =
                recv(external_server_socket, buffer1, sizeof(buffer1), 0);
            writeToFile1("received the data\n");
            writeToFile1(buffer1);
          }
          writeToFile1("Connected to external server");

          int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
          if (clientSocket == -1) {
            perror("Error creating client socket");
            writeToFile1("Error creating client socket");

            exit(EXIT_FAILURE);
          }

          sockaddr_in proxyBAddress;
          proxyBAddress.sin_family = AF_INET;
          proxyBAddress.sin_addr.s_addr = INADDR_ANY;

          writeToFile1("Connecting to 20014");

          proxyBAddress.sin_port = htons(20014);
          if (connect(clientSocket,
                      reinterpret_cast<struct sockaddr *>(&proxyBAddress),
                      sizeof(proxyBAddress)) == -1) {
            perror("Error connecting to server");
            writeToFile1("Error connecting to server");

            exit(EXIT_FAILURE);
          }
          writeToFile1("connected");
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
            fdlist[emptySlot].fd = clientSocket;
            fdlist[emptySlot].events = POLLIN | POLLOUT;

            fdlist1[emptySlot].fd = external_server_socket;
            fdlist1[emptySlot].events = POLLIN | POLLOUT;
          }
        }
        fds[0].revents = 0;
      }
      for (size_t i = 1; i < clientSockets.size(); ++i) {
        if (clientSockets[i] != -1 && (fds[i].revents & POLLIN)) {
          char buffer[HTTPS_PACKET_SIZE1] = { 0 };
          if (clientSockets[i] == -1)
            continue;

          writeToFile1("About to receive data");
          writeToFile1("clientSockets[i] = ");
          writeToFile1(to_string(clientSockets[i]));
          int bytesRead = recv(clientSockets[i], buffer, HTTPS_PACKET_SIZE1, 0);

          if (bytesRead > 0 /*&& errno != 0*/) {
            writeToFile1("Received following bytes of data");
            writeToFile1(to_string(bytesRead));
          } else {
            writeToFile1("ERROR receiving data");
            perror("ERROR ");
          }

          if (bytesRead <= 0 /*&& errno != 0*/) {
            clientSockets[i] = -1;
            fds[i].fd = -1;    // Update fds
            fds[i].events = 0; // Clear events
          } else if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            if (fdlist[i].fd == -1)
              continue;

            writeToFile1("\n\n BBB says:");
            writeToFile1(buffer);

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

            writeToFile1("\n\n BBB sends BACK ACK:");
            writeToFile1(response);

            int bytesSent = send(clientSockets[i], response, sz, 0);
            free(response);
            if (bytesSent == -1) {
              perror("Error sending data to client");
              close(clientSockets[i]);
              clientSockets[i] = -1;
              fds[i].fd = -1;    // Update fds
              fds[i].events = 0; // Clear events
            }

            char *newBuf = buffer;
            int total = sizes[i] + bytesRead;
            bool shouldFree = false;
            if (sizes[i] > 0) {
              newBuf = (char *)malloc(sizes[i] + bytesRead + 1);
              memcpy(newBuf, extra[i], sizes[i]);
              memcpy(newBuf + sizes[i], buffer, bytesRead);
              newBuf[total] = { 0 };
              writeToFile1("\nsending to processor: ");
              writeToFile1(newBuf);
              free(extra[i]);
              shouldFree = true;
            }
            sizes[i] = 0;

            ParseResult result = parseBuffer((char *)newBuf, total);

            writeToFile1("\nresult.totalCompletePackets: ");
            writeToFile1(to_string(result.totalCompletePackets));

            for (int j = 0; j < result.totalCompletePackets; j++) {

              char temp[2050] = { 0 };
              memcpy(temp, result.packets[j], result.lengths[j]);
              temp[result.lengths[j]] = 0;
              writeToFile1("\n\nData: ");
              writeToFile1(temp);
              bytesSent =
                  send(fdlist[i].fd, result.packets[j], result.lengths[j], 0);

              if (result.lengths[j] > 4) {
                if (result.packets[j][0] == 'a' &&
                    result.packets[j][1] == 'b' &&
                    result.packets[j][2] == 'c' &&
                    result.packets[j][3] == 'd') {

                  int kl = result.packets[j][4];
                  writeToFile1("\n\nThe EOF character value is ");
                  writeToFile1(to_string(kl));
                }
              }

              writeToFile1(
                  "return value bytesSent from send to TSessionListener: ");
              writeToFile1(to_string(bytesSent));

              if (bytesSent != result.lengths[j]) {
                writeToFile1("ERROR sending data.");
              } else {
                writeToFile1("Bytes sent to TSessionListener: ");
                writeToFile1(to_string(result.lengths[j]));
              }

              if (bytesSent == -1) {
                perror("Error sending data to client");
                close(fdlist[i].fd);
                fdlist[i].fd = -1;    // Update fdlist
                fdlist[i].events = 0; // Clear events
              }
            }
            writeToFile1("result.lastIncompleteLength: ");
            writeToFile1(to_string(result.lastIncompleteLength));

            if (result.lastPacketIncomplete) {
              sizes[i] = result.lastIncompleteLength;
              extra[i] = (char *)malloc(result.lastIncompleteLength);
              memcpy(extra[i], result.lastIncompletePointer,
                     result.lastIncompleteLength);
              writeToFile1("\nEXTRA extracted: ");
              writeToFile1(extra[i]);

            } else {
              sizes[i] = 0;
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
          char buffer[BUFFER_SIZE] = { 0 };
          if (fdlist[k].fd == -1)
            continue;
          int bytesRead = recv(fdlist[k].fd, buffer, sizeof(buffer), 0);
          if (bytesRead <= 0 && errno != 0) {
            close(fdlist[k].fd);
            fdlist[k].fd = -1;    // Update fdlist
            fdlist[k].events = 0; // Clear events
            close(fds[k].fd);
            fds[k].fd = -1;    // Update fdlist
            fds[k].events = 0; // Clear events
            close(clientSockets[k]);
            clientSockets[k] = -1;
            break;
          } else if (bytesRead > 0) {
            buffer[bytesRead] = '\0';

            writeToFile1("\n\n BBB sends BACK:");
            writeToFile1(buffer);

            char *httpRequest = (char *)malloc(HTTPS_PACKET_SIZE1);
            int headerLen = 0;
            if (httpRequest != NULL) {
              for (int kl = 0; kl < HTTPS_PACKET_SIZE1; ++kl) {
                httpRequest[kl] = 0;
              }

              char *requestBody =
                  httpRequest +
                  snprintf(httpRequest, HTTPS_PACKET_SIZE1,
                           "POST / HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "Content-Type: application/octet-stream\r\n"
                           "Content-Length: %zu\r\n"
                           "\r\n",
                           bytesRead /*, sz*/);
              headerLen = requestBody - httpRequest;
              memcpy(requestBody, buffer, bytesRead);
            } else {
              writeToFile("\n\n FAILED");
              exit(EXIT_FAILURE);
            }

            writeToFile1("\n\n BBB sends BACK:");
            writeToFile1(httpRequest);

            int bytesSent =
                send(fdlist1[k].fd, httpRequest, headerLen + bytesRead, 0);
            free(httpRequest);

            if (bytesSent == -1) {
              perror("Error sending data to client");
              close(fdlist1[k].fd);
              fdlist1[k].fd = -1;    // Update fdlist
              fdlist1[k].events = 0; // Clear events
            }
          }
          fdlist[k].revents = 0;
        }
      }
    }

    ready = poll(fdlist1, 2, 0); // Wait indefinitely for events

    if (ready == -1) {
      perror("Error in poll");
      exit(EXIT_FAILURE);
    }

    for (int k = 1; k < MAX_CLIENTS; ++k) {
      if (fdlist1[k].revents & POLLIN) {
        char buffer[HTTPS_PACKET_SIZE1] = { 0 };
        if (fdlist1[k].fd == -1)
          continue;
        int bytesRead = recv(fdlist1[k].fd, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0 /*&& errno != 0*/) {
          close(fdlist1[k].fd);
          fdlist1[k].fd = -1;    // Update fdlist
          fdlist1[k].events = 0; // Clear events
                                 //      close(fds[k].fd);
                                 //     fds[k].fd = -1;    // Update fdlist
                                 //    fds[k].events = 0; // Clear events
                                 //  close(clientSockets[k]);
                                 // clientSockets[k] = -1;
          break;
        } else if (bytesRead > 0) {
          buffer[bytesRead] = '\0';

          writeToFile1("\n\n BBB received ACK response :");
          writeToFile1(buffer);
        }
      }
    }
  }

  close(serverSocket);
  return 0;
}
