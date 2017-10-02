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
/* Send the entire content of a file trough a socket
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buffer[1024];
    size_t s1=1024;
    int fd=fileno(resource);


    int nread;
    while(1){
        if((nread=read(fd,buffer,1024))<=0){
            break;
        }
        send(client, buffer, nread, 0);
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
/* Return the informations to fill an HTTP header
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
/***********************************************************************/
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



    /**********************************************************************/
    /* Code in courtesy of Beej Guide on select */
    /**********************************************************************/

    fd_set mainFD;  // main FD list
    fd_set tempFD;  // temp FD list
    int FDmax;      // max FD int

    int listener;               //listening socket FD
    int newlyAcceptedFD;        // newest accepted socket FD
    struct sockaddr_storage clientAddress;
    socklen_t addressLength;


    char clientIP[INET6_ADDRSTRLEN];

    int status=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, sckt;

    struct addrinfo instanceInfo, *ai, *p;

    FD_ZERO(&mainFD);    // clear the mainFD
    FD_ZERO(&tempFD);    // clear the tempFD

    memset(&instanceInfo, 0, sizeof instanceInfo); // clear data stored under instanceInfo

    instanceInfo.ai_family = AF_UNSPEC;
    instanceInfo.ai_socktype = SOCK_STREAM;
    instanceInfo.ai_flags = AI_PASSIVE;


    if ((sckt = getaddrinfo(NULL, portnum, &instanceInfo, &ai)) != 0) {
        exit(1);
    }


    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    //report binding error
    if (p == NULL) {
        fprintf(stderr, "Error Message: binding error\n");
        save_logs("Error Message: binding error");

        exit(1);
    }

    freeaddrinfo(ai); // free when finished

    // listen
    if (listen(listener, 20) == -1) {
        perror("listen");
        exit(1);
    }

    FD_SET(listener, &mainFD); // register the listener to the mainFD

    FDmax = listener; // get the max FD which is the listener

    while(1) {
        tempFD = mainFD;

        if (select(FDmax+1, &tempFD, NULL, NULL, NULL) == -1) {
            save_logs("server: closed");
            perror("select");
            exit(1);
        }

        for(i = 0; i <= FDmax; i++) {
            if (FD_ISSET(i, &tempFD)) {
                if (i == listener) {
                    addressLength = sizeof clientAddress;
                    newlyAcceptedFD = accept(listener,
                                             (struct sockaddr *)&clientAddress,
                                             &addressLength);

                    if (newlyAcceptedFD == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newlyAcceptedFD, &mainFD);
                        if (newlyAcceptedFD > FDmax) {    //update max
                            FDmax = newlyAcceptedFD;
                        }
                        printf("server: new connection from %s on "
                                       "socket %d\n",
                               inet_ntop(clientAddress.ss_family,
                                         get_in_addr((struct sockaddr*)&clientAddress),
                                         clientIP, INET6_ADDRSTRLEN),
                               newlyAcceptedFD);
                        save_logs("server: new connection started");

                    }
                } else {
                    accept_request(i);
                    FD_CLR(i, &mainFD); //deregister
                }
            }
        }
    }

}