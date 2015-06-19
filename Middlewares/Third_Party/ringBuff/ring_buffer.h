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


#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cmsis_os.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ringBufClient {
    uint8_t *ptr;
    uint32_t count;
    uint8_t client_num;
    uint32_t read_size;
    uint32_t shift_size;
    bool overrun;

    osMessageQId msg_id;
    uint32_t msg_val;
} ringBufClient;

typedef struct ringBuf {
    uint8_t *buff;
    uint32_t buff_size;
    uint8_t *head;
    uint32_t count;
    bool can_override;
    osMutexId rw_mutex_id;

    ringBufClient *clients;
    uint32_t num_clients;
    uint8_t client_num_assign;
} ringBuf;


typedef enum {
    BUFF_EMPTY,
    BUFF_FULL,
    BUFF_NOT_ENOGH_SPACE,
    BUFF_NOT_READY,
    BUFF_NO_CLIENTS,
    BUFF_NOT_A_CLIENT,
    BUFF_ERROR,
    BUFF_OK,
} ringBufStatus;


ringBufStatus ringBuf_init ( ringBuf* _this, const size_t buff_size, bool can_override );
ringBufStatus ringBuf_deinit ( ringBuf *_this );
ringBufStatus ringBuf_flush ( ringBuf *_this );

ringBufStatus ringBuf_registClient ( ringBuf *_this, size_t read_size, size_t shift_size, osMessageQId msg_id, uint32_t msg_val, uint8_t *client_num );
ringBufStatus ringBuf_unregistClient ( ringBuf *_this, const uint8_t client_num );
ringBufStatus ringBuf_shift_Client ( ringBuf *_this, const uint8_t client_num, size_t shift_size );
	
ringBufStatus ringBuf_write ( ringBuf* _this, const uint8_t* input, const size_t write_size );
ringBufStatus ringBuf_read_const ( ringBuf *_this, uint8_t client, uint8_t *output );
ringBufStatus ringBuf_read_var ( ringBuf *_this, uint8_t client, size_t read_size, size_t shift_size, uint8_t *output );
ringBufStatus ringBuf_read_all ( ringBuf *_this, uint8_t client, uint8_t *output, size_t *read_size );

bool is_ringBuf_init			( ringBuf *_this );
bool is_ringBuf_empty 		( ringBuf *_this );
bool is_ringBuf_full 			( ringBuf *_this );
bool has_ringBuf_clients	( ringBuf *_this );

static void ringBuf_updateCount ( ringBuf *_this );
static ringBufStatus ringBuf_read ( ringBuf* _this, uint8_t** ptr, uint32_t* count, size_t read_size, size_t shift_size, uint8_t* output );
static ringBufStatus ringBuf_findClient ( ringBuf *_this, const uint8_t client, uint8_t *idx );


#ifdef __cplusplus
}
#endif


#endif	// RING_BUFFER_H
