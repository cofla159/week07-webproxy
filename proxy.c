#include <stdio.h>
#include "./tiny/csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int get_request(int fd, rio_t *rio, char *method, char *uri, char *version, char *headers);
void request_to_server();
void send_response();
void read_request(rio_t *rio, char *method, char *uri, char *version, char *headers);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], headers[MAXLINE];
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

    if (get_request(connfd, &rio, method, uri, version, headers) < 0)
    {
      Close(connfd);
      continue;
    };
    // request_to_server();
    // send_response();
    Close(connfd);
  }
  return 0;
}

int get_request(int fd, rio_t *rio, char *method, char *uri, char *version, char *headers)
{
  struct stat sbuf;
  char buf[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  read_request(rio, method, uri, version, headers);

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
  return 0;
}

void read_request(rio_t *rio, char *method, char *uri, char *version, char *headers)
{
  char buf[MAXLINE];
  int is_request_line = 1;
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
      strcat(headers, buf);
    }
  }
}

void request_to_server()
{
  int clientfd;
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
