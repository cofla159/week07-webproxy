#include <stdio.h>
#include <string.h>
#include "./tiny/csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int get_request(int fd, rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver);
int request_to_server(char *method, char *uri, char *version, char *headers, char *endserver, char *response);
void send_response();
void read_request(rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver);
void make_headers(char *headers);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], headers[MAXLINE], endserver[MAXLINE], response[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  rio_t rio;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from host: (%s, %s)\n", hostname, port);

    Rio_readinitb(&rio, connfd);

    if (get_request(connfd, &rio, method, uri, version, headers, endserver) < 0) // 굳이 version을 받아올 의미가 있나? 어차피 다 1.0으로 보낼건데?
    {
      Close(connfd);
      continue;
    };
    make_headers(headers);
    request_to_server(method, uri, version, headers, endserver, response); // 응답 못 받았을 때의 처리 필요
    // send_response();
    Close(connfd);
  }
  return 0;
}

int get_request(int fd, rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver)
{
  struct stat sbuf;
  char buf[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  read_request(rio, method, uri, version, headers, endserver);

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) // RFC 1945 - GET/HEAD/POST + 토큰의 extension-method..?
  {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return -1;
  }
  if (!strcmp(version, "1.1"))
  {
    strcpy(version, "HTTP/1.0");
  }
  if (strcasecmp(headers, "HOST:"))
  {
    clienterror(fd, headers, "400", "Bad Request",
                "HOST header is empty in the request");
    return -1;
  }
  return 0;
}

void read_request(rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver)
{
  char buf[MAXLINE];
  int is_request_line, host_idx;
  is_request_line = 1;
  while (strcmp(buf, "\r\n"))
  {
    if (is_request_line)
    {
      Rio_readlineb(rio, buf, MAXLINE);
      sscanf(buf, "%s %s %s", method, uri, version);
      is_request_line = 0;
    }
    else
    {
      Rio_readlineb(rio, buf, MAXLINE);
      if (strcasecmp(buf, "Connection: keep-alive"))
      {
        strcpy(buf, "Connection: close");
      }
      if (strcasecmp(buf, "Proxy-Connection: keep-alive"))
      {
        strcpy(buf, "Proxy-Connection: close");
      }
      if (host_idx = strstr(buf, "HOST: "))
      {
        strcpy(endserver, host_idx + 6);
      }
      strcat(headers, buf);
    }
  }
}

void make_headers(char *headers)
{
  char *connection_p, proxy_connection_p;
  if (strstr(headers, "User-Agent:"))
  {
    strcat(headers, user_agent_hdr);
  }
  if (strstr(headers, "Connection:"))
  {
    strcat(headers, "Connection: close");
  }
  if (strstr(headers, "Proxy-Connection:"))
  {
    strcat(headers, "Proxy-Connection: close");
  }
}

int request_to_server(char *method, char *uri, char *version, char *headers, char *end_server, char *response)
{
  int clientfd, is_absolute_uri, is_port, request_port;
  char request_uri[MAXLINE], full_http_request[MAXLINE];
  rio_t rio;

  is_absolute_uri = strstr(uri, "://");
  if (is_absolute_uri)
  {
    strcpy(request_uri, uri);
  }
  else
  {
    sprintf(request_uri, "%s%s", end_server, uri);
  }

  is_port = is_absolute_uri ? strstr(is_absolute_uri + 3, ":") : strstr(uri, ":");
  if (!is_port)
  {
    request_port = 80;
  }
  {
    request_port = atoi(is_port + 1);
  }

  sprintf(full_http_request, "%s %s %s\n%s\r\n", method, uri, version, headers); // 그냥 uri 넣어도 되나? \r\n 한번만 들어가는게 맞나?

  clientfd = Open_clientfd(request_uri, request_port);
  Rio_readinitb(&rio, clientfd);

  Rio_writen(clientfd, full_http_request, strlen(full_http_request));
  Rio_readlineb(&rio, response, MAXLINE);

  Close(clientfd);
  return 0;
};

void send_response(){

};

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
