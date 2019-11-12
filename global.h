#include <zlib.h>

#include <stdint.h>
#include <errno.h>

#define STATUS_OK   1
#define STATUS_NOK -1
#define TIMEOUT     10

int srv_fd;
int kbd_fd;
int mouse_fd;
int fb_fd;

z_stream zStream;


struct prog_arguments {
    char      server[15+1];
    uint16_t  port;
    char      password[255];

    char      fb_name[255];
    char      mouse_name[255];

    int       encodeRaw;
    int       encodeCopyRect;
    int       encodeTight;
    int       sharedflag;
} progArgs;


enum client_msg_type {
    setPixelFormat = 0,
    setEncodings   = 2,
    fbUpdateReq    = 3,
    keyEvent       = 4,
    pointerEvent   = 5,
    clientCutText  = 6
};

enum srv_msg_type {
    framebufferUpdate  = 0,
    setColorMapEntries = 1,
    Bell               = 2,
    serverCutText      = 3
};


// 7.7.  Encodings
enum encodes {
    Raw      = 0,
    copyRect = 1,
    RRE      = 2,
    Hextile  = 5,
    Tight    = 7,
    TRLE     = 15,
    ZRLE     = 16,
    cursorPseudoEnc = -239,
    desctopSizePseudoEnc = -223
};


