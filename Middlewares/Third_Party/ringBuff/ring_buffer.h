/**
  ******************************************************************************
  * @file    ring_buffer.h
  * @author  Alejandro Alvarez
  * @version V1.0.0
  * @date    22-Mayo-2014
  * @brief   Audio Functions for ASR_DTW
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
	*/

/* Includes ------------------------------------------------------------------*/


#ifndef __ringBuf_H
#define __ringBuf_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cmsis_os.h>


typedef enum
{
	RING_BUFFER_READY,
}	ringBufMsgs;

typedef struct ringBufClient {
    uint8_t *ptr;
    uint32_t count;
    uint8_t client_num;
    uint32_t read_size;
    uint32_t shift_size;
    bool overrun;
	
	  osMessageQId msg_id;
} ringBufClient;

typedef struct ringBuf {
    uint8_t *buff;
    uint32_t buff_size;
    uint8_t *head;
    uint32_t count;
    bool can_override;
    
    ringBufClient *clients;
    uint32_t num_clients;
    uint8_t client_num_assign;
} ringBuf;


typedef enum ringBufStatus {
    BUFF_EMPTY,
    BUFF_FULL,
    BUFF_ERROR,
    BUFF_NOT_READY,
    BUFF_NOT_A_CLIENT,
    BUFF_OK,
} ringBufStatus;

#ifdef __cplusplus
extern "C" {
#endif

ringBufStatus ringBuf_init ( ringBuf* _this, const size_t buff_size, bool can_override );
ringBufStatus ringBuf_deinit ( ringBuf *_this );
ringBufStatus ringBuf_flush ( ringBuf *_this );

static ringBufStatus ringBuf_read ( ringBuf* _this, uint8_t** ptr, uint32_t* count, size_t read_size, size_t shift_size, uint8_t* output );
ringBufStatus ringBuf_write ( ringBuf* _this, const uint8_t* input, const size_t write_size );

ringBufStatus ringBuf_registClient ( ringBuf *_this, size_t read_size, size_t shift_size, osMessageQId msg_id, uint8_t *client_num );
ringBufStatus ringBuf_unregistClient ( ringBuf *_this, const uint8_t client_num  );
ringBufStatus ringBuf_findClient ( ringBuf *_this, const uint8_t client, uint8_t *idx );

ringBufStatus ringBuf_read_const ( ringBuf *_this, uint8_t client, uint8_t *output );
ringBufStatus ringBuf_read_var ( ringBuf *_this, uint8_t client, size_t read_size, size_t shift_size, uint8_t *output );
ringBufStatus ringBuf_read_all ( ringBuf *_this, uint8_t client, uint8_t *output, size_t *read_size );
	
bool  is_ringBuf_empty ( ringBuf *_this );
bool  is_ringBuf_full ( ringBuf *_this );

#ifdef __cplusplus
}
#endif


#endif	// __ringBuf_H
