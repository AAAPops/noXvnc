#### noXvnc

VNC viewer which runs on top of Linux Frame Buffer absolutely without X server.  
Partially works. You can see cartoon "Masha and The Bear" at least -)

No **LibVNC** dependecies!

How to build:
```bash
$ mkdir build && cd build
$ cmake ../
$ make

# ./nxvnc ip:N
    ip — IP address of VNC server
    N — number of VNC screen to connect to (You know it when starts Xvnc server)
```

