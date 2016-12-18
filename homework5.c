//Port Number 8083
//Ankita Tank
//CS 361 Homework 5


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

void serve_request(int);

char * request_str = "HTTP/1.0 200 OK\r\n"
"Content-type: text/html; charset=UTF-8\r\n\r\n";

char * requestPDF="HTTP/1.0 200 OK\r\n"
"Content-type: application/pdf; charset=UTF-8\r\n\r\n";	

char * requestGIF="HTTP/1.0 200 OK\r\n"
"Content-type: image/gif; charset=UTF-8\r\n\r\n";

char * requestJPG="HTTP/1.0 200 OK\r\n"
"Content-type: image/jpeg; charset=UTF-8\r\n\r\n";

char * requestPNG="HTTP/1.0 200 OK\r\n"
"Content-type: image/png; charset=UTF-8\r\n\r\n";

char * requestICO="HTTP/1.0 200 OK\r\n"
"Content-type: image/x-icon; charset=UTF-8\r\n\r\n";

char * requestBIN="HTTP/1.0 200 OK\r\n"
"Content-type: text/bin; charset=UTF-8\r\n\r\n";




char * request_txt = "HTTP/1.0 200 OK\r\n"
"Content-type: text/plain; charset=UTF-8\r\n\r\n";


char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
"<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";


char * ERROR= "<!DOCTYPE html>\n"	
	"<html>\n"
	"<head>\n"
	"<body bgcolor=\"black\">\n"
	"<h1 style= \"color:white\">    -> -> -> Oppss!!! Something Went Wrong <- <- <-   <br>  </h>"
	"<img src = \"https://media.giphy.com/media/EpoXy1uKk5kXe/giphy.gif\" alt=\"Mountain View\" style=\"width:1000px:height:1000px:\" >\n"
	"</body>\n"
	"</html>";


//char * FAIL ="HTTP/1.0 404 ERROR \r\n"
//      "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * index_body = "<li><head><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></head></html>";


void * thread(void *arg)
{


	int sock =(*((int*)arg));
	free(arg);
	pthread_detach(pthread_self());
	serve_request(sock);
	//close(*(int*)vargp);
	return NULL;
}









/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */

void get_directory_content(int client_fd,char* directory_path,char* requested_file)
{

	//printf("i th edirectory path");


	//printf(directory_path);

	//  char* directory_listing = NULL;

	// open directory path up 
	DIR* path = opendir(directory_path);
	//	struct dirent* x;


	// check to see if opening up directory was successful
	if(path != NULL)
	{
		//    directory_listing = (char*) malloc(sizeof(char)*512);
		//      directory_listing[0] = '\0';


		char new[4096];
		sprintf(new,index_hdr,requested_file,requested_file);
		send(client_fd,new,strlen(new),0);



		// stores underlying info of files and sub_directories of directory_path
		struct dirent* underlying_file = NULL;


		// iterate through all of the  underlying files of directory_path
		while((underlying_file = readdir(path)) != NULL)
		{


			char content[4096];
			char tempContent[4096];
			char directory_listing[4096];
			struct stat checker;


			strcpy(directory_listing, directory_path);
			strcat(directory_listing,"/");
			strcat(directory_listing,underlying_file->d_name);

			printf("directory_listing: %s\n", directory_listing);

			//tempContent[0]='.';
			strcpy(&tempContent[0],directory_listing);
			strcat(tempContent,"/");

			printf("tempContent: %s\n",tempContent);

			if(stat(tempContent,&checker)==0)
			{
				if(S_ISDIR(checker.st_mode))
				{
					strcat(directory_listing,"/");
					printf("New directory_listing: %s\n",directory_listing);
				}
			}

			printf("\n");

			sprintf(content,index_body,directory_listing, underlying_file->d_name);
			send(client_fd,content,strlen(content),0);



		}
		send(client_fd,index_ftr,strlen(index_ftr),0);
		//closedir(path);
	}

	//return directory_listing;


}




char* parseRequest(char* request) {
	//assume file paths are no more than 256 bytes + 1 for null. 
	char *buffer = malloc(sizeof(char)*257);
	memset(buffer, 0, 257);

	if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 

	sscanf(request, "GET %s HTTP/1.", buffer);
	return buffer; 
}


void serve_request(int client_fd)
{
	int read_fd;
	int bytes_read;
	int file_offset = 0; 
	char client_buf[4096];
	char send_buf[4096];
	char filename[4096];
	char * requested_file;
	memset(client_buf,0,4096);
	memset(filename,0,4096);

	struct stat checker;
	int fileexists;



	while(1){

		file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
		if(strstr(client_buf,"\r\n\r\n"))
			break;
	}
	printf("\n\n\n");
//	printf("REQUEST IS: %s\n",client_buf);
	requested_file = parseRequest(client_buf);
	//send(client_fd,request_str,strlen(request_str),0);
	// take requested_file, add a . to beginning, open that file
	filename[0] = '.';  
	strncpy(&filename[1],requested_file,4095);
	printf("The filename is: %s\n", filename);
	//manstat helper from directory
	fileexists = stat(filename,&checker);
	printf("FILE EXISTS: %i\n", fileexists);


	if(fileexists==0 && (S_ISREG(checker.st_mode))) //it does exist
	{
		printf("TEST1\n");

		if(strstr(filename, ".pdf"))
		{
			printf("Before PDF\n");
			send(client_fd,requestPDF,strlen(requestPDF),0);
		}
		else if (strstr(filename, ".gif"))
		{       
			printf("Before GIF\n");
			send(client_fd,requestGIF,strlen(requestGIF),0);


		}
		else if(strstr(filename, ".jpg"))
		{       
			printf("Before JPG\n");
			send(client_fd,requestJPG,strlen(requestJPG),0);


		}


		else if (strstr(filename, ".txt"))
                {
                        printf("Before TXT\n");
                        send(client_fd,request_txt,strlen(request_txt),0);


                }



		else if (strstr(filename, ".bin"))
                {
                        printf("Before BIN\n");
                        send(client_fd,requestBIN,strlen(requestBIN),0);


                }

		else if(strstr(filename, ".png"))
		{       
			printf("Before PNG\n");
			send(client_fd,requestPNG,strlen(requestPNG),0);


		}
		else if(strstr(filename, ".ico"))
		{       
			printf("Before ICO\n");
			send(client_fd,requestICO,strlen(requestICO),0);




		}

		else
		{
			printf("Before send text/html");
			send(client_fd,request_str,strlen(request_str),0);		

		}

		read_fd = open(filename,0,0);
		while(1) //while you are reading in
		{

			bytes_read = read(read_fd,send_buf,4096);
			//if nothing i sbeing readed.
			if(bytes_read == 0)
			{
				break;
			}

			send(client_fd,send_buf,bytes_read,0);
		}	

	}

	//file not found
	else if(fileexists==-1)
	{
		printf("Error Openning the file");
		send(client_fd,ERROR,strlen(ERROR),0);	
		//	send(cliend_fd,FAIL,strlen(FAIL),0);

	}
	else if(fileexists==0 && S_ISDIR(checker.st_mode))
	{
		char name2[4096];
		strcpy(name2,filename);
		strcat(name2,"/index.html");


		if(stat(name2,&checker)<0)
		{
			get_directory_content(client_fd,filename,requested_file);		

		}
		else
		{	

			printf("Before HTML IN_DIR\n");
			send(client_fd,request_str,strlen(request_str),0);
			read_fd = open(name2,0,0);
			while(1) //while you are reading in
			{

				bytes_read = read(read_fd,send_buf,4096);
				//if nothing i sbeing readed.
				if(bytes_read == 0)
				{
					break;
				}

				send(client_fd,send_buf,bytes_read,0);
			}

			//ProcessFiles(client_fd,name2);
		}
	}


	close(read_fd);
	close(client_fd);
	return;

}



void ErrorMessage()
{
	printf("Invalid Number of Arguments Please try Again ... \n");
	exit(1);
}


/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
	//pthread_t *t=NULL;
	//int final=0;




	if(argc!=3){
		ErrorMessage();
	}


	/* For checking return values. */
	int retval;

	//for (int i = 0; i < argc; ++i)
	//{

	//  printf("hgfhgfgffhgfhargv[%d]: %s\n", i, argv[i]);
	//}


	/* Read the port number from the first command line argument. */
	int port = atoi(argv[1]);
	chdir(argv[2]);


	printf("my port number is: %i\n ", port);
	printf("this is the directory provided: %s\n ",argv[2]);

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
		int* sock= (int*)  malloc (sizeof(int));
		//char buffer[256];

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
		(*sock) = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
		if(sock < 0) {
			perror("Error accepting connection");
			exit(1);
		}

		/* At this point, you have a connected socket (named sock) that you can
		 * use to send() and recv(). */

		/* ALWAYS check the return value of send().  Also, don't hardcode
		 * values.  This is just an example.  Do as I say, not as I do, etc. */
		printf("Serving the client's request\n");
		//serve_request(sock);

		/* Tell the OS to clean up the resources associated with that client
		 * connection, now that we're done with it. */


		pthread_t tid;
		pthread_create(&tid,NULL,thread,(void*)sock);
		//pthread_exit(NULL);


	}



	close(server_sock);
}
