/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp,int *length);
static void post_requesthdrs(rio_t *rp,int *length);
static void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

int main(int argc, char **argv) 
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
	doit(connfd);
	Close(connfd);
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static,contentLength=0,isGet=1;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
  
    /* Read and print request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("%s %s","The request lines:\n",buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("You are using %s\n",version);
    if (strcasecmp(method, "GET")!=0 && strcasecmp(method, "POST")!=0) {
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio,&contentLength);
    
    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
	clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	return;
    }

    if (is_static) { /* Serve static content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	serve_static(fd, filename, sbuf.st_size);
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
        if (strcasecmp(method, "POST")==0) {
            post_dynamic(fd, filename,contentLength,&rio);
        }
        else
            serve_dynamic(fd, filename, cgiargs);
    }
}
/* $end doit */

/* $begin post*/

static void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp)
{
    char buf[MAXLINE],length[32], *emptylist[] = { NULL },data[MAXLINE];
    int p[2];
    sprintf(length,"%d",contentLength);
    memset(data,0,MAXLINE);
    pipe(p);
    /*       The post data is sended by client,we need to redirct the data to cgi stdin.
     *  	 so, child read contentLength bytes data from fp,and write to p[1];
     *    parent should redirct p[0] to stdin. As a result, the cgi script can
     *    read the post data from the stdin.*/
     
    if (Fork() == 0)
	{                     /* child  */
		Close(p[0]);
        Rio_readnb(rp,data,contentLength);
        Rio_writen(p[1],data,contentLength);
		exit(0)	;
	}
    
    /* Send response headers to client */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    
    //Wait(NULL);
    Dup2(p[0],STDIN_FILENO);  /* Redirct p[0] to stdin */
    Close(p[0]);
    
    Close(p[1]);
    setenv("CONTENT-LENGTH",length , 1);
    Dup2(fd,STDOUT_FILENO);        /* Redirct stdout to client */
    Execve(filename, emptylist, environ);
}
/* $end post_dynamic */

/*
static void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp)
{
    char buf[MAXLINE],length[32], *emptylist[] = { NULL },data[MAXLINE];
    sprintf(length,"%d",contentLength);
    memset(data,0,MAXLINE);
    
    if (Fork() == 0)
	{                    
        Rio_readnb(rp,data,contentLength);
       // Rio_writen(p[1],data,contentLength);
    // Send response headers to client 
        
    //Wait(NULL);
        sprintf(buf, data);
        Rio_writen(fd, buf, strlen(buf));
        sprintf(buf, "\r\nHTTP/1.0 200 OK\r\n");
        Rio_writen(fd, buf, strlen(buf));
        sprintf(buf, "Server: Tiny Web Server\r\n");
        Rio_writen(fd, buf, strlen(buf));

    //Dup2(fd,STDIN_FILENO);  // Redirct p[0] to stdin 
    setenv("CONTENT-LENGTH",length , 1);
    Dup2(fd,STDOUT_FILENO);        // Redirct stdout to client 
    Execve(filename, emptylist, environ);
    }
    //Wait(NULL);
}*/



/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, int *length)
{
    char buf[MAXLINE];
    char *p;
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s","The request headers:\n");
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    if(strncasecmp(buf,"Content-Length:",15)==0)
    {
        p=&buf[15];
        p+=strspn(p," \t");
        *length=atol(p);
    }
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
	strcpy(cgiargs, "");
	strcpy(filename, ".");
	strcat(filename, uri);
	if (uri[strlen(uri)-1] == '/')
	    strcat(filename, "home.html");
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
	strcpy(filename, ".");
	strcat(filename, uri);
	return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* child */
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); 
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
	Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
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
/* $end clienterror */
