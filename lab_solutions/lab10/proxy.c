/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
void Perform(int connfd, struct sockaddr_in sockaddr);
int passRequestLine(char *hostname, char *port, char *buf, char *method, char *pathname);
int passRequestHeader(int serverfd, int connfd, char *buf, rio_t *rio);
int passRequestBody(char *buf, int connfd, int serverfd, int requestLength);
void *thread(void *p);

// global variable
struct sockaddr_in sockAddress;
sem_t mutex, mutexfd;

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    int listenfd, *connfdp;
    struct sockaddr_storage clientAddress;
    socklen_t clientLength = sizeof(clientAddress);
    pthread_t threadid;
    Sem_init(&mutex, 0, 1);
    Sem_init(&mutexfd, 0, 1);
    listenfd = Open_listenfd(argv[1]);
    
    Signal(SIGPIPE, SIG_IGN); // ignore signal SIGPIPE

    while (1) {
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&sockAddress, &clientLength);
        Pthread_create(&threadid, NULL, thread, connfdp);
    }

    exit(0);
}

void *thread(void *p)
{
    int connfd = *((int*)p);
    Pthread_detach(pthread_self()); // new thread
    Perform(connfd, sockAddress);
    Free(p);

    return NULL;
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}

int passRequestLine(char *hostname, char *port, char *buf, char *method, char *pathname)
{
    P(&mutexfd);
    int serverfd = Open_clientfd(hostname, port);
    V(&mutexfd);
    sprintf(buf, "%s /%s HTTP/1.1\r\n", method, pathname);
    Rio_writen(serverfd, buf, strlen(buf));

    return serverfd;
}

int passRequestHeader(int serverfd, int connfd, char *buf, rio_t *rio)
{
    int requestLength;

    while (Rio_readlineb(rio, buf, MAXLINE) > 0) {
        Rio_writen(serverfd, buf, strlen(buf));

        if (strcmp(buf, "\r\n") == 0) {
            break;
        }

        if (strstr(buf, "Content-Length:")) {
            sscanf(buf, "Content-Length: %d\r\n", &requestLength);
        }
    }

    return requestLength;
}

int passRequestBody(char *buf, int connfd, int serverfd, int requestLength)
{    
    rio_t rio2;
    size_t size = 0;
    int n, length;
    Rio_readinitb(&rio2, serverfd);
   
    while ((n = Rio_readlineb(&rio2, buf, MAXLINE)) != 0) {
        if (strcmp(buf, "\r\n") != 0) {
            Rio_writen(connfd, buf, strlen(buf));
            size += n;

            if (strstr(buf, "Content-Length:")) {
                sscanf(buf, "Content-Length: %d\r\n", &length);
            }
        } else {
            size += 2;  
            break; 
        }
    }

    size += length;
    Rio_writen(connfd, "\r\n", strlen("\r\n"));

    while (length > 0) {
        Rio_readnb(&rio2, buf, 1);
        Rio_writen(connfd, buf, 1);
        length--;
    }

    return size;
}

void Perform(int connfd, struct sockaddr_in sockaddr)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE], log[MAXLINE];
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);

    sscanf(buf, "%s %s %s", method, uri, version);
    parse_uri(uri, hostname, pathname, port);
    
    /* ------start of request forwarding----- */

    // request line
    int serverfd = passRequestLine(hostname, port, buf, method, pathname);

    // request header
    int requestLength = passRequestHeader(serverfd, connfd, buf, &rio);
    
    // request body
    if (!strcmp(method, "POST")) {
        while (requestLength > 0) {
            Rio_readnb(&rio, buf, 1);
            Rio_writen(serverfd, buf, 1);
            requestLength--;
        }
    }

    int size = passRequestBody(buf, connfd, serverfd, requestLength);

    /* -----end of request forwarding----- */

    // print relevant log to screen for inspection
    P(&mutex);
    format_log_entry(log, &sockaddr, uri, size);
    printf("%s\n", log);
    V(&mutex);
    close(connfd);
    close(serverfd);

    return;
}

