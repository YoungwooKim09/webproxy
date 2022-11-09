/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <stdlib.h>

void doit(int fd);                                                                // 한 개의 HTTP 트랜잭션을 처리한다.
void read_requesthdrs(rio_t *rp);                                                 // 요청 헤더를 읽고 무시한다.
int parse_uri(char *uri, char *filename, char *cgiargs);                          // URI를 파일 이름과 옵션으로 CGI 인자 스트링을 분석한다.
void serve_static(int fd, char *filename, int filesize, char method[MAXLINE]);    // 정적 컨텐츠를 클라이언트에게 서비스한다.
void get_filetype(char *filename, char *filetype);                                // 파일 이름의 접미어 부분을 검사해서 파일 타입을 결정한다.
void serve_dynamic(int fd, char *filename, char *cgiargs, char method[MAXLINE]);  // 동적 컨텐츠를 클라이언트에게 서비스한다.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,               // 에러 메시지를 클라이언트에게 보낸다.
                 char *longmsg);

// argc : argument count*
// ./tiny = '1'*
// argv : argument vector*
// argv[0] = ./tiny*
// argv[1] = host*
// argv[2] = port*
int main(int argc, char **argv) {
  int listenfd, connfd;                   
  char hostname[MAXLINE], port[MAXLINE];  //hostname - 도메인 이름 or IP주소, service - 서비스이름 or 포트번호
  socklen_t clientlen;                    //클라이언트의 IP주소 길이 - 32비트*
  struct sockaddr_storage clientaddr;     //클라이언트의 소켓 주소*

  /* Check command line args */
  if (argc != 2) {                        // 서버를 열기 위한 정상적인 format이 아니라면
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);                              // 에러메시지 종료
  }

  listenfd = Open_listenfd(argv[1]);                  // 듣기 식별자를 오픈하고 리턴하는 도움함수
  while (1) {                                         // 무한 루프를 통해 항시 대기
    clientlen = sizeof(clientaddr);                   // 클라이언트 주소 바이트 크기
    connfd = Accept(listenfd, (SA *)&clientaddr,      // 클라이언트로부터의 연결 요청을 기다린다.
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, // 소켓 주소 구조체를 대응되는 호스트이름(호스트주소)와 서비스이름(포트번호) 스트링으로 변환
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);              // hostname : 클라이언트 IP, port : 클라이언트 port
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;    // 정적컨텐츠인지 동적컨텐츠인지 알려주는 변수(1 - 정적 컨텐츠, 0 - 동적 컨텐츠)
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // uri : 파일 이름과 옵션인 인자들을 포함하는 url의 접미어 ex) adder?15000&213
  char filename[MAXLINE], cgiargs[MAXLINE];                           // cgiargs - 15000&213, filename과 cgiargs가 포함되면 동적 컨텐츠
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);                                            // connfd와 주소 rio에 위치한 읽기 버퍼와 연결
  Rio_readlineb(&rio, buf, MAXLINE);                                  // 최대 maxline-1개의 바이트를 rio에서 읽고, 이것을 메모리 위치 buf에 복사
  printf("Request headers:\n");
  printf("%s", buf);                                                  // method URI version
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {      // arg1과 arg2가 같으면 0을 반환, HEAD 요청 또한 고려
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);                                             // 요청을 읽어들였으므로, 다른 요청 헤더들을 무시한다.
  
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {                                    // 파일 정보를 가져와서 sbuf에 넣어준다.
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) {  /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {      // 보통 파일이라는 것과 읽기 권한을 가지고 있는지 검증
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {      // 보통 파일이라는 것과 실행 권한을 가지고 있는지 검증
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);        // 최대 maxline-1개의 바이트를 rio에서 읽고, 이것을 메모리 위치 buf에 복사
  while(strcmp(buf, "\r\n")) {            // arg1과 arg2과 같을 경우 0, 문자열 간의 비교를 통해 음수와 양수
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {  /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return -1;
  }
  else {                          /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/*
 * get_filetype - Derive file type from filename 
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpg");
  else
    strcpy(filetype, "text/plain");
}

void serve_static(int fd, char *filename, int filesize, char method[MAXLINE])
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
  
  if (!(strcasecmp(method, "HEAD")))    // HEAD 요청일 때
    return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);                          // 읽기 위해서 filename을 오픈하고 식별자를 얻어온다.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);   // 요청한 파일을 가상메모리 영역으로 매핑한다.
  Close(srcfd);                                                 // 파일을 메모리로 매핑한 이후 식별자는 더 이상 필요없으므로 파일을 닫는다.
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);                                       // 매핑된 가상메모리 주소를 반환한다.

  // 연습 문제
  // /* Send response body to client */
  // srcfd = Open(filename, O_RDONLY, 0);
  // srcp = (void *)malloc(sizeof(char) * filesize);
  // Rio_readn(srcfd, srcp, filesize);
  // Close(srcfd);
  // Rio_writen(fd, srcp, filesize);
  // free(srcp);
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char method[MAXLINE])
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {  /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);     /* 환경변수를 cgiargs로 설정한다. */
    setenv("REQUEST_METHOD", method, 1);    /* HEAD METHOD에 대한 처리를 위해 환경변수를 method로 설정한다. */
    Dup2(fd, STDOUT_FILENO);                /* Redirect stdout to client - 표준 출력을 클라이언트와 연계된 연결 식별자로 재지정 */
    Execve(filename, emptylist, environ);   /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}