#include <stdio.h>

void closeServerPort(int sockfd);
void recv_send_data(int new_sockfd);
int parseAndOperate(char* text, int infoLength);
int myServer();
