#include "connexion.hpp"

void setup_server(t_server &s, t_config &config)
{
    (void)config;
    s.addrlen = sizeof(s.address);

    //initialise client sockets to null or inactive
    for (unsigned int i = 0; i < SOMAXCONN; i++)
    {
        s.client_socket[i] = 0;
    }

    //Creating server socket file descriptor
    //AF_INET = IPv4 | AF_INET6 = IPv6
    //SOCK_STREAM = TCP | STOCK_DGRAM = UDP
    //0 is protocol value for IP
    if ((s.server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        std::cout << "Error: socket failed" << std::endl;
        exit(1);
    }

    //set server socket to allow multiple connections
    int opt = 1;
    if(setsockopt(s.server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == -1)
    {
      std::cout << "Error: sotsockopt failed" << std::endl;
      exit(1);
    }
    //type of socket created
    s.address.sin_family = AF_INET; //= IPv4
    s.address.sin_addr.s_addr = INADDR_ANY; //Makes the socket bound to all network interfaces on the host, important when server offers services to multiple networks
    s.address.sin_port = htons(config.port); //decode port adress with htons

    if (bind(s.server_socket, (struct sockaddr *)&s.address, s.addrlen) == -1)
    {
        std::cout << "Error: bind failed" << std::endl;
        P(strerror(errno));
        exit(1);
    }

    std::cout << "Listening on port: ";
    std:: cout << config.port << std::endl;
    //Server socket waits for a client socket to connect
    //SOMAXCONN is constant of max number of client requests we could wait for
    if (listen(s.server_socket, SOMAXCONN) == -1)
    {
        std::cout << "Error: listen failed" << std::endl;
        exit(1);
    }
}

void client_disconnection(t_server &s, unsigned int i)
{
  //Show to debug
  getpeername(s.client_socket[i] , (struct sockaddr*)&s.address , (socklen_t*)&s.addrlen);
  printf("Host disconnected\n-ip: %s\n-port: %d \n" , inet_ntoa(s.address.sin_addr) , ntohs(s.address.sin_port));

  //Close the socket and reset to zero for re-use
  close(s.client_socket[i]);
  s.client_socket[i] = 0;
}

void get_client_request(t_server &s, t_config &config)
{
  (void)config;
  int message_len = -1; //Receive message lenght to add a /0 at end str
  char message_buffer[1025];  //Received message is taken into a char* message_buffer because we use C functions

  for (unsigned int i = 0; i < SOMAXCONN; i++)
  {
      if (FD_ISSET(s.client_socket[i] , &s.active_socket_read)) //If client socket still in active sockets, a request exists from that client
      {
          message_len = recv(s.client_socket[i] , message_buffer, 1024, MSG_PEEK); //Read the incoming message //MSG_PEEK //read whole message
          if (message_len == 0) //If incoming message lenght is equal to 0, the client socket closed connection
            client_disconnection(s, i);
          else
          {
              message_buffer[message_len] = '\0'; //End message buffer with terminating /0
              s.socket_to_answer[s.client_socket[i]] += std::string(message_buffer); //Create key in map with its value
          }
      }
  }
}

void add_new_socket_to_active_sockets(t_server &s)
{
  for (unsigned int i = 0; i < SOMAXCONN; i++)
  {
      if(s.client_socket[i] == 0)
      {
          s.client_socket[i] = s.connected_socket;
          break;
      }
  }
}

void new_incoming_connection(t_server &s, t_config &config)
{
    (void)config;
    //Extracts first connection request in queue and returns new connected socket (server - client)
    if ((s.connected_socket = accept(s.server_socket, (struct sockaddr *)&s.address, (socklen_t*)&s.addrlen)) == -1)
    {
      std::cout << "Error: listen failed" << std::endl;
      exit(1);
    }
    // fcntl(s.connected_socket, F_SETFL, O_NONBLOCK);
    add_new_socket_to_active_sockets(s);
    printf("New connection\n-socket fd: %d\n-ip: %s\n-port: %d\n" , s.connected_socket , inet_ntoa(s.address.sin_addr) , ntohs(s.address.sin_port)); //Show to debug
}

void reset_sockets(t_server &s)
{
  FD_ZERO(&s.active_socket_read); //Deactivate the active sockets
  FD_SET(s.server_socket, &s.active_socket_read); //Add the server socket to active sockets
  FD_ZERO(&s.active_socket_write); //Deactivate the active sockets
  FD_SET(s.server_socket, &s.active_socket_write); //Add the server socket to active sockets

  //Add active client sockets to the active sockets
  for (unsigned int i = 0 ; i < SOMAXCONN ; i++)
  {
      if(s.client_socket[i] > 0)
      {
          FD_SET(s.client_socket[i] , &s.active_socket_read);
          FD_SET(s.client_socket[i] , &s.active_socket_write);
      }
  }
  s.socket_to_answer.clear();
}

void wait_connexion(t_server &s, t_config &config)
{
  (void)config;
  int ret;
  reset_sockets(s);
  //Wait untill a socket gets activated, NULL means wait indefinitely
  //Select helps manipulate multiple active clients (cleaner way of handling it than using threads)
  if ((ret = select(FD_SETSIZE , &s.active_socket_read, &s.active_socket_write, NULL , NULL)) == -1)//check if ready to read and write at same time //changes read and write sets, only keeps the active ones //returns the total
  {
      std::cout << "Error: select failed" << std::endl;
      exit(1);
  }
  else if (ret == 0) //If return is zero timeout happened
    wait_connexion(s, config);
  // else if (ret % 2 != 0) //If read and write are not open at the same time //select returns total number of sockets ready for reading or ready for writing. Thus if those sockets are both ready for reading and writing, the returned value should be pair (divisible by two)
  //   wait_connexion(s, config);
  std::cout << "RET SELECT: " << ret << std::endl;
}
