/*
* selectserver.c - A TCP echo server that keeps track of the
        *                  number of connection requests and allows
        *                  the user to query this count and to terminate
*                  the server from stdin. It multiplexes two different
*                  kinds of events:
*                     1. the user has hit the return key
*                     2. a connection request has arrived
        *
        * usage: tcpserver <port>
        *
        * commands from stdin:
*   "c<nl>"  print the number of connection requests
*   "q<nl>"  quit the server
*/
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))
#include <arpa/inet.h>

#define BUFSIZE 1024
#define PORT 9999
#define SERVER_STRING "Server: Liso/1.0 \r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *, int);
void not_found(int);
void serve_file(int, const char *, char*);
int startup(u_short *);
void unimplemented(int);
void error(char *msg);
char *get_filename_ext(const char *);
char * getType(char *);
/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
/* Not Found message*/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
/**********************************************************************/
/* Get extension of a file */
/**********************************************************************/
char *get_filename_ext(const char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}
/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename, int size)
{
    char buf[1024];
    (void)filename;
    char outstr[200];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    char *extension;
    char *type;
    tmp = localtime(&t);
    struct stat attrib;
    char date[200];
    //Caluclate last modification date
    stat(filename, &attrib);
    strftime(date,sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", localtime(&(attrib.st_ctime)));
    if (tmp == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }
    //Calculate local time
    if (strftime(outstr, sizeof(outstr), "%a, %d %b %Y %H:%M:%S %Z", tmp) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }/* could use filename to determine file type */
    extension=get_filename_ext(filename);
    type=getType(extension);
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n",getType(extension));
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "Connection: keep alive\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf,"Date: %s\r\n",outstr);
    send(client, buf, strlen(buf), 0);
    sprintf(buf,"Content-Length: %d\r\n",size);
    send(client, buf, strlen(buf), 0);
    sprintf(buf,"Last-Modified: %s\r\n",date);
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

}

/**********************************************************************/
/* This function reply the user with the same message in the request */
/**********************************************************************/
void echo(int client)
{
    char buf[1024];


    recv(client, buf, BUFSIZE, 0);
    send(client, buf, strlen(buf), 0);
    return;
}
/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename, char *method)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    struct stat st;
    int size;

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        stat(filename, &st);
        size = st.st_size;
        headers(client, filename,size);
        if(strcmp(method,"GET")==0)
            cat(client, resource);
    }
    fclose(resource);
}

char * getType(char *extension){
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    char ext[1024];
    static char format[1024];

    if(!(resource = fopen("htdocs/MIME", "r"))){
        {
            fprintf(stdout,"Error opening the file\n");
            return "error";
        }
    }
    while(fscanf(resource,"%s %s",ext,format)!=EOF) {
        if (strcmp(extension, ext) == 0){
            return format;
        }
    }

}
/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
    char *query_string = NULL;
    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    //Get The Method
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';
    //NOT GET OR POST
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST") && strcasecmp(method, "HEAD"))
    {
        unimplemented(client);
        return;
    }
    if (strcasecmp(method, "POST") == 0){
        fprintf(stdout,"RECEIVED A %s\n",method);
        fprintf(stdout, "url: %s\n ",url);

        cgi = 1;
        unimplemented(client);
        return;
    }
    i = 0;
    //Retreive the URL
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    //GET, HEAD METHODS
    if (strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)
    {
        fprintf(stdout,"RECEIVED A %s\n",method);
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
	fprintf(stdout,"%s\n",url);
    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {

        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        //check the type of file
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH)    )
            cgi = 1;
        //Is it readable?
        if(st.st_mode & S_IRUSR) {
            serve_file(client, path,method);

        }else{
            fprintf(stdout,"Do not have permission to read the file\n");
        }
    }
    //echo(client);
    close(client);
    return;
}
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

int main(int argc, char **argv) {
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    char buf[BUFSIZE]; /* message buffer */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    int connectcnt; /* number of connection requests */
    int notdone;
    fd_set readfds;


    portno = atoi(argv[1]);

    /*
     * socket: create the parent socket
     */
    parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (parentfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));

    /* this is an Internet address */
    serveraddr.sin_family = AF_INET;

    /* let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* this is the port we will listen on */
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(parentfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * listen: make this socket ready to accept connection requests
     */
    if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */
        error("ERROR on listen");


    /* initialize some things for the main loop */
    clientlen = sizeof(clientaddr);
    notdone = 1;
    connectcnt = 0;
    printf("server> ");
    fflush(stdout);

    /*
     * main loop: wait for connection request or stdin command.
     *
     * If connection request, then echo input line
     * and close connection.
     * If command, then process command.
     */
    fprintf(stdout,"I---------SERVER LISTENING ON %d -------------I\n",portno);
    while (notdone) {

        /*
         * select: Has the user typed something to stdin or
         * has a connection request arrived?
         */
        FD_ZERO(&readfds);          /* initialize the fd set */
        FD_SET(parentfd, &readfds); /* add socket fd */
        FD_SET(0, &readfds);        /* add stdin fd (0) */
        if (select(parentfd+1, &readfds, 0, 0, 0) < 0) {
            error("ERROR in select");
        }

        /* if the user has entered a command, process it */
        if (FD_ISSET(0, &readfds)) {

            fgets(buf, BUFSIZE, stdin);
            switch (buf[0]) {
                case 'c': /* print the connection cnt */
                    printf("Received %d connection requests so far.\n", connectcnt);
                    printf("server> ");
                    fflush(stdout);
                    break;
                case 'q': /* terminate the server */
                    notdone = 0;
                    break;
                default: /* bad input */
                    printf("ERROR: unknown command\n");
                    printf("server> ");
                    fflush(stdout);
            }
        }

        /* if a connection request has arrived, process it */
        if (FD_ISSET(parentfd, &readfds)) {
            /*
             * accept: wait for a connection request
             */
            childfd = accept(parentfd,
                             (struct sockaddr *) &clientaddr, &clientlen);
            accept_request(childfd);
            if (childfd < 0)
                error("ERROR on accept");
            connectcnt++;



            close(childfd);
        }
    }

    /* clean up */
    printf("Terminating server.\n");
    close(parentfd);
    exit(0);
}
