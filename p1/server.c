/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <sys/stat.h>
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

#define CHUNK_SIZE 4096
#define MAX_FILENAME_SIZE 128
#define PLAINTEXT 0
#define JPEG 1
#define GIF 2

enum {
  inv = -1,
  html,
  htm,
  txt,
  jpg,
  jpeg,
  gif
} types;

char* types_arr[6] = {
  "html",
  "htm",
  "txt",
  "jpg",
  "jpeg",
  "gif"
};

char* content_types[4] = {
  "text/html",
  "text/plain",
  "image/jpeg",
  "image/gif"
};

enum {
  ok,
  moved,
  bad,
  notfound,
  httpns,
} status;

char* status_arr[5] = {
  "200 OK",
  "301 Moved Permanently",
  "400 Bad Request",
  "404 Not Found",
  "505 HTTP Version Not Supported"
};

const int space_len = 3;
int connect_sock_fd, listen_sock_fd;

void error(char *msg)
{
  perror(msg);
  exit(1);
}

void sig_handler(int signo) {
  if (signo == SIGINT) {
    close(connect_sock_fd);
    close(listen_sock_fd);
    printf("Recieved SIGINT\n");
  }
}

// IN: string with %20
// OUT: string with %20 replaced with spaces
char* insertSpaces(char* str) {
  if (!str) return NULL;
  int len = strlen(str);
  // The maxixum i value (we don't wanna go out of bounds)
  // Because we are searching for %20 and it is 3 characters long
  int max_it = len - space_len;
  char* ret = malloc(sizeof(char) * MAX_FILENAME_SIZE);
  int i = 0;
  int j = 0;
  // Iterate through the filename, look for substring %20
  while (str[i] != '\0') {
    if (i <= max_it && str[i] == '%') {
      if (str[i + 1] == '2' && str[i + 2] == '0') {
	ret[j] = ' ';
	j++;
	i += space_len;
	continue;
      }
    }
    else {
      ret[j] = str[i];
      j++;
    }
    i++;
  }
  // Place the null byte
  ret[j] = '\0';
  return ret;
}

// IN: filename, permissions
// OUT: file descriptor associated with case insensitive filename
int open_ci(char* filename, int perm) {
  int fd = -1;
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  DIR* curr_dir = opendir(cwd);
  if (opendir != NULL) {
  struct dirent* entry;
  while ((entry = readdir(curr_dir)) != NULL) {
    // case insensitive comparison of filename and current entry
    if (strcasecmp(filename, entry->d_name) == 0) {
      fd = open(entry->d_name, perm);
      break;
    }
  }
  }
  return fd;
}

// IN: request message
// OUT: file name requested 
char* getFilename(char* req_line) {
  if (!req_line) return NULL;
  char* cpy = NULL;
  char* first_line = NULL;
  char* filename = NULL;
  int it = 0;
  // Necessary as strsep modifies input
  cpy = req_line;
  // Get the first line of request message
  first_line = strsep(&cpy, "\n");
  if (first_line != NULL) {
    // Get the file name from the first line
    filename = NULL; 
    while ((filename = strsep(&first_line, " ")) != NULL) {
      if (it == 1) break;
      it++;
    }
  }
  filename = insertSpaces(filename);
  // remove the /
  filename = filename + 1;
  return filename;
}

// IN: filename
// OUT: filetype number (based on enum)
char* getFiletype(char* filename) {
  char* ext = NULL;
  int it = 0;
  while ((ext = strsep(&filename, ".")) != NULL) {
    if (it == 1) break;
    it++;
  }
  if (ext != NULL) {
    if (strcmp(ext, types_arr[html]) == 0) return content_types[0];
    else if (strcmp(ext, types_arr[htm]) == 0) return content_types[0];
    else if (strcmp(ext, types_arr[txt]) == 0) return content_types[1];
    else if (strcmp(ext, types_arr[jpeg]) == 0) return content_types[2];
    else if (strcmp(ext, types_arr[jpg]) == 0) return content_types[2];
    else if (strcmp(ext, types_arr[gif]) == 0) return content_types[3];
  }
  // TODO: Figure out what happens when recieving a unsupported extension
  // Current behavior is returning plaintext type
  return content_types[1];
}

// IN: file descriptor of file and socket to send to
// OUT: total number of bytes written to the socket
char* sendFile(int fd, struct stat fd_stat) {
  char* buf = malloc(sizeof(char) * (fd_stat.st_size + 1));
  int bytes_read;
  memset(buf, 0, sizeof(char) * (fd_stat.st_size + 1));
  bytes_read = read(fd, buf, fd_stat.st_size);
  buf[fd_stat.st_size + 1] = '\0';
  
  return buf;
}

// IN: file descriptor of file and socket to send to
void sendResponse(char* filename, int fd, struct stat fd_stat,int send_sock) {
  // Get file type
  char* type = getFiletype(filename);
  char* status;
  // Get status code
  // TODO: Maybe implement the other error codes
  if (fd < 0) status = status_arr[notfound];
  else status = status_arr[ok];
  // Get the date
  time_t cur_time;
  char cur_time_buf[52];
  struct tm* cur_tm_info;
  time(&cur_time);
  cur_tm_info = gmtime(&cur_time);
  strftime(cur_time_buf, 52, "%a,%e %b %G %T GMT", cur_tm_info);
  // Get the last modified time
  char file_time_buf[52];
  struct tm* file_tm_info;
  file_tm_info = gmtime(&(fd_stat.st_ctime));
  strftime(file_time_buf, 52, "%a,%e %b %G %T GMT", file_tm_info);

  // Compile the message
  char msg_buf[CHUNK_SIZE];
  memset(msg_buf, 0, CHUNK_SIZE);
 
  // If the file exists
  if (fd > 0) {
    sprintf(msg_buf,
	    "HTTP/1.1 %s\r\nConnection: keep-alive\r\nDate: %s\r\nServer: Ubuntu\r\nLast-Modified: %s\r\nContent-Length: %ld\r\nContent-Type: %s\r\n\r\n",
	    status, cur_time_buf, file_time_buf, fd_stat.st_size, type);
  }
  else {
    sprintf(msg_buf,
	    "HTTP/1.1 %s\r\nConnection: close\r\nDate: %s\r\nServer: Ubuntu\r\n\r\n",
	    status, cur_time_buf);
  }

  // Print response (debugging)
  printf("%s\n", msg_buf);
  // Send response
  write (send_sock, msg_buf, strlen(msg_buf));
  // Send file
  if (fd > 0) {
  char file_buf[CHUNK_SIZE];
  memset(file_buf, 0, CHUNK_SIZE);
  long bytes_read;
  while ((bytes_read = read(fd, file_buf, 1)) != 0) {
    if (bytes_read > 0)
      write(send_sock, file_buf, bytes_read);
    else
      error("Error when reading file\n");
  }
  }
}

int main(int argc, char *argv[]) {
  int  port_num;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  if (argc != 2) {
    fprintf(stderr, "Error: Improper number of arguments used\n");
    exit(1);
  }
  else {
    port_num = atoi(argv[1]);
  }
  
  // Signal handler for SIGINT
  signal(SIGINT, sig_handler);
  
  // Open the listen socket
  listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock_fd < 0)
    error("ERROR opening socket");

  // Fill in server details
  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_num);

  // Bind the listen socket
  if (bind(listen_sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  // Start listening for connections
  listen(listen_sock_fd, 5);

  // Continuously accept connections
  int bytes_read, bytes_written = 0;
  char msg_buf[CHUNK_SIZE];
  char* html_msg = NULL;
  char* file_buf = NULL;
  char* filename = NULL;
  
  int fd = -1;
  struct stat fd_stat;
  memset(msg_buf, 0, CHUNK_SIZE);

  while ((connect_sock_fd = accept(listen_sock_fd, (struct sockaddr *) &cli_addr, &clilen)) > 0) {
    // read client's message
    bytes_read = read(connect_sock_fd, msg_buf, CHUNK_SIZE - 1);
    if (bytes_read < 0) {
      close(listen_sock_fd);
      close(connect_sock_fd);
      error("ERROR reading from socket\n");
    }
    
    // print message
    printf("%s\n", msg_buf);
    //printf("%s\n", filename);

    // get the filename
    filename = getFilename(msg_buf);
    
    // open filename
    fd = open_ci(filename, O_RDONLY);
    
    if (fd > 0) {
      fstat(fd, &fd_stat);
    }
    //reply to client
    sendResponse(filename, fd, fd_stat, connect_sock_fd);
    
    shutdown(connect_sock_fd, 0);  // close connection
  }
  shutdown(listen_sock_fd, 0);
  return 0;
}
