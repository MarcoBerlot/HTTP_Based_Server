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
#include <time.h>


#define ISspace(x) isspace((int)(x))
#include <arpa/inet.h>

#define BUFSIZE 1024
#define PORT "9999"
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
void get_time(char *date);

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

//    echo(client);
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

//        cgi = 1;

        char buff[BUFSIZE];
        char date[BUFSIZE];

        get_time(date);

        sprintf(buff, "HTTP/1.1 200 No Content\r\n");
        sprintf(buff, "%sServer: Liso/1.0\r\n", buff);
        sprintf(buff, "%sDate: %s\r\n", buff, date);
//        sprintf(buff, "Connection: keep alive\r\n");
//        if (!context->keep_alive) sprintf(buff, "%sConnection: close\r\n", buff);
        sprintf(buff, "%sContent-Length: 0\r\n", buff);
        sprintf(buff, "%sContent-Type: text/html\r\n\r\n", buff);
        send(client, buff, strlen(buff), 0);

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

void get_time(char *date)
{
    struct tm tm;
    time_t now;
    now = time(0);
    tm = *gmtime(&now);
    strftime(date, BUFSIZE, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv)
{


    char *portnum;
    portnum = argv[1];

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, portnum, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }


    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                                       "socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                    }
                } else {
                    // handle data from a client
//                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                    // got error or connection closed by client
//                        if (nbytes == 0) {
//                            // connection closed
//                            printf("selectserver: socket %d hung up\n", i);
//                        } else {
//                            perror("recv");
//                        }
                    accept_request(i);
//                        close(i); // bye!
                    FD_CLR(i, &master); // remove from master set
//                    }
// else {
//                        // we got some data from a client
//                        if (send(i, buf, nbytes, 0) != nbytes) {
//                            perror("send");
//                        }
//
//                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

}