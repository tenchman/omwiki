#include "didi.h"

static int debug;
int hostlogin; //declared external in wikilogin.c
int nologin; //declared external in wikilogin.c
int dosendmail; //declared external in wikilogin.c
int lgindex; //declared external in wiki.c

int
usage()
{
  fprintf(stderr, "Usage: didiwiki [options]\n");
  fprintf(stderr, "   -a, --autologin       : login localhost automatically\n");
  fprintf(stderr, "   -n, --nologin         : login automatically all users\n");
  fprintf(stderr, "   -d, --debug           : debug mode, listens to requests from stdin\n");
  fprintf(stderr, "   -h, --home   <path>   : specify didiwiki's home directory\n");
  fprintf(stderr, "   -l, --listen <ipaddr> : specify IP address\n");
  fprintf(stderr, "   -p, --port   <port>   : specify port number \n");
  fprintf(stderr, "   -r, --restore         : restore the default WikiHelp\n");
  fprintf(stderr, "       --help            : display this help message\n");
  fprintf(stderr, "   -s, --sendmail        : run sendmail script automatically\n");
  fprintf(stderr, "   -i, --index <length>  : override the default index length\n");
  fprintf(stderr, "   -v, --version         : display the version\n");
  exit(1);
}

static struct addrinfo *addressinfo(char *listenaddr)
{
  struct addrinfo hints;
  struct addrinfo *result;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_CANONNAME;

  if (0 == getaddrinfo(listenaddr, NULL, &hints, &result)) {
    return result;
  }
  return NULL;
}

int
main(int argc, char **argv)
{
  HttpRequest    *req  = NULL;
  int             port = 8000;
  int             c;
  char           *didiwiki_home = NULL;
  int             restore_WikiHelp = 0;
  char           *listenaddr = "0.0.0.0"; /* by default bind server to "0.0.0.0" */
  struct addrinfo *ai;

  /* default values */
  debug = 0; //normal mode
  hostlogin = 0; //host will have to login
  nologin = 0; //users will have to login.
  dosendmail = 0; //don't send systematically email at each registration
  lgindex = 20; //print 20 files before to make a new index box

  while (1)
  {
    static struct option long_options[] =
    {
      {"autologin",  no_argument,   0, 'a'},
      {"nologin",  no_argument,     0, 'n'},
      {"debug",  no_argument,       0, 'd'},
      {"version",  no_argument,     0, 'v'},
      {"listen", required_argument, 0, 'l'},
      {"port",   required_argument, 0, 'p'},
      {"home",   required_argument, 0, 'h'},
      {"restore",   no_argument,    0, 'r'},
      {"sendmail",  no_argument,    0, 's'},
      {"index",  required_argument, 0, 'i'},
      {"help",   no_argument,       0,  10 },
      {0, 0, 0, 0}
    };

    /* getopt_long stores the option index here */
    int option_index = 0;

    c = getopt_long (argc, argv, "adl:p:h:i:rsv", long_options, &option_index);

    /* detect the end of the options */
    if (c == -1)
    break;

    switch (c)
    {
      case 0:
        break;
      case 'i': //set index length
        lgindex = atoi(optarg);
        if (lgindex==0) lgindex=20;
        fprintf(stderr,"Index length = %i\n",lgindex);
        break;
      case 'a': //autologin for the localhost
        hostlogin = 1;
        fprintf(stderr,"Localhost is logged in.\n");
        break;
      case 'n': //autologin any user
        nologin = 1;
        fprintf(stderr,"Any user registrered or not will be logged in.\n");
        break;
      case 'd':
        debug = 1;
        break;
      case 'v':
        printf("CiWiki alias DidiWiki - version %s\n\n",VERSION);
        return 0;
      case 'p': //default port is 8000
        port = atoi(optarg);
        break;
      case 'h': //default home directory is ~/.didiwiki
        didiwiki_home = optarg;
        break;
      case 'l': //listen a inet address
	listenaddr = optarg;
        break;
      case 'r': //rewrite Wikihelp page
        restore_WikiHelp=1;
        break;
      case 's':
        dosendmail= 1;
        break;
      case 10:
        usage();
        break;
      default:
        usage();
    }
  } //end while

  if (NULL == (ai = addressinfo(listenaddr))) {
    printf("didiwiki: invalid ip address \"%s\"\n", listenaddr);
    exit(1);
  }
  wiki_init(didiwiki_home,restore_WikiHelp);

  if (debug)
  {
    req = http_request_new();   /* reads request from stdin */
  }
  else
  {
    req = http_server(ai, port);    /* forks here */
  }

  wiki_handle_http_request(req);

  return 0;
}
