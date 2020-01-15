/*
 * proxy.c - ICS Web proxy
 * Kang Huquan 
 * 517021910839
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/* In debug mode, debug information such as printf will be not execute */
const int debug_mode = 0;
sem_t log_mutex;

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
int proxy_send(int clientfd, rio_t *conn_rio, char *requestheader, size_t content_length, char *method, char *pathname, char *version);
size_t proxy_receive(int connfd, rio_t *client_rio);
void doit(int connfd, struct sockaddr_in *clientaddr);

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0){
	    fprintf(stderr, "Rio_readlineb error");
        rc = 0;
    }
    return rc;
} 
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n) 
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0){
	    fprintf(stderr, "Rio_readnb error");
        rc = 0;
    }
    return rc;
}
int Rio_writen_w(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n){
	    fprintf(stderr, "Rio_writen error");
        return -1;
    }
    return 0;
}

typedef struct vargp{
    int connfd;
    struct sockaddr_in clientaddr;
} vargp_t;

void *thread(void *vargp){
    Pthread_detach(Pthread_self());
    vargp_t *vargp_self = (vargp_t *)vargp;
    doit(vargp_self->connfd, &(vargp_self->clientaddr));
    Close(vargp_self->connfd);
    Free(vargp_self);
    return NULL;
}

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    int listenfd;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;
    vargp_t *vargp;

    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    Sem_init(&log_mutex, 0, 1);
    Signal(SIGPIPE, SIG_IGN);
    /* As Server, Listen to Client */
    listenfd = Open_listenfd(argv[1]);
    while(1){
        vargp = Malloc(sizeof(vargp_t));
        vargp->connfd = Accept(listenfd, (SA *)&(vargp->clientaddr), &clientlen);
        Pthread_create(&tid, NULL, thread, vargp);
        Getnameinfo((SA *)&(vargp->clientaddr), clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        if(debug_mode) 
            printf("\n\n----------------Connected to (%s, %s)\n", client_hostname, client_port);
    }

    Close(listenfd);
    exit(0);
}

void doit(int connfd, struct sockaddr_in *clientaddr){
    char buf[MAXLINE], requestheader[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE];
    int clientfd;
    rio_t conn_rio, client_rio;
    size_t n, byte_size = 0, content_length = 0;

    /* Connect to Server */
    Rio_readinitb(&conn_rio, connfd);
    if(Rio_readlineb_w(&conn_rio, buf, MAXLINE) == 0){
        fprintf(stderr, "error: read empty request line\n");
        return;
    }
    if(sscanf(buf, "%s %s %s", method, uri, version) != 3){
        fprintf(stderr, "error: mismatched parameters\n");
        return;
    }
    if(parse_uri(uri, hostname, pathname, port) != 0 ){
        fprintf(stderr, "error: parse uri wrong\n");
        return;
    }

    /* Request Header */
    sprintf(requestheader, "%s /%s %s\r\n", method, pathname, version);
    while ((n = Rio_readlineb_w(&conn_rio, buf, MAXLINE)) != 0)
    {
        if(!strncasecmp(buf, "Content-Length", 14))
            sscanf(buf+15, "%zu", &content_length);
        sprintf(requestheader, "%s%s", requestheader, buf);
        if(debug_mode) 
            printf("(%zu bytes) %s", strlen(buf), buf);
        if(!strncmp(buf, "\r\n", 2))
            break;  
    }
    if(n == 0)
        return;

    /* Open Clientfd */
    while ((clientfd = open_clientfd(hostname, port)) < 0){
        fprintf(stderr, "error: open client fd wrong\n");
        fprintf(stderr, "error: hostname-%s\n", hostname);
        fprintf(stderr, "error: port-%s\n", port);
        return;
    }
    Rio_readinitb(&client_rio, clientfd);

    /* Debug Infomation */
    if(debug_mode) {
        printf("**********Connect to Server***\n");
        printf("Parse uri: %s %s %s\n", hostname, pathname, port);
        printf("Open clientfd %d", clientfd);
    } 

    //Send By Proxy From Client to Server
    if(proxy_send(clientfd, &conn_rio, requestheader, content_length, method, pathname, version) != -1)
        //Send By Proxy From Server to Client
        byte_size = proxy_receive(connfd, &client_rio);

    /* Format Output */
    format_log_entry(buf, clientaddr, uri, byte_size);
    P(&log_mutex);
    printf("%s\n", buf);
    V(&log_mutex);

    Close(clientfd);
}

int proxy_send(int clientfd, rio_t *conn_rio, char *requestheader, size_t content_length, char *method, char *pathname, char *version)
{
    char buf[MAXLINE];

    if(debug_mode) {
        printf("**********Send By Proxy From Client to Server***\n");
        printf(">>> Request headers:\n");
    }

    if(Rio_writen_w(clientfd, requestheader, strlen(requestheader)) == -1)
        return -1;

    if(debug_mode) {
        printf("%s", requestheader);
        printf("Send ALL Request Headers! Done!\n");
        printf(">>> Request bodys:\n");
    }

    /* Request Body */
    if(strcasecmp(method, "GET"))
        for(int i=0; i<content_length; i++){
            if(Rio_readnb_w(conn_rio, buf, 1) == 0)
                return -1;
            if(Rio_writen_w(clientfd, buf, 1) == -1)
                return -1;
            if(debug_mode)  
                printf("%c", *buf);
        }
    
    if(debug_mode) {
        printf("\nSend ALL Request Bodys! Done! Content-Length: %zu\n", content_length);
    }
    return 0;
}

size_t proxy_receive(int connfd, rio_t *client_rio)
{
    char buf[MAXLINE];
    size_t n, byte_size = 0, content_length = 0;

    /* Response Header */
    if(debug_mode) {
        printf("**********Send By Proxy From Server to Client***\n");
        printf(">>> Response headers:\n");
    } 

    while((n = Rio_readlineb_w(client_rio, buf, MAXLINE)) != 0){
        byte_size += n;
        if(!strncasecmp(buf, "Content-Length: ", 14))
            sscanf(buf+15, "%zu", &content_length);
        if(Rio_writen_w(connfd, buf, strlen(buf)) == -1)
            return 0;
        if(debug_mode)  
            printf("(%zu bytes) %s", strlen(buf), buf);
        if(!strncmp(buf, "\r\n", 2))
            break;
    }
    if(n == 0)
        return 0;

    if(debug_mode){
        printf("Receive ALL Response Headers! Done!\n");
        printf(">>> Response bodys:\n");
    }

    /* Response Body */
    for(int i=0; i<content_length; i++){
        if(Rio_readnb_w(client_rio, buf, 1) == 0)
            return 0;
        byte_size ++;
        if(Rio_writen_w(connfd, buf, 1) == -1)
            return 0;
        if(debug_mode)  
            printf("%c", *buf);
    }

    if(debug_mode)  
        printf("\nReceive All Response Body! Done! Content-Length: %zu\n", content_length);

    return byte_size;
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

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}


