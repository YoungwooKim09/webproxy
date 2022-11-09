#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int fd);
void parse_uri(char *url, char *hostname, char *port, char *filename);

// /* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr =
//     "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
//     "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;


  /* Check command line args */
  if (argc != 2) {                       
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);      
  while (1) {                             
    clientlen = sizeof(clientaddr);       
    connfd = Accept(listenfd, (SA *)&clientaddr,      
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }

  // printf("%s", user_agent_hdr);
  return 0;
}

void doit(int fd)
{
  int server_fd;
  char buf[MAXLINE], sbuf[MAX_OBJECT_SIZE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], port[MAXLINE], filename[MAXLINE];
  rio_t rio , servrio;

  //클라이언트로부터 요청을 받는부분
  Rio_readinitb(&rio, fd);                                          
  Rio_readlineb(&rio, buf, MAXLINE);                    
  printf("Request headers:\n");
  printf("%s", buf);                                    
  sscanf(buf, "%s %s %s", method, url, version);
  parse_uri(url, hostname, port, filename);

  // proxy에서 tiny연결
  server_fd = Open_clientfd(hostname, port);
  Rio_readinitb(&servrio, server_fd);
  
  // tiny에 보낼 헤더 작성
  sprintf(buf, "GET %s HTTP/1.0\r\n", filename);
  sprintf(buf, "%sHOST: %s:%s\r\n", buf, hostname, port);
  sprintf(buf, "%sUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);
  Rio_writen(server_fd, buf, strlen(buf));

  // tiny-> proxy
  Rio_readnb(&servrio, sbuf, MAX_OBJECT_SIZE);

  // proxy -> client
  Rio_writen(fd, sbuf, MAX_OBJECT_SIZE);
  Close(server_fd);
}

void parse_uri(char *url, char *hostname, char *port, char *filename)
{
  char *p;
  char arg1[MAXLINE], arg2[MAXLINE];
  
  p = strchr(url, '/');
  strcpy(arg1, p+2);              // hostname:port/home.html

  if (strstr(arg1, ":")) {

    p = strchr(arg1, ':');
    *p = '\0';
    strcpy(hostname, arg1);       // hostname
    strcpy(arg2, p+1);            // port/home.html

    p = strchr(arg2, '/');
    *p = '\0';
    strcpy(port, arg2);           // port
    *p = '/';
    strcpy(filename, p);          // /home.html
  }
  
  else {                          // hostname/home.html
    p = strchr(arg1, '/');
    *p = '\0';
    strcpy(hostname, arg1);
    *p ='/';
    strcpy(filename, p);
  }
}