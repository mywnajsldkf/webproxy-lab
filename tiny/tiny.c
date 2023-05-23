/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#define malloc

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

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
    // 연결 요청을 접수한다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

// 1개의 HTTP 트랜잭션을 처리한다.
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  // 다른 메서드를 요청하면, 에러 메시지를 보내고, main 루틴으로 돌아온다.
  if (strcasecmp(method, "GET") * strcasecmp(method, "HEAD") != 0) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // read_requesthdrs(&rio);
  
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);  // static or dynamic
  // file이 디스크 상에 없으면, 에러 메시지를 즉시 클라이언트에게 보내고 반환한다.
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static)  /* Serve static content*/
  {
    // 보통 파일이고, 읽기 권한인지 검증한다.
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 맞다면 클라이언트에게 제공한다.
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else {
    // 동적 컨텐츠에 대한 것이라면 파일이 실행 가능한지 검증하고
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 그렇다면 동적 컨텐츠를 제공한다.
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

// 에러메시지를 클라이언트에게 전달한다.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// header를 읽고 무시한다.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")){ /* Static content */
      strcpy(cgiargs, "");    // CGI 인자 스트링을 지우고
      strcpy(filename, ".");  // 상대 리눅스 경로 이름으로 변환한다.
      strcat(filename, uri);
      if (uri[strlen(uri)-1] == '/')
      {
        strcat(filename, "home.html");  // '/' 문자로 끝난다면 기본 파일 이름을 추가한다.
      }
      return 1;
  }
  else {  /* Dynamic content */
      ptr = index(uri, '?');
      if (ptr) {
        strcpy(cgiargs, ptr+1);
        *ptr = '\0';
      }
      else
        strcpy(cgiargs, "");
      // URI 부분을 상대 리눅스 파일 이름으로 변환한다.
      strcpy(filename, ".");
      strcat(filename, uri);
      return 0;
  }  
}

void serve_static(int fd, char *filename, int filesize, char *method)
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
  printf("Response headers: \n");
  printf("%s", buf);

  if (strcasecmp(method, "GET") == 0)
  {
    srcfd = Open(filename, O_RDONLY, 0);
    #ifdef malloc
      srcp = Malloc(filesize);  // filesize만큼 동적 메모리를 할당한다.
      Rio_readn(srcfd, srcp, filesize); // 파일을 읽어서 srcp에 복사한다.
      Close(srcfd);   // 파일 매핑이 완료되면 파일을 닫는다.
      Rio_writen(fd, srcp, filesize); // 파일을 읽어서 버퍼쓰기만큼 작성한다.
      free(srcp); // 사용한 가상 메모리를 반환한다.
    #else
      srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
      Close(srcfd);
      Rio_writen(fd, srcp, filesize);
      Munmap(srcp, filesize);
    #endif
  }
}

/**
 * get_filetype - Derive file type from filename
*/
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
  {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif"))
  {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png"))
  {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg"))
  {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".mpg"))
  {
    strcpy(filetype, "video/mpg");
  } else if (strstr(filename, ".mp4"))
  {
    strcpy(filetype, "video/mp4");
  }
  else
  {
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program*/
  }
  Wait(NULL);
}