# LISOD
LISOD is a web server in C language with support for multiple concurrent clients using the Berkeley Sockets API
## Usage
In order to compile just run the command make. To execute the program you should pass the port number as the first argument from command line,
### Functions
Trough the use of the select() function the server is able to support up to 1021 concurrent connections.
Right now the server supports `GET`, `HEAD` `POST`.
Everything that is implemented in based on RFC 2616
