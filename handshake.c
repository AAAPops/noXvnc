#define pr_fmt(fmt) "  _" fmt
#define DEBUG_HNDSHAKE  0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <crypt.h>

#include "global.h"
#include "logerr.h"
#include "stream.h"
#include "utils.h"
#include "d3des.h"


struct srv_init_data {
    uint16_t fb_x;
    uint16_t fb_y;
    uint32_t name_len;
    char     name_str[255];
} srvInitData;


struct pixel_format_data {
    uint8_t   bpp;
    uint8_t   depth;
    uint8_t   big_endian_flag;
    uint8_t   true_color_flag;
    //uint16_t  red_max;
    //uint16_t  green_max;
    //uint16_t  blue_max;
    //uint8_t   red_shift;
    //uint8_t   green_shift;
    //uint8_t   blue_shift;
} pixelFormat;

enum sectype {
    Invalid = 0,
    None    = 1,
    VncAuth = 2
} secAuthType;


/*  7.1.1.  ProtocolVersion Handshake  */
int handshake_proto_ver() {
    if (DEBUG_HNDSHAKE) printf("-----\n%s() \n", __func__);

    char srv_msg[12];
    char client_msg[12] = "RFB 003.008\n";

    int nbytes_read;

    /* Get protocol version from server */
    nbytes_read = stream_getN_bytes(srv_msg, sizeof(srv_msg), 0);
    {
        debug_cond(DEBUG_HNDSHAKE, "Get 'Protocol Version':");
        mem2hex(DEBUG_HNDSHAKE, srv_msg, nbytes_read, 30);
    }
    if( nbytes_read != 12 ) {
        fprintf(stderr, "  ...False \n");
        err_exit_2((char *)__func__);
    }

    srv_msg[11] = '\0';
    fprintf(stderr, "  Server's Proto Version: '%s'", srv_msg);

    /* A ton of checks have to be done here about incoming ProtocolVersion
     * But I just send back message 'RFB 003.008\n' */
    stream_send_all(client_msg, sizeof(client_msg));
    fprintf(stderr, "  ...Ok \n");

    return STATUS_OK;
}


/*  7.1.2.  Security Handshake  */
int handshake_sec() {
    if (DEBUG_HNDSHAKE) printf("-----\n%s() \n", __func__);

    char number_security_types[1];
    char security_types[10];
    int nbytes_read;


    /* Get enumeration of security Auth types from server */
    nbytes_read = stream_getN_bytes(number_security_types, 1, 0);
    {
        debug_cond(DEBUG_HNDSHAKE, "Get 'Number of Security types':");
        mem2hex(DEBUG_HNDSHAKE, number_security_types, 1, 16);
    }

    nbytes_read = stream_getN_bytes(security_types, number_security_types[0], 0);
    {
        debug_cond(DEBUG_HNDSHAKE, "Get 'Auth Types List':");
        mem2hex(DEBUG_HNDSHAKE, security_types, number_security_types[0], 16);
    }

    fprintf(stderr, "  Server offer security type: ");
    int iter;
    for( iter = 0; iter < number_security_types[0]; iter++ ) {
        switch( security_types[iter] ) {
            case None:
                fprintf(stderr, " None(1)");
            break;

            case VncAuth: {
                fprintf(stderr, " VncAuth(2)");

                char vncauth = VncAuth;
                stream_send_all(&vncauth, 1);

                /* 7.2.2.  VNC Authentication */
                /* Get 16-byte challenge from server */
                unsigned char challenge[16];
                nbytes_read = stream_getN_bytes((char *)challenge, 16, 0);
                {
                    debug_cond(DEBUG_HNDSHAKE, "\n");
                    debug_cond(DEBUG_HNDSHAKE, "Get '16-byte challenge from server':");
                    mem2hex(DEBUG_HNDSHAKE, (char *)challenge, nbytes_read, 16);
                }
                if (nbytes_read != 16) {
                    fprintf(stderr, "  It doesn't look like DES challenge \n");
                    err_exit();
                }

                unsigned char key[16];
                for (int i = 0; i < 8; i++)
                    key[i] = i < strlen(progArgs.password) ? progArgs.password[i] : 0;
                deskey(key, EN0);

                unsigned char secResult[16];
                for (int iter2 = 0; iter2 < 16; iter2 += 8)
                    des(challenge + iter2, secResult + iter2);
                stream_send_all(secResult, 16);
            }
            break;

            case Invalid:
            default:
                fprintf(stderr, " Invalid(0)");
            break;
        }

    }

    /* 7.1.3.  SecurityResult Handshake */
    /* Get Security Handshake status from server*/
    char security_result[4];
    nbytes_read = stream_getN_bytes(security_result, 4, 0);

    if( ntohl( *(uint32_t *)security_result ) != 0 ) {
        fprintf(stderr, "  ...Auth Failure \n");
        err_exit();
    }
    fprintf(stderr, "  ...Auth Success \n");

    return STATUS_OK;
}


/* 7.3.1.  ClientInit */
int handshake_client_init() {
    if (DEBUG_HNDSHAKE) printf("-----\n%s() \n", __func__);;
    char shared_flag;

    if( progArgs.sharedflag )
        shared_flag = 0x1;
    else
        shared_flag = 0x0;

    /* Send to the server info about session type: Shared(non-zero) or Exclusive(0) */
    stream_send_all(&shared_flag, 1);
    {
        debug_cond(DEBUG_HNDSHAKE, "Send 'Shared Flag' to server:");
        mem2hex(DEBUG_HNDSHAKE, &shared_flag, 1, 16);
    }

    return STATUS_OK;
}


/* 7.3.2.  ServerInit  */
int handshake_server_init() {
    if (DEBUG_HNDSHAKE) printf("-----\n%s() \n", __func__);
    ssize_t nbytes_read;
    char srv_init_msg[24] = {0};
    char desktop_name[1024] = {0};

    /* This tells the client the width and height of the server's framebuffer,
     * its pixel format, and the name associated with the desktop */
    nbytes_read = stream_getN_bytes(srv_init_msg, 24, 0);
    {
        debug_cond(DEBUG_HNDSHAKE, "Get 'ServerInit' msg:");
        mem2hex(DEBUG_HNDSHAKE, srv_init_msg, nbytes_read, 50);
    }

    srvInitData.fb_x = ntohs( *(uint16_t*) srv_init_msg );
    srvInitData.fb_y = ntohs( *(uint16_t*)(srv_init_msg + 2) );
    srvInitData.name_len = ntohl( *(uint32_t *)(srv_init_msg + 20) );

    nbytes_read = stream_getN_bytes(desktop_name, srvInitData.name_len, 0);
    strcpy(srvInitData.name_str, desktop_name);

    fprintf(stderr, "  Server's framebuffer size: %dx%d,", srvInitData.fb_x, srvInitData.fb_y);
    fprintf(stderr, "  Desktop name [%d]: '%s' \n", srvInitData.name_len, srvInitData.name_str);


    pixelFormat.bpp = *(srv_init_msg  + 4);
    pixelFormat.depth = *(srv_init_msg  + 5);
    pixelFormat.big_endian_flag = *(srv_init_msg + 6);
    pixelFormat.true_color_flag = *(srv_init_msg + 7);

    fprintf(stderr, "  Using pixel format: depth %d (%dbpp) %s %s,",
            pixelFormat.depth, pixelFormat.bpp,
            pixelFormat.big_endian_flag != 0 ? "big_endian" : "little_endian",
            pixelFormat.depth == 24 ? "rgb888" : "rgb???" );
    fprintf(stderr, "  true_color_flag: %d\n", pixelFormat.true_color_flag);

    return STATUS_OK;
}




int handshake_with_srv() {
    if (DEBUG_HNDSHAKE) printf("-----\n%s() \n", __func__);

    if( handshake_proto_ver() < 0 )
        err_exit();

    if( handshake_sec() < 0 )
        err_exit();

    if(  handshake_client_init() < 0 )
        err_exit();

    if( handshake_server_init() < 0 )
        err_exit();

    return STATUS_OK;
}