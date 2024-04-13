#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SA struct sockaddr
/* node struct */
typedef struct {
  int fd_out;
  char *ip;
  int port;
} qflex_node_t;
/* my node id */
static int node_id = -1;
static uint64_t QUANTA;
static int n_nodes;
static qflex_node_t server;
static qflex_node_t *nodes;
static int quanta_id;
static int prev_quanta_id;
static int nodes_boot;
static int nodes_ready;
static int nodes_begin;
static int quanta_begin;
/* number of packet received in the current quanta, it needs to be equal to
 * pkt_sent to move the next quanta */
static int pkts_recv;
static int pkts_sent;
static int serverfd;
static int incoming_connections;
static int nodes_at_boot;
static _Atomic int can_count = 0;

enum HEADERS {
  RDY = 1,   /* node is ready */
  ARDY = 2,  /* ack node is ready */
  NC = 3,    /* new connection */
  BT = 4,    /* boot : DO NOT EDIT (used in afterboot script) */
  BBT = 5,   /* broadcast boot */
  PKT = 6,   /* ack packet */
  BEGIN = 7, /* ack packet */
  ABEGIN = 8 /* ack packet */
};

static int qflex_send_ready(int n) {
  int conv = htonl(n);
  int type = ARDY;

  int arr[] = {type, conv};
  for (int i = 0; i < n_nodes; ++i) {
    if (write(nodes[i].fd_out, &arr, 2 * sizeof(int)) == -1) {
      printf("Can not send number to server\n");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}
static int qflex_send_begin(int n) {
  int conv = htonl(n);
  int type = ABEGIN;

  int arr[] = {type, conv};
  for (int i = 0; i < n_nodes; ++i) {
    if (write(nodes[i].fd_out, &arr, 2 * sizeof(int)) == -1) {
      printf("Can not send number to server\n");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}
static int qflex_send_boot() {
  int type = BBT;

  int arr[] = {type};
  for (int i = 0; i < n_nodes; ++i) {
    if (write(nodes[i].fd_out, &arr, 1 * sizeof(int)) == -1) {
      printf("Can not send number to server\n");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}

static void qflex_move_to_next_quanta(void) {

  /* since I aspect it to be reset */
  assert(nodes_begin == 0 && quanta_id == quanta_begin);

  /* reset */
  nodes_ready = 0;
  pkts_sent = 0;
  pkts_recv = 0;
  prev_quanta_id = quanta_id;
  printf(
      "[-] MOVE TO NEXT QUANTA -> %d\n------------------------------------\n\n",
      quanta_id + 1);

  qflex_send_ready(quanta_id);
  quanta_id++;
}
static void qflex_can_send(void) {
  nodes_begin = 0;
  printf("[-] BEGIN -> %d\n", quanta_id);
  quanta_begin = quanta_id;
  qflex_send_begin(quanta_id);
}
static void qflex_start(void) {
  nodes_boot = 0;
  printf("[-] START\n");
  qflex_send_boot();
}

static void qflex_config_load(char *path) {
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  fp = fopen(realpath(path, NULL), "r");
  if (fp == NULL) {
    printf("Config file does not exist\n");
    exit(-1);
  }

  /* number of nodes */
  n_nodes = 0;
  /* first cycle to get len */
  while ((read = getline(&line, &len, fp)) != -1) {
    n_nodes++;
  }
  n_nodes--;
  nodes = malloc(n_nodes * sizeof(qflex_node_t));
  printf("NODES: %d\n", n_nodes);
  printf("QUANTA: %ld\n", QUANTA);
  fclose(fp);

  fp = fopen(realpath(path, NULL), "r");
  /* reset length, so starting to read file from BOF */
  len = 0;
  line = NULL;
  int c = 0;
  while ((read = getline(&line, &len, fp)) != -1) {
    char *ptr = strtok(line, ";");
    char *ip = malloc(strlen(ptr) * sizeof(char));
    strcpy(ip, ptr);
    ptr = strtok(NULL, ";");
    int port = atoi(ptr);
    /* first line is the server */
    if (c == 0) {
      server.ip = ip;
      server.port = port;
      printf("SERVER IP: %s - PORT: %d\n", server.ip, server.port);
    } else {
      nodes[c - 1].ip = ip;
      nodes[c - 1].port = port;
      printf("NODE IP: %s - PORT: %d\n", nodes[c - 1].ip, nodes[c - 1].port);
    }
    c++;
  }

  fclose(fp);
}

static void *qflex_server_open_thread(void *args) {
  int opt = true;
  struct sockaddr_in servaddr;
  int port = server.port;
  fd_set readfds;
  int activity, valread, sd;
  int max_sd, addrlen, incoming_socket;
  struct sockaddr_in address;
  int buf;

  for (int i = 0; i < n_nodes; i++)
    nodes[i].fd_out = 0;

  // socket create and verification
  serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverfd == -1) {
    printf("socket creation failed...\n");
    exit(0);
  } else
    printf("Socket successfully created..\n");
  bzero(&servaddr, sizeof(servaddr));

  if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                 sizeof(opt)) < 0) {
    exit(EXIT_FAILURE);
  }

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if ((bind(serverfd, (SA *)&servaddr, sizeof(servaddr))) != 0) {
    printf("socket bind failed...\n");
    exit(EXIT_FAILURE);
  }
  printf("Socket successfully binded..\n");

  if ((listen(serverfd, n_nodes * 2)) != 0) {
    printf("Listen failed...\n");
    exit(EXIT_FAILURE);
  }
  printf("Server listening..\n");

  for (;;) {
    // clear the socket set
    FD_ZERO(&readfds);

    // add master socket to set
    FD_SET(serverfd, &readfds);
    max_sd = serverfd;

    // add child sockets to set
    for (int i = 0; i < n_nodes; ++i) {
      // socket descriptor
      sd = nodes[i].fd_out;

      // if valid socket descriptor then add to read list
      if (sd > 0) {
        FD_SET(sd, &readfds);
        // highest file descriptor number, need it for the select function
        if (sd > max_sd)
          max_sd = sd;
      }
    }

    // wait for an activity on one of the sockets , timeout is NULL , so wait
    // indefinitely
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) {
      printf("select error");
    }

    /* there is probably an error with the handling of my self when it comes to
     * master server */
    if (FD_ISSET(serverfd, &readfds)) {
      // printf("SOMETHING SERVER\n");
      /* if the server completed its job, stop here avoiding possible errors */
      if ((incoming_socket = accept(serverfd, (struct sockaddr *)&address,
                                    (socklen_t *)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }
      /* read the header */
      if ((valread = read(incoming_socket, &buf, sizeof(buf))) <= 0) {
        printf("NOTHING\n");
        continue;
      }

      /* if we can count, accept new connections */
      if (buf == NC) {
        /* immediately read the node id */
        int recv_node_id;
        while ((valread = read(incoming_socket, &recv_node_id,
                               sizeof(recv_node_id))) <= 0) {
          // inform user of socket number - used in send and receive commands
        }
        recv_node_id = ntohl(recv_node_id);
        printf("New connection , socket fd is %d , ip is : %s , port : %d , id "
               ": %d\n",
               incoming_socket, inet_ntoa(address.sin_addr),
               ntohs(address.sin_port), recv_node_id);
        nodes[recv_node_id].fd_out = incoming_socket;
        ++incoming_connections;
      } else if (buf == BT) {
        if (++nodes_at_boot == n_nodes) {
          atomic_store(&can_count, 1);
          int v = BBT;
          printf("EMULATION STARTS\n");
          for (int j = 0; j < n_nodes; ++j) {
            write(nodes[j].fd_out, &v, sizeof(v));
          }
        }
      } else {
        printf("unknown %d\n", buf);
      }
    }

    // else its some IO operation on some other socket :)
    for (int i = 0; i < n_nodes; ++i) {
      sd = nodes[i].fd_out;

      if (FD_ISSET(sd, &readfds)) {
        // printf("SOMETHING NODES\n");
        // memset(buffer, 0, 1024);
        // Check if it was for closing , and also read the incoming message
        if ((valread = read(sd, &buf, sizeof(buf))) == 0) {
          // Somebody disconnected , get his details and print
          getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
          printf("Host disconnected , ip %s , port %d \n",
                 inet_ntoa(address.sin_addr), ntohs(address.sin_port));

          close(sd);
          nodes[i].fd_out = 0;
          --incoming_connections;
          /* TODO: close all fds */
          exit(EXIT_FAILURE);
        } else if (valread > 0) {
          int remote = 0;
          switch (buf) {
          case BT:
            if (++nodes_boot == n_nodes) {
              qflex_send_boot();
            }
            break;
          case RDY:
            assert(nodes_boot == 0);
            read(sd, &buf, sizeof(buf));
            remote = htonl(buf);
            read(sd, &buf, sizeof(buf));
            int sent = htonl(buf);
            printf("RDY %d - %d from %d with %d pkts (%d)\n", remote, quanta_id,
                   i, sent, nodes_ready + 1);
            assert(remote == quanta_id);
            ++nodes_ready;
            pkts_sent += sent;
            /* all nodes are ready */
            if (nodes_ready == n_nodes && pkts_recv == pkts_sent) {
              qflex_move_to_next_quanta();
            }
            break;

          case PKT:
            read(sd, &buf, sizeof(buf));
            remote = htonl(buf);
            pkts_recv++;
            printf("PKTS RECV %d - %d (%d - %d) from %d\n", pkts_recv,
                   pkts_sent, remote, quanta_id, i);
            assert(remote == quanta_id);
            if (nodes_ready == n_nodes && pkts_recv == pkts_sent) {
              qflex_move_to_next_quanta();
            }
            break;
          case BEGIN:
            read(sd, &buf, sizeof(buf));
            remote = htonl(buf);
            ++nodes_begin;
            printf("BGN %d - %d from %d (%d)\n", remote, quanta_id, i,
                   nodes_begin);
            assert(remote == quanta_id);
            if (nodes_begin == n_nodes) {
              qflex_can_send();
            }
            break;
          default:
            printf("UNK %d\n", htonl(buf));
          }
        } else {
          printf("[x] ERROR READING\n");
          exit(EXIT_FAILURE);
        }
      }
    }
  }
  printf("exited");
  pthread_exit(NULL);
}
static void qflex_server_open(void) {
  pthread_t t1;
  int res = pthread_create(&t1, NULL, qflex_server_open_thread, NULL);
  if (res) {
    printf("error %d\n", res);
  }
}

static void qflex_clients_open(void) {
  /* connect to all the remote nodes */
  for (int i = 0; i < n_nodes; i++) {
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(nodes[i].ip);
    servaddr.sin_port = htons(nodes[i].port);

    do {
      nodes[i].fd_out = socket(AF_INET, SOCK_STREAM, 0);
      if (nodes[i].fd_out == -1) {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
      }
      if (connect(nodes[i].fd_out, (SA *)&servaddr, sizeof(servaddr)) == 0) {
        break;
      }
      usleep(100);
    } while (true);
    /* send header */
    int msg = NC;
    write(nodes[i].fd_out, &msg, sizeof(msg));
    /* sending my node id */
    msg = htonl(node_id);
    write(nodes[i].fd_out, &msg, sizeof(msg));
    printf("connected to the server..\n");
  }
  /* wait for all nodes to connect to my server */
  while (incoming_connections < n_nodes - 1) {
    usleep(100);
  }
}

int main(int argc, char **argv) {

  QUANTA = atoi(argv[2]);
  qflex_config_load(argv[1]);
  qflex_server_open_thread(NULL);

  (void)qflex_server_open;
  (void)qflex_clients_open;
  return 0;
}
