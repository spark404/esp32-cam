//
// Created by Hugo Trippaers on 17/04/2021.
//

#ifndef ESPCAM_PORT_H
#define ESPCAM_PORT_H

int32_t networkSend( NetworkContext_t * pContext, const void * pBuffer, size_t bytes );
int32_t networkRecv( NetworkContext_t * pContext, void * pBuffer, size_t bytes );

uint32_t getTimeStampMs();


#endif //ESPCAM_PORT_H
