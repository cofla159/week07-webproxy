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

typedef struct
{
  int *connfdp, *clientfd;
  char hostname[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], headers[MAXLINE], endserver[MAXLINE];
  rio_t rio;
} variable_t;

typedef struct
{
  char path[MAXLINE];
  char contents[MAX_OBJECT_SIZE];
  node_t *next;
} node_t;

int get_request(int fd, rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver, node_t *head);
int request_to_server(char *method, char *uri, char *version, char *headers, char *endserver, int *clientfd, node_t *head);
void send_response(int connfd, int clientfd);
void read_request(rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver);
void make_headers(char *headers);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void *thread(void *vargp);
node_t *check_caching(char *uri, node_t *head);
void add_caching(node_t *head, char *uri, char *full_http_request);

int main(int argc, char **argv)
{
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  int listenfd;
  variable_t *variables;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    variable_t *varibles = (variable_t *)Malloc(sizeof(variable_t));
    strcpy(variables->uri, "");

    clientlen = sizeof(clientaddr);
    variables->connfdp = (int *)Malloc(sizeof(int));
    *(variables->connfdp) = Accept(listenfd, (SA *)&clientaddr,
                                   &clientlen);

    Getnameinfo((SA *)&clientaddr, clientlen, variables->hostname, MAXLINE, variables->port, MAXLINE, 0);
    printf("Accepted connection from host: (%s, %s)\n", variables->hostname, variables->port);

    Pthread_create(&tid, NULL, thread, variables);
  }
  return 0;
}

void *thread(void *vargp)
{
  variable_t *variables = (variable_t *)vargp;
  int connfd = *(variables->connfdp);
  node_t *head = NULL;

  Pthread_detach(pthread_self());

  Rio_readinitb(&(variables->rio), connfd);

  if (get_request(connfd, &(variables->rio), variables->method, variables->uri, variables->version, variables->headers, variables->endserver, head) < 0) // version을 받아오는 의미가 있나 어차피 다 1.0으로 보낼건데?
  {
    Close(connfd);
  };
  make_headers(variables->headers);
  request_to_server(variables->method, variables->uri, variables->version, variables->headers, variables->endserver, &(variables->clientfd), head);
  send_response(connfd, variables->clientfd);

  Free(vargp);
  Close(connfd);
  return NULL;
}

int get_request(int fd, rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver, node_t *header)
{
  node_t *found_node;
  read_request(rio, method, uri, version, headers, endserver);

  if (found_node = check_caching(uri, header))
  {
    Rio_writen(fd, found_node->contents, strlen(found_node->contents));
  }

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return -1;
  }
  if (!strcmp(version, "HTTP/1.1"))
  {
    strcpy(version, "HTTP/1.0");
  }
  if (!strstr(headers, "Host:"))
  {
    clienterror(fd, headers, "400", "Bad Request",
                "Host header is empty in the request");
    return -1;
  }
  return 0;
}

void read_request(rio_t *rio, char *method, char *uri, char *version, char *headers, char *endserver)
{
  char buf[MAXLINE];
  int is_request_line;
  char *host_idx, *host_name_s, *path;
  is_request_line = 1;
  strcpy(uri, "");
  strcpy(buf, "");
  while (strcmp(buf, "\r\n"))
  {
    if (is_request_line)
    {
      Rio_readlineb(rio, buf, MAXLINE);
      sscanf(buf, "%s %s %s", method, uri, version);

      host_name_s = strstr(uri, "http://");
      path = host_name_s ? strstr(host_name_s + 7, "/") : strstr(uri, "/");
      if (path)
      {
        strcpy(uri, path);
      }
      else
      {
        strcpy(uri, "/");
      }
      is_request_line = 0;
    }
    else
    {
      Rio_readlineb(rio, buf, MAXLINE);
      if (strstr(buf, "Connection:"))
      {
        strcpy(buf, "Connection: close\n");
      }
      if (strstr(buf, "Proxy-Connection:"))
      {
        strcpy(buf, "Proxy-Connection: close\n");
      }
      if (host_idx = strstr(buf, "Host: "))
      {
        strncpy(endserver, host_idx + 6, strlen(host_idx) - 7);
        endserver[strlen(endserver) - 1] = '\0';
      }
      strcat(headers, buf);
    }
  }
}

node_t *check_caching(char *uri, node_t *head)
{
  if (!head)
    return NULL;

  node_t now_node = *head;
  while (now_node.next)
  {
    if (now_node.path == uri)
      // 노드 맨 앞으로 옮겨주기
      return &now_node;
    now_node = *(now_node.next);
  }
  return NULL;
}

void make_headers(char *headers)
{
  char new_header[MAXLINE];
  strcpy(new_header, "");
  strncpy(new_header, headers, strlen(headers) - 2);

  if (!strstr(headers, "User-Agent:"))
  {
    strcat(new_header, user_agent_hdr);
  }
  if (!strstr(headers, "Connection:"))
  {
    strcat(new_header, "Connection: close\n");
  }
  if (!strstr(headers, "Proxy-Connection:"))
  {
    strcat(new_header, "Proxy-Connection: close\n");
  }
  strcat(new_header, "\r\n");
  strcpy(headers, "");
  strcpy(headers, new_header);
}

int request_to_server(char *method, char *uri, char *version, char *headers, char *endserver, int *clientfd, node_t *head)
{
  char *is_port, *rest_uri;
  char request_uri[MAXLINE], full_http_request[MAXLINE], request_port[MAXLINE];

  is_port = strstr(endserver, ":");
  if (!is_port)
  {
    strcpy(request_uri, endserver);
    strcpy(request_port, "80");
  }
  else
  {
    strncpy(request_uri, endserver, (int)(is_port - endserver));
    strcpy(request_port, is_port + 1);
  }

  sprintf(full_http_request, "%s %s %s\n%s\r\n\r\n", method, uri, version, headers);
  if (strlen(full_http_request) <= MAX_OBJECT_SIZE)
  {
    add_caching(head, uri, full_http_request);
  }

  *clientfd = Open_clientfd(request_uri, request_port);
  Rio_writen(*clientfd, full_http_request, strlen(full_http_request));
  return 0;
};

void add_caching(node_t *head, char uri[], char full_http_request[])
{
  node_t *new_node = (node_t *)Malloc(sizeof(node_t));
  strcpy(new_node->path, uri);
  strcpy(new_node->contents, full_http_request);
  new_node->next = *head;
  head = new_node
}

void send_response(int connfd, int clientfd)
{
  char buf[MAXLINE], header[MAXLINE];
  rio_t rio;
  int content_length;

  Rio_readinitb(&rio, clientfd);
  strcpy(buf, "");
  strcpy(header, "");
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio, buf, MAXLINE);
    if (strstr(buf, "Content-length:"))
    {
      char *p = strchr(buf, ':');
      char temp_str[MAXLINE];
      strcpy(temp_str, p + 1);
      content_length = atoi(temp_str);
    }
    strcat(header, buf);
  }
  Rio_writen(connfd, header, strlen(header));

  char *body = malloc(content_length);
  Rio_readnb(&rio, body, content_length);
  Rio_writen(connfd, body, content_length);

  free(body);
  Close(clientfd);
}

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
