#ifndef CONTAINER_H_
#define CONTAINER_H_

#include <stdio.h>

typedef enum { 
CONTAINER_INIT, 
CONTAINER_ADD, 
CONTAINER_CAPABILITIES, 
CONTAINER_PLAY, 
CONTAINER_STOP, 
CONTAINER_SEEK, 
CONTAINER_SEEK_ABS, 
CONTAINER_LENGTH, 
CONTAINER_DEL, 
CONTAINER_SWITCH_AUDIO, 
CONTAINER_SWITCH_SUBTITLE, 
CONTAINER_INFO, 
CONTAINER_STATUS, 
CONTAINER_LAST_PTS, 
CONTAINER_DATA,
CONTAINER_SET_BUFFER_SEEK_TIME,
CONTAINER_SET_BUFFER_SIZE,
CONTAINER_GET_BUFFER_SIZE,
CONTAINER_GET_BUFFER_STATUS,
CONTAINER_STOP_BUFFER
} ContainerCmd_t;

typedef struct Container_s {
    char * Name;
    int (* Command) (/*Context_t*/void  *, ContainerCmd_t, void *);
    char ** Capabilities;

} Container_t; 

extern Container_t FFMPEGContainer;

typedef struct ContainerHandler_s {
    char * Name;
    Container_t * selectedContainer;

    int (* Command) (/*Context_t*/void  *, ContainerCmd_t, void *);
} ContainerHandler_t;

#endif
