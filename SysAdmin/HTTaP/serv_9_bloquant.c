/* serv_9_blocant.c */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

#define PORT_NUMBER "60000"
#define KEEPALIVE    15  /* en secondes */
#define REQUETES 1 /* Empêche plus d'une connexion à la fois */
#define TAMPON 4000

#define GHDL_TCPPORT    ("GHDL_TCPPORT")
#define GHDL_KEEPALIVE  ("GHDL_KEEPALIVE")
#define GHDL_STATICPATH ("GHDL_STATICPATH")
#define GHDL_ROOTPAGE   ("GHDL_ROOTPAGE")
#define GHDL_USER       ("GHDL_USER")
#define GHDL_GROUP      ("GHDL_GROUP")

enum liste_des_etats {
  ETAT0_initialisation,
  ETAT1_attente_connexion,
  ETAT2_attente_donnee_entrante
} etat_serveur=ETAT0_initialisation;


const char *err400="HTTP/1.0 400 Invalid\x0d\x0a\
Content-Type: text/html\x0d\x0a\
Content-Length: 34\x0d\x0a\
\x0d\x0a\
<html><body>Mauvaise requete</body><html>";

const char *LED_On="HTTP/1.0 200 OK\x0d\x0a\
Content-Type: text/html\x0d\x0a\
Content-Length: 42\x0d\x0a\
\x0d\x0a\
<html><body>LED allum&eacute;e</body><html>";

const char *LED_Off="HTTP/1.0 200 OK\x0d\x0a\
Content-Type: text/html\x0d\x0a\
Content-Length: 42\x0d\x0a\
\x0d\x0a\
<html><body>LED &eacute;teinte</body><html>";

void *my_getenv(char *s) {
  s=getenv(s);
  if (s && s[0])
    return s;
  return NULL;
}

/* sorties avec un message d'erreur contextualisé */
void erreur(char *msg) {
  fflush(NULL);
  perror(msg);
  exit(EXIT_FAILURE);
}
void err(char *msg) {
  fflush(NULL);
  printf(msg);
  exit(EXIT_FAILURE);
}

/* variable persistentes du serveur */
int sock_serveur, sock_client, keepalive,
    mode_polled, fcntl_flags;
const int one = 1, zero=0; /* pour setsockopt() */
struct sockaddr_storage sockaddr_serveur, sockaddr_client;
socklen_t taille_client = sizeof(sockaddr_client);
char buffer[TAMPON];
char *chemin_statique, *page_racine;
uid_t prog_uid;
struct stat stat_buf;

void serveur_polled() {
  mode_polled=MSG_DONTWAIT;
  fcntl(sock_serveur, F_SETFL, fcntl_flags | O_NONBLOCK);
}

void serveur_blocant() {
  mode_polled=0;
  fcntl(sock_serveur, F_SETFL, fcntl_flags);
}

void serveur() {

  /* variables locales du serveur, non persistantes */
  struct addrinfo hints, *liste_adresses, *p_adresse,
              *aiv4, *aiv6, *choix_ai;
  int len, flags, sockV4, sockV6, recv_len;
  char *env_port, *b, *env_user, *env_group;
  struct passwd *pswd=NULL;
  struct group *grp=NULL;

  switch(etat_serveur) {
/********************************************************/
    case ETAT0_initialisation :

/* Lecture de la configuration */

      /* gestion du numéro du port */
      env_port = my_getenv(GHDL_TCPPORT);
      if (!env_port)
        env_port=PORT_NUMBER;

      /* Durée de persistance de la connexion */
      keepalive = KEEPALIVE;
      b = my_getenv(GHDL_KEEPALIVE);
      if (b) {
        keepalive = atol(b);
        if ((keepalive < 3) || (keepalive > 200))
          err("GHDL_KEEPALIVE doit être entre 3 et 200");
      }
      printf("Port: %s, Keepalive: %ds\n",
           env_port, keepalive);

      /* Chemin vers le répertoire des fichiers statiques */
      chemin_statique = my_getenv(GHDL_STATICPATH);
      if (!chemin_statique)
        chemin_statique = "."; // Le répertoire courant

      /* Nom du fichier pour l'URI racine (habituellement index.html) */
      page_racine = my_getenv(GHDL_ROOTPAGE);
      if (!page_racine)
        page_racine = "index.html";
      printf("Chemin: %s  Page racine : %s\n",
           chemin_statique, page_racine);

      /* détecte l'utilisateur et le groupe */
      pswd=NULL; // pas besoin de les initialiser à NULL à chaque appel
      grp=NULL;
      env_user = my_getenv(GHDL_USER);
      if (env_user) {
        errno = 0;
        pswd = getpwnam(env_user);
        if (!pswd)
          erreur("$GHDL_USER invalide ");

        printf("User: %s", env_user);
        prog_uid = pswd->pw_uid;

        env_group = my_getenv(GHDL_GROUP);
        if (env_group) {
          errno = 0;
          grp = getgrnam(env_group);
          if (!grp)
            erreur("$GHDL_GROUP invalide ");
          printf(", Group: %s", env_group);
        }
      }
      else
        prog_uid = getuid();
      printf("\n");

      /* Vérifie le chemin pour accéder aux fichiers statiques */
      if (stat(chemin_statique, &stat_buf)
          || !S_ISDIR(stat_buf.st_mode))
        erreur("$GHDL_STATICPATH : Chemin invalide");
      if (stat_buf.st_uid != prog_uid)
        err("L'utilisateur n'est pas propriétaire de $GHDL_STATICPATH\n");
      if (chdir(chemin_statique))
        erreur("Echec de déplacement vers $GHDL_STATICPATH");
      if (chroot(chemin_statique))
        perror("Warning: Echec de chroot() ");
      /* Vérifie le fichier racine */
      if (stat(page_racine, &stat_buf)
          || !S_ISREG(stat_buf.st_mode))
        erreur("$GHDL_ROOTPAGE : Fichier invalide");
      if (stat_buf.st_uid != prog_uid)
        err("L'utilisateur n'est pas propriétaire de $GHDL_ROOTPAGE\n");

      /* change le groupe PUIS l'utilisateur (après avoir chrooté) */
      if (pswd){ // GHDL_USER existe
        if (grp) {
          if (setgid(grp->gr_gid))
            erreur ("Echec de changement de groupe ");
        }
        if (setuid(pswd->pw_uid))
          erreur ("Echec de changement d'utilisateur ");
      }

/* ouverture de la socket */

      /* décrit le type de socket que nous voulons ouvrir */
      memset(&hints, 0, sizeof(hints)); /* vidange préliminaire */
      hints.ai_family = AF_UNSPEC; /* IPv4 ou 6, comme ça lui chante */
      hints.ai_socktype = SOCK_STREAM; /* TCP */
      hints.ai_flags = AI_PASSIVE; /* mode serveur */
      if ((flags=getaddrinfo(NULL, env_port, &hints, &liste_adresses)) < 0) {
        printf("getaddrinfo: %s\n", gai_strerror(flags));
        exit(EXIT_FAILURE);
      }

      sockV4=-1; sockV6=-1;
      p_adresse = liste_adresses;
      /* Balaie la liste d'adresses et tente de créer des sockets */
      while (p_adresse) {
        if (p_adresse->ai_family == AF_INET6) { /* IPV6 */
          if (sockV6 < 0) {
            if ((sockV6 = socket(p_adresse->ai_family,
                p_adresse->ai_socktype, p_adresse->ai_protocol)) >= 0) {
              aiv6 = p_adresse;
              printf("IPv6 found, ");
            }
          }
        }
        else {
          if (p_adresse->ai_family == AF_INET) { /* IPv4 */
            if (sockV4 < 0) {
              if ((sockV4 = socket(p_adresse->ai_family,
                  p_adresse->ai_socktype, p_adresse->ai_protocol)) >= 0) {
                aiv4 = p_adresse;
                printf("IPv4 found, ");
              }
            }
          }  /* else : protocole inconnu, on essaie le suivant */
        }
        p_adresse=p_adresse->ai_next; /* avance dans la liste chaînée */
      }

      /* sélection de la socket : */
      if (sockV6 >= 0) {
        choix_ai = aiv6;
        sock_serveur = sockV6;
        /* Tente d'activer la traduction IPv6->IPv4 */
        if (setsockopt(sockV6, IPPROTO_IPV6,
              IPV6_V6ONLY, &zero, sizeof(zero)) < 0) {
          perror("notice : setsockopt(IPV6_V6ONLY)");
          /* Si la traduction n'est pas possible, on essaie de passer en IPv4 */
          if (sockV4 >= 0) {
            printf("close(V6)");
            close(sockV6);
          }
        }
        else {    /* traduction possible : on ferme l'IPv4*/
          if (sockV4 >= 0) {
            printf("v4 over v6 => close(v4)");
            close(sockV4);
            sockV4=-1;
          }
        }
      }

      /* pas d'IPv6 ou pas de traduction IPv6 */
      if (sockV4 >= 0) {
        choix_ai = aiv4;
        sock_serveur = sockV4;
      }
      else
        if (sockV6 < 0)
          err("Aucune socket convenable choisie");

      printf("\n");

#ifdef SO_REUSEADDR
      if (setsockopt(sock_serveur, SOL_SOCKET,
                SO_REUSEADDR, &one, sizeof(one)) < 0)
        perror("setsockopt(SO_REUSEADDR)");
#endif
#ifdef SO_REUSEPORT
      if (setsockopt(sock_serveur, SOL_SOCKET,
                 SO_REUSEPORT, &one, sizeof(one)) < 0)
        perror("setsockopt(SO_REUSEPORT)");
#endif

      /* Branche la socket */
      if (bind(sock_serveur, choix_ai->ai_addr,
                             choix_ai->ai_addrlen) < 0)
        erreur("Echec de bind() du serveur");

      /* Ecoute la socket */
      if (listen(sock_serveur, REQUETES) < 0)
        erreur("Echec de listen() du serveur");

      /* Débloque la socket */
      fcntl_flags = fcntl(sock_serveur, F_GETFL);
      serveur_polled();

      /* libère la mémoire occupée par la liste */
      freeaddrinfo(liste_adresses);

      etat_serveur=ETAT1_attente_connexion;

/********************************************************/
    case ETAT1_attente_connexion:
      /* Attente d'une connexion */
      if ((sock_client = accept(sock_serveur,
         (struct sockaddr *) &sockaddr_client, &taille_client)) < 0) {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
          return;
        else
          erreur("accept() : Echec d'attente de connexion");
      }

      /* Désactive Nagle */
      if (setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
        erreur("setsockopt(TCP_NODELAY)");

      getnameinfo((struct sockaddr *) &sockaddr_client, taille_client,
         buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST);
      b=buffer;
      if (memcmp("::ffff:",buffer,7)==0)
        b+=7; // saute le préfixe IPv6
      printf("Client connecté de %s\n",b);

      etat_serveur= ETAT2_attente_donnee_entrante;

/********************************************************/
    case ETAT2_attente_donnee_entrante :
      /* attend quelque chose */
      if ( (recv_len=recv(sock_client, buffer, TAMPON, mode_polled))
              < 0) {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
          return;
        else
          erreur("Echec de réception du message du client");
      }

      /* analyse de la requête */
      if ((recv_len >= 7) && (strncmp("GET /1 ", buffer, 7) == 0)) {
        printf("Allumage de la LED\n");
        b=LED_On;
        serveur_blocant();
      }
      else {
        if ((recv_len >= 7) && (strncmp("GET /0 ", buffer, 7) == 0)) {
          printf("Extinction de la LED\n");
          b=LED_Off;
          serveur_polled();
        }
        else {
          printf("Requête invalide\n");
          b=err400;
        }
      }

      /* Réponse : */
      len=strlen(b);
      if (send(sock_client, b, len, 0) != len)
        erreur("Echec d'envoi du message");

      close(sock_client);
      printf("Fermeture de la connexion\n");

      etat_serveur=ETAT1_attente_connexion;
      return;

/********************************************************/
    default: err("état interdit !");
  }
}

#ifndef EMBEDDED_SERVER
/* fonction principale qui appelle le serveur périodiquement */
int main(int argc, char *argv[]) {
  const char spinner[4]="-\\|/";
  int phase=0;
  while (1) {
    usleep(100*1000);   /* attente de 100ms */
    printf("%c%c", spinner[(phase++)&3],0xd);
    fflush(NULL);
    serveur();
  }
}
#endif