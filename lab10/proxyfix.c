/*
 * proxy.c - ICS Web proxy
 * Name: Wang Xinwei
 * StuID: 516030910041
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

void doit(int connfd, struct sockaddr_in *sockaddr);
ssize_t Rio_writen_w(int fd, void *userbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *userbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *fp, void *userbuf, size_t maxlen);
ssize_t Open_clientfd_w(char *hostname, char *port);
void *thread(void*);	

sem_t mutex;
//FILE *fp;

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
	/* various declaration*/
	int listenfd;
    char hostname[MAXLINE], port[MAXLINE];
    char *connfdp;

    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

	Signal(SIGPIPE, SIG_IGN);

	/* check arguments */
	if (argc != 2){
		exit(1);
	}

	int portnum = atoi(argv[1]);
	if (portnum < 1024 || portnum > 65536){
		exit(2);
	}

	/* proxy */
	Sem_init(&mutex, 0, 1);

	listenfd = Open_listenfd(argv[1]);

	while(1){
		clientlen = sizeof(clientaddr);
		connfdp = Malloc(sizeof(int) + sizeof(uint32_t));
		*((int*)connfdp) = Accept(listenfd, (SA *)&clientaddr, &clientlen); 	
	
		int flags = NI_NUMERICHOST;
		Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, flags);
		inet_pton(AF_INET, hostname, connfdp+sizeof(int));
	
		Pthread_create(&tid, NULL, thread, connfdp);
	}

    exit(0);
}


/*
 * thread
 */
void *thread(void *vargp)
{
    int connfd = *((int*)vargp);
	Pthread_detach(pthread_self());

    struct in_addr addr;
	struct sockaddr_in sockaddr;
    addr.s_addr = *((uint32_t*)(vargp+sizeof(int)));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr = addr;

	free(vargp);
    doit(connfd, &sockaddr);
    close(connfd);
	
	return NULL;
}

/*
 * doit - handle the client HTTP transaction
 */
void doit(int connfd, struct sockaddr_in *sockaddr)
{
	/* various declaration */
	int clientfd;
	char client_buf[MAXLINE], server_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char target[MAXLINE], path[MAXLINE], port[MAXLINE];
	rio_t client_rio, server_rio;
	
	int req_bodysize = 0, req_tmp;
	int res_bodysize = 0, res_headsize = 0, res_tmp;
	int read_res, write_res;

	/* read request line and headers */
 	rio_readinitb(&client_rio, connfd);
	read_res = rio_readlineb(&client_rio, client_buf, MAXLINE); 

	if (read_res <= 0) { 
		return;
	}
	sscanf(client_buf, "%s %s %s", method, uri, version);
	//	return;
	//}
    if (parse_uri(uri, target, path, port) != 0){
		return;
	}


	/* build a request to target server */
	char request[MAXLINE];
	sprintf(request, "%s /%s %s\r\n", method, path, version);
	
	while ((read_res = rio_readlineb(&client_rio, client_buf, MAXLINE))){
		if (read_res <= 0){
			return;
		}
	
		if (!strncasecmp(client_buf, "Content-Length", 14)){
			req_bodysize = atoi(client_buf + 15);
		}
		sprintf(request, "%s%s", request, client_buf);
		
		if (!strcmp(client_buf, "\r\n")){
			break;
		}
	}

	if (read_res <= 0){
        return;
	}
	

	/* send request to target server */
	clientfd = open_clientfd(target, port);
	if (clientfd <= 0){
		return;
	}

	rio_readinitb(&server_rio, clientfd);

	write_res = rio_writen(clientfd, request, strlen(request));
	if (write_res != strlen(request)){
        close(clientfd);
		return;
	}

	/* send request body to server */
	if (req_bodysize != 0){
		req_tmp = req_bodysize;
		while (req_tmp > 0){
			read_res = rio_readnb(&client_rio, client_buf, 1);
			if ((read_res <= 0) && (req_tmp > 1)){
		        close(clientfd);
				return;
			}

			write_res = rio_writen(clientfd, client_buf, 1);
			if (write_res != 1){
    			close(clientfd);
		    	return;
			}

			req_tmp -= 1;
		}
	}


	/* receive response from server */
	while ((res_tmp = rio_readlineb(&server_rio, server_buf, MAXLINE))){
		if (res_tmp <= 0){
			close(clientfd);
			return;
		}	
	
		if (!strncasecmp(server_buf, "Content-Length", 14)){
			res_bodysize = atoi(server_buf + 15);
		}
		
		res_headsize += res_tmp;
		write_res = rio_writen(connfd, server_buf, strlen(server_buf));

		if (write_res != strlen(server_buf)){
        	close(clientfd);
			return;
		}

		if (!strcmp(server_buf, "\r\n")){
			break;
		}
	}
	
	if (res_tmp == 0){
        close(clientfd);
		return;
	}
	
	
	/* forward the response body to client */
	res_tmp = res_bodysize;
	while (res_tmp > 0){
	 	read_res = rio_readnb(&server_rio, server_buf, 1);
		if ((read_res <= 0) && (res_tmp > 1)){
        	close(clientfd);
			return;
		}

		write_res = rio_writen(connfd, server_buf, 1);
		if (write_res != 1){
        	close(clientfd);
			return;
		}

		res_tmp -= 1;
	}

	close(clientfd);	


	/* Print log */
	char logstring[MAXLINE];
	format_log_entry(logstring, sockaddr, uri, res_bodysize + res_headsize);

	P(&mutex);
	printf("%s\n", logstring);
	V(&mutex);
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


ssize_t Rio_writen_w(int fd, void *userbuf, size_t n)
{
	ssize_t rc;

	if ((rc = rio_writen(fd, userbuf, n)) != n)
	{
		fprintf(stderr, "%s: %s\n", "Rio writen error", strerror(errno));
		return 0;
	}
	
	return rc;
}

ssize_t Rio_readnb_w(rio_t *fp, void *userbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(fp, userbuf, n)) != n)
    {
        fprintf(stderr, "%s: %s\n", "Rio readnb error", strerror(errno));
        return 0;
    }

    return rc;
}

ssize_t Rio_readlineb_w(rio_t *fp, void *userbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(fp, userbuf, maxlen)) < 0)
    {
        fprintf(stderr, "%s: %s\n", "Rio readlineb error", strerror(errno));
        return 0;
    }

    return rc;
}

ssize_t Open_clientfd_w(char *hostname, char *port)
{
    ssize_t rc;

    if ((rc = open_clientfd(hostname, port)) < 0)
    {
        fprintf(stderr, "%s: %s\n", "Open_clientfd error", strerror(errno));
        return 0;
    }

    return rc;
}
