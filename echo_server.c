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
#include <arpa/inet.h>

#define ISspace(x) isspace((int)(x))
#define BUFSIZE 1024
#define SERVER_STRING "Server: Liso/1.0 \r\n"

char *logFile;
char *www;
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
/* Logging */
/**********************************************************************/
void save_logs(const char *s)
{

    FILE *log_file = fopen(logFile, "a"); //append

    if (log_file == NULL)
    {
        printf("Error accessing the file!\n");
        exit(1);
    }

    time_t localTime;
    struct tm *Tm;
    localTime = time(NULL);
    Tm = localtime(&localTime);

    fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d - ",
            Tm->tm_year+1900,
            Tm->tm_mon+1,
            Tm->tm_mday,
            Tm->tm_hour-2,
            Tm->tm_min,
            Tm->tm_sec
    );
    fprintf(log_file, "%s\n", s);
    fclose(log_file);
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
/********
 * **************************************************************/
/* Loops through the URL and gets the appropriate HTTP fileType values */
/**********************************************************************/
char * getType(char *extension){
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    char ext[1024];
    static char format[1024];

    fprintf(stdout,"Path %s\n",extension);


    if(strcasecmp(extension,"html")==0){
        return "text/html";
    }
    else if(strcasecmp(extension,"css")==0){
        return "text/css";
    }
    else if(strcasecmp(extension,"png")==0){
        return "image/png";
    }
    else if(strcasecmp(extension,"jpg")==0){
        return "image/jpeg";
    }
    else if(strcasecmp(extension,"gif")==0){
        return "image/gif";
    }
    else{
        fprintf(stdout,"error\n");
        return "text/html";
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

        char buf[BUFSIZE];

        sprintf(buf, "HTTP/1.1 200 No Content\r\n");
        sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
        sprintf(buf, "%sContent-Length: 0\r\n", buf);
        sprintf(buf, "%sContent-Type: text/html\r\n", buf);
        send(client, buf, strlen(buf), 0);
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
//    fprintf(stdout,"%s\n",url);
//    sprintf(path, "%s%s",www, url);

    sprintf(path, "www%s", url);
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
    //variables are dynamically assigned
    portnum = argv[1];
    logFile = argv[2];
    www = argv[3];

    save_logs("Server Started - Main Loop");
    fprintf(stdout,"RECEIVED portnum: %s\n",portnum);
    fprintf(stdout,"RECEIVED logfile: %s\n",logFile);
    fprintf(stdout,"RECEIVED www path: %s\n",www);

    fd_set mainFD;    // mainFD file descriptor list
    fd_set tempFD;  // temp file descriptor list for select()
    int FDmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newlyAcceptedFD;        // newly accept()ed socket descriptor
    struct sockaddr_storage clientAddress; // client address
    socklen_t addressLength;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&mainFD);    // clear the mainFD and temp sets
    FD_ZERO(&tempFD);

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
        fprintf(stderr, "Error Message: binding error\n");
        save_logs("Error Message: binding error");

        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the mainFD set
    FD_SET(listener, &mainFD);

    // keep track of the biggest file descriptor
    FDmax = listener; // so far, it's this one

    // main loop
    while(1) {
        tempFD = mainFD; // copy it
        if (select(FDmax+1, &tempFD, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= FDmax; i++) {
            if (FD_ISSET(i, &tempFD)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addressLength = sizeof clientAddress;
                    newlyAcceptedFD = accept(listener,
                                             (struct sockaddr *)&clientAddress,
                                             &addressLength);

                    if (newlyAcceptedFD == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newlyAcceptedFD, &mainFD); // add to mainFD set
                        if (newlyAcceptedFD > FDmax) {    // keep track of the max
                            FDmax = newlyAcceptedFD;
                        }
                        printf("selectserver: new connection from %s on "
                                       "socket %d\n",
                               inet_ntop(clientAddress.ss_family,
                                         get_in_addr((struct sockaddr*)&clientAddress),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newlyAcceptedFD);
                        save_logs("INFO Message: new connection started");

                    }
                } else {
                    accept_request(i);
                    FD_CLR(i, &mainFD);
                }
            }
        }
    }

}