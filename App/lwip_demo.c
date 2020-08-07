#include "wm_include.h"
#include "User/lwip_demo.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/udp.h"

#define UDP_ECHO_PORT 7

const static char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_index_html[] = "<html><head><title>Congrats!</title></head> \
                                       <body><h1>Welcome to LwIP 1.4.1 HTTP server!</h1> \
                                       <center><p>This is a test page based on netconn API.</center></body></html>";

/** Serve one HTTP connection accepted in the http thread */
static void httpserver_serve(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  printf("\n http server is connected \n");
  
  /* Read the data from the port, blocking if nothing yet there. 
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);
  
  if (err == ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    
    /* Is this an HTTP GET command? (only check the first 5 chars, since
    there are other formats for GET, and we're keeping it very simple )*/
    if (buflen>=5 &&
        buf[0]=='G' &&
        buf[1]=='E' &&
        buf[2]=='T' &&
        buf[3]==' ' &&
        buf[4]=='/' ) {
      
      /* Send the HTML header 
             * subtract 1 from the size, since we dont send the \0 in the string
             * NETCONN_NOCOPY: our data is const static, so no need to copy it
       */
      netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
      
      /* Send our HTML page */
      netconn_write(conn, http_index_html, sizeof(http_index_html)-1, NETCONN_NOCOPY);
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);
  
  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}

/** The main function, never returns! */
static void httpserver_thread(void *arg)
{
  printf("\n http server thread start \n");
  struct netconn *conn, *newconn;
  err_t err;
  LWIP_UNUSED_ARG(arg);
  
  /* Create a new TCP connection handle */
  conn = netconn_new(NETCONN_TCP);
  LWIP_ERROR("http_server: invalid conn", (conn != NULL), return;);
  
  /* Bind to port 80 (HTTP) with default IP address */
  netconn_bind(conn, NULL, 80);
  
  /* Put the connection into LISTEN state */
  netconn_listen(conn);
  
  do {
    err = netconn_accept(conn, &newconn);
    if (err == ERR_OK) {
      httpserver_serve(newconn);
      netconn_delete(newconn);
    }
  } while(err == ERR_OK);
 
  LWIP_DEBUGF(HTTPD_DEBUG,
    ("http_server_netconn_thread: netconn_accept received error %d, shutting down",
    err));
  netconn_close(conn);
  netconn_delete(conn);
}

/** Initialize the HTTP server (start its thread) */
void httpserver_init()
{
  printf("\n creat http server thread \n");
  
  tls_os_task_create(NULL,
      "http_server_netconn",
      httpserver_thread,
      NULL,
      NULL,
      512,
      7,
      0);
}

static void udp_demo_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    struct pbuf *q = NULL;
    int recv_datalen = 0;
    u8* recv_buf = malloc(p->tot_len);
    const char * replay_head = "Reply: ";
    u8* out_buf = malloc(strlen(replay_head) + (p->tot_len));

    memset(recv_buf, 0, p->tot_len);
    memset(out_buf, 0, strlen(replay_head) + (p->tot_len));
    
    if(arg)
        printf("%s", (char*)arg);

    for (q = p; q != NULL; q = q->next) {
		memcpy(recv_buf + recv_datalen, q->payload, q->len);
		recv_datalen += q->len;
	}
    pbuf_free(p);
    q = NULL;
    printf("recv_buf: %s\n", (char*)recv_buf);
    
    memcpy(out_buf, replay_head, strlen(replay_head));
    memcpy(out_buf + strlen(replay_head), recv_buf, strlen(recv_buf));
    free(recv_buf);
    printf("out_buf: %s\n", (char*)out_buf);

    q = pbuf_alloc(PBUF_TRANSPORT, strlen(out_buf) + 1, PBUF_RAM);
    if(!q)

    {
        printf("out of PBUF_RAM \n");
        return;
    }

    memset(q->payload, 0, q->len);
    memcpy(q->payload, out_buf, strlen(out_buf));
    free(out_buf);
    printf("pbuf->payload: %s\n", (char*)q->payload);

    udp_sendto(pcb, q, addr, port);
    pbuf_free(q);
}

void udp_demo_init(void)
{
    struct udp_pcb * upcb;

    printf("\n init udp demo \n");
    
    upcb = udp_new();
    udp_bind(upcb, IP_ADDR_ANY, UDP_ECHO_PORT);
    udp_recv(upcb, udp_demo_callback, NULL);
}

