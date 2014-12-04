// Brad Taylor
// CS 450
// December 2014

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#define MAX_CLIENTS 35

static unsigned int cli_count = 0;
static int uid = 10;


// client struct
typedef struct {
  struct sockaddr_in addr; //client remote address
  int connfd; // connection file descriptor
  int uid; // client unique id
  char name [32]; // client username
} client_t;


// array of clients
client_t *clients[MAX_CLIENTS];

// function prototypes
void queue_add (client_t *cl, int * dupe);
void queue_delete (int uid);
void kick (int auth, int uid); 
void send_message (char *s, int uid);
void send_message_all (char *s);
void send_message_self (const char *s, int connfd);
void send_message_client (char *s, int uid);
void send_message_name (char *s, char * name); 
void send_active_clients (int connfd);
void strip_newline(char *s);
void print_client_addr (struct sockaddr_in addr);
void *handle_client (void *arg);


int main (int argc, char *argv[])
{
  int listenfd = 0, connfd = 0, portNum = 8910;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;

  char username [32];
  
  // thread id, fork process later
  pthread_t tid;
  
  // socket settings
  listenfd = socket (AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
  
  if (argc > 1)
    portNum = atoi (argv[1]);
  
  // default port 8910
  serv_addr.sin_port = htons(8910); 
  
  // bind
  if (bind (listenfd, (struct sockaddr*) &serv_addr, sizeof (serv_addr)) < 0) {
    perror("Socket binding failed");
    return 1;
  }
  
  // listen
  if (listen (listenfd, 10) < 0) {
    perror("Socket listening failed");
    return 1;
  }
  
  // started ok
  printf("Server started on %d, awaiting connections\n", portNum);
  
  // busy wait to accept clients
  while (1) {
    socklen_t clilen = sizeof (cli_addr);
    connfd = accept (listenfd, (struct sockaddr*) &cli_addr, &clilen);
    
    // check the amount of clients
    if ((cli_count + 1) == MAX_CLIENTS) {
      printf ("Maximum clients reached\n");
      printf ("Rejecting request from ");
      print_client_addr (cli_addr);
      printf ("\n");
      close (connfd);
      continue;
    }
    

    // client settings
    client_t *cli = (client_t *) malloc (sizeof (client_t));
    cli->addr = cli_addr;
    cli->connfd = connfd;
    cli->uid = uid++;

    send_message_self ("Connected!\r\n", cli->connfd);

    // get their name
    int rlen;
    send_message_self ("Please enter a username:\r\n", cli->connfd);
    if ((rlen = read (cli->connfd, username, sizeof (username) - 1)) > 0) {
      username[rlen] = '\0';
      strip_newline(username);
      memcpy (cli->name, username, strlen (username) + 1);
    }    
    int dupe = 0;
    // add client to queue and fork the thread 
    queue_add (cli, &dupe);
    if (dupe == 0)
      pthread_create (&tid, NULL, &handle_client, (void*) cli);
    else {
      //send message and kick
      send_message_self ("Username was not unique. Please reconnect.\r\n", cli->connfd);
      //kick (1, cli->uid); */

      // do the same for now
      pthread_create (&tid, NULL, &handle_client, (void*) cli);
    }
      
 
    // let cpu sleep
    sleep(1);
  }
}


// functions

// add a client to client array and set dupe flag if so
void queue_add (client_t *cl, int * dupe) 
{
  int i;
  for (i = 0; i < cli_count; i++)
    if (!memcmp (clients[i]->name, cl->name, strlen (cl->name) + 1) && clients[i])
      * dupe = 1;
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (!clients[i]) { // add to end of array
      clients[i] = cl;
      return;
    }
  }
}


// remove client from queue 
void queue_delete (int uid) 
{
  int i;
  for(i = 0;i < MAX_CLIENTS; i++)
    if (clients[i])
      if (clients[i]->uid == uid) {
	clients[i] = NULL;
	return;
      }
}

/*
void kick (const int auth, int uid)
{
  int low = 9;
  int i;
  for (i = 0; i < cli_count; i++) {
    // get lowest auth
    if (auth < low)
      low = auth;
  }

  if (auth == 1 || auth == low) {
    // ok to kick
    
  }
  else {
    // send could not kick and return
    close (->connfd);    
    return;
  }

  
}
*/

// send a message to all except sender
void send_message (char *s, int uid) 
{
  int i;
  for(i = 0; i < MAX_CLIENTS; i++) 
    if (clients[i])
      if (clients[i]->uid != uid)
  write (clients[i]->connfd, s, strlen(s));
  printf ("%s", s);
}

// send a message to all the clients
void send_message_all (char *s) 
{
  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
    if (clients[i])
      write(clients[i]->connfd, s, strlen(s));
  printf ("%s", s);
}

// echo back a message to the sender
void send_message_self (const char *s, int connfd) 
{
  write(connfd, s, strlen(s));
}

// send a pm to specific client using uid
void send_message_client (char *s, int uid) 
{
  int i;
  for (i = 0; i < MAX_CLIENTS; i++) 
    if (clients[i])
      if (clients[i]->uid == uid)
  write(clients[i]->connfd, s, strlen(s));
}

// send a pm to specific client name 
void send_message_name (char *s, char * name) 
{
  int i;
  for (i = 0; i < cli_count; i++) 
    if (clients[i])
      if (!memcmp (clients[i]->name, name, strlen (name) + 1))
	write(clients[i]->connfd, s, strlen(s));
}

// send a list of all active clients
void send_active_clients (int connfd)
{
  int i;
  char s[64];
  for(i = 0; i < MAX_CLIENTS; i++)
    if(clients[i]) {
      sprintf (s, "%s\r\n", clients[i]->name);
      send_message_self(s, connfd);
    }
}


// get rid of newline and replace with null terminator
void strip_newline(char *s)
{
  while(*s != '\0') {
    if(*s == '\r' || *s == '\n')
      *s = '\0';
    s++;
  }
}

// print a client ip using bit shifts
void print_client_addr (struct sockaddr_in addr)
{
  printf("%d.%d.%d.%d",
   addr.sin_addr.s_addr & 0xFF,
   (addr.sin_addr.s_addr & 0xFF00)>>8,
   (addr.sin_addr.s_addr & 0xFF0000)>>16,
   (addr.sin_addr.s_addr & 0xFF000000)>>24);
}

// handle communication with the client
void *handle_client (void *arg)
{
  char buff_out[1024];
  char buff_in[1024];
  int rlen;
  
  cli_count++;
  client_t *cli = (client_t *) arg;
  
  printf ("Accepting ");
  print_client_addr (cli->addr);
  printf (" uid is %d, alias is %s\n", cli->uid, cli->name);
  
  sprintf (buff_out, "%s has joined the room!\r\n", cli->name);
  send_message_all (buff_out);
  
  // receive input from client */
  while ((rlen = read (cli->connfd, buff_in, sizeof (buff_in) - 1)) > 0) {
    buff_in[rlen] = '\0';
    buff_out[0] = '\0';
    strip_newline(buff_in);
    
    // ignore empty buffer
    if(!strlen(buff_in))
      continue;
    
    // special options, accessed by backslash
    if (buff_in[0] == '\\'){
      char *command, *param;
      command = strtok(buff_in," ");
      if (!strcmp (command, "\\quit")) // want to quit, break the read loop
	break;
      else if (!strcmp (command, "\\ping")) // ping yourself
	send_message_self("pong ;)\r\n", cli->connfd);
      else if(!strcmp (command, "\\name")) { // change name
	param = strtok(NULL, " ");
	if (param) {
	  char *old_name = strdup (cli->name);
	  strcpy (cli->name, param);
	  sprintf (buff_out, "%s is now going by %s\r\n", old_name, cli->name);
	  free (old_name);
	  send_message_all (buff_out);
	}
	else
	  send_message_self ("Response: Name cannot be null\r\n", cli->connfd);
      }
      else if (!strcmp (command, "\\pm")) { // private message
	param = strtok(NULL, " ");
	if (param) {
	  printf("%s", param);
	  int uid = atoi (param);
	  param = strtok (NULL, " ");	  
	  if (param) {
	    sprintf (buff_out, "[PM][%s]", cli->name); // what they'll get
	    while (param != NULL) {
	      strcat (buff_out, " ");
	      strcat (buff_out, param);
	      param = strtok (NULL, " ");
	    }
	    strcat (buff_out, "\r\n");
	    send_message_client (buff_out, uid);
	  }
	  else 
	    send_message_self ("Response: Message cannot be null\r\n", cli->connfd);
	}
	else
	  send_message_self ("Response: Reference cannot be null\r\n", cli->connfd);
      }
      else if (!strcmp (command, "\\active")) { // print active clients
	sprintf (buff_out, "%d connected client(s)\r\n", cli_count);
	send_message_self (buff_out, cli->connfd);
	send_active_clients (cli->connfd);
	
      }

      else if(!strcmp (command, "\\lkick")) { // kick someone
	/*	param = strtok(NULL, " ");
	if (param) {
	  char *old_name = strdup (cli->name);
	  strcpy (cli->name, param);
	  sprintf (buff_out, "%s is now going by %s\r\n", old_name, cli->name);
	  free (old_name);
	  send_message_all (buff_out);
	}
	else*/
	send_message_self ("Response: Not yet implemented.\r\n", cli->connfd);
      }
      
      
      else if (!strcmp (command, "\\help")) { // help menu
	strcat (buff_out, "\\quit     Quit chatroom\r\n");
	strcat (buff_out, "\\ping     Server test\r\n");
	strcat (buff_out, "\\name     <name> Change nickname\r\n");
	strcat (buff_out, "\\pm  <reference> <message> Send private message\r\n");
	strcat (buff_out, "\\active   Show active clients\r\n");
	//	strcat (buff_out, "\\kick     <name> Kick someone (requires admin)\r\n");
	strcat (buff_out, "\\help     Show help\r\n");
	send_message_self (buff_out, cli->connfd);
      }
      else
	// echo back
	send_message_self("Response: Unkown command\r\n", cli->connfd);
    }
    else { 
      // else just send message
      sprintf (buff_out, "[%s] %s\r\n", cli->name, buff_in);
      send_message (buff_out, cli->uid);
    }
  }
  
  // close the connection
  close (cli->connfd);
  sprintf (buff_out, "%s has left the room, bye %s!\r\n", cli->name, cli->name);
  send_message_all (buff_out);
  
  // delete client from queue and yeild thread
  queue_delete (cli->uid);
  free (cli);
  cli_count--;
  pthread_detach (pthread_self());
  
  return NULL;
}
