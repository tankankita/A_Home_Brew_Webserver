#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>

#define BACKLOG (10)

char root_dir[256];
char curr_dir[256];
int port;

void serve_request(int);

char * response_str = "HTTP/1.0 %s\r\n"
        "Content-type: %s; charset=UTF-8\r\n\r\n";


char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
        "<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";

// snprintf(output_buffer,4096,index_hdr,filename,filename);


char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr><address>Abashe 1.0.0 Server at localhost Port %d</address></body></html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char* parseRequest(char* request) {
    //assume file paths are no more than 256 bytes + 1 for null. 
    char *buffer = malloc(sizeof(char)*257);
    memset(buffer, 0, 257);

    if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 

    sscanf(request, "GET %s HTTP/1.", buffer);

    return buffer; 
}

// return status string based on status code
char* get_status_string(int status_code) {
    if (status_code == 200) {
        return "200 OK";
    } else if (status_code == 404) {
        return "404 Not Found";
    } else {
        return "500 Internal Server Error";
    }
}

// return MIME type string based on filename extension.
char* get_mime_type(char* filename) {
    
    // create copy of filename string so we don't alter filename pointer later using strtok
    char copy[256];
    strncpy(copy, filename, 256);

    // get file extension
    char* temp;
    char extension[256];
    temp = strtok(copy, ".");
    strncpy(extension, temp, 256);
    while (temp != NULL) {
        strncpy(extension, temp, 256);
        temp = strtok(NULL, ".");
    }
    
    // lookup mime type based on file extension
    if (strcmp(extension, "html") == 0) {
        return "text/html";
    } else if (strcmp(extension, "txt") == 0)  {
        return "text/plain";
    } else if (strcmp(extension, "jpeg") == 0 || strcmp(extension, "jpg") == 0 ) {
        return "image/jpeg";
    } else if (strcmp(extension, "gif") == 0 ){
        return "image/gif";
    } else if (strcmp(extension, "png") == 0) {
        return "image/png";
    } else if (strcmp(extension, "pdf") == 0) {
        return "application/pdf";
    } else if (strcmp(extension, "ico") == 0) {
        return "image/x-icon";
    } else if (strcmp(extension, "js") == 0) {
        return "application/javascript";
    } else {
        return "application/octet-stream";
    }

}

// returns response string, should be freed by caller
// takes in filename and http status_code
char* get_response(char* filename, int status_code) {
    char* response = (char*) malloc(sizeof(char)*512);
    memset(response, 0, 512);
    
    char temp[512];
    snprintf(temp, 512, response_str, get_status_string(status_code), get_mime_type(filename));
    
    strncpy(response, temp, 512);
    return response;
}

// send string to socket client_fd
void serve_string(char* string, int client_fd) {
    int len = strlen(string);
    int sent = send(client_fd, string, len, 0);
    // if we didn't send them all, send the remainder
    while (sent < len) {
        sent = sent + send(client_fd, string + sent, len - sent, 0);
    }
}

// send file_fd to socket client_fd
void serve_file(int file_fd, int client_fd) {
    int bytes_read;
    char send_buf[4096];
    while(1){
        bytes_read = read(file_fd ,send_buf,4096);
        if(bytes_read == 0)
            break;
        if (bytes_read == -1) {
            printf("error %s\n", strerror(errno));
            break;
        }
        int sent = send(client_fd,send_buf,bytes_read,0);
        // if we didn't send them all, send the remainder
        while (sent < bytes_read) {
            sent = sent + send(client_fd, send_buf + sent, bytes_read - sent, 0);
        }
    }
}

// generate an HTML page with directory listing, write it to a file,
// then send it
void serve_listing(char* dirpath, int client_fd, char* relative_path) {

    // generate HTML page
    char listing[128000];
    char temp[4096];
    snprintf(temp, 4096, index_hdr, dirpath, dirpath);
    strncpy(listing, temp, 4096);
    DIR *dir;
    struct dirent *ep;
    // for every file in the listing
    dir = opendir (dirpath);
    if (dir != NULL)
    {
        while (ep = readdir(dir)) {
            // generate a listing entry (table row)
            char row[4096];
            char filepath[4096];
            snprintf(filepath, 4096, "%s%s", relative_path, ep->d_name);
            snprintf(row, 4096, index_body, filepath, ep->d_name);
            // append it to the listing string
            strncat(listing, row, 128000); 
        }
        (void) closedir (dir);
    } else
        perror ("Couldn't open the directory to view listing");

    // now append the footer
    char row[4096];
    snprintf(row, 4096, index_ftr, port);
    strncat(listing, row, 128000);
    
    // send it
    serve_string(listing, client_fd);
}

// taken from http://stackoverflow.com/a/4553053/341505
// returns true if path is directory
int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

void serve_request(int client_fd){
    int read_fd;
    int file_offset = 0;
    char client_buf[4096];
    char * requested_file;
    memset(client_buf,0,4096);
    while(1){

        file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
        if(strstr(client_buf,"\r\n\r\n"))
            break;
    }
    requested_file = parseRequest(client_buf);

    // now construct filepath string using curr_dir + root_dir + filename
    char filepath[8192];
    snprintf(filepath, 8192, "%s/%s%s", curr_dir, root_dir, requested_file);


    if (is_directory(filepath)) {
        // check if index.html exists and serve that
        char indexpath[8192];
        snprintf(indexpath, 8192, "%s/index.html", filepath);
        read_fd = open(indexpath,0 ,0);
        // can't find index file for this directory
        if (read_fd < 0) {
            // if file doesnt exist, serve a directory listing page instead
            struct stat buffer;
            if (stat(indexpath, &buffer) < 0 && errno == ENOENT) {
                // serve response 200
                // throw in ".html" just for content-type
                serve_string(get_response(".html", 200), client_fd);
                // serve directory listing
                serve_listing(filepath, client_fd, requested_file);
                free(requested_file);
                return;
            } else {
                // serve 500 internal server error since file exists but we can't open it
                serve_string(get_response(requested_file, 500), client_fd);
                printf("can't open index.html, error: %s\n", strerror(errno));
                close(read_fd);
                free(requested_file);
                return;
            }
        // the index.html will be served
        } else {    
            serve_string(get_response("index.html", 200), client_fd);
            // serve file
            serve_file(read_fd, client_fd);
            close(read_fd);
            free(requested_file);
            return;
        }
        // if not directory, serve the file
    } else {
        read_fd = open(filepath,0,0);
        if (read_fd < 0) { 

            // serve the 404 file if you can't find the file
            struct stat buffer;
            if (stat(filepath, &buffer) < 0 && errno == ENOENT) {
                char newpath[8192];
                snprintf(newpath, 8192, "%s/%s/404.html", curr_dir, root_dir);
                close(read_fd);
                read_fd = open(newpath, 0, 0);
                if (read_fd < 0) {
                    // can't open file, send internal server error
                    serve_string(get_response(requested_file, 500), client_fd);
                    printf("can't open 404.html at %s, error: %s\n", newpath, strerror(errno));
                    free(requested_file);
                    close(read_fd);
                    return;
                }
                
                char* tmp = "404.html";
                // else if we can serve the 404, then first serve response
                serve_string(get_response(tmp, 404), client_fd);
                // serve file
                serve_file(read_fd, client_fd);
                close(read_fd);
                free(requested_file);
                return;
            } else {
                // can't open file, send internal server error
                serve_string(get_response(requested_file, 500), client_fd);
                printf("error getting 404.html: %s\n", strerror(errno));
                close(read_fd);
                free(requested_file);
                return;
            }
        // if we can find the file, serve it
        } else {
            // serve 200 OK response
            serve_string(get_response(requested_file, 200), client_fd);
            // serve file
            serve_file(read_fd, client_fd);
            close(read_fd);
            free(requested_file);
        }
    }

    // we should never get here, all cases are accounted for above
    return;
}

// TODO handle each incoming client in its own thread
// Your program should take two arguments:
/* 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
   
    
    /* For checking return values. */
    int retval;

    /* Read the port number from the first command line argument. */
    port = atoi(argv[1]);

    strncpy(root_dir, argv[2], 255);
    
    // find current directory
    getcwd(curr_dir, 256);
    if (curr_dir == NULL) {
        printf("failed to get working directory error: %s\n", strerror(errno));
        exit(1);
    }
    char dir[8192];
    snprintf(dir, 8192, "%s/%s", curr_dir, root_dir);
    printf("Welcome to Abashe, Basheer's web server! We are currently running on port %d and our root directory is %s\n", port, dir);
    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
            sizeof(reuse_true));
    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */


    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces


    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if(retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

    while(1) {
        /* Declare a socket for the client connection. */
        int sock;

        /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr); 

        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if(sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */
        serve_request(sock);

        /* Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it. */
        close(sock);
    }

    close(server_sock);
}
