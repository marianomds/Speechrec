/**
  ******************************************************************************
  * @file    ring_bufffer.c
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
#include <ring_buffer.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "stm32f4xx_hal.h"

#undef assert
#define assert(expr) assert_param(expr)

//#ifdef FREERTOS

//#define malloc(size) pvPortMalloc(size)
//#define free(ptr) pvPortFree(ptr)

//#endif


ringBufStatus ringBuf_init ( ringBuf* _this, const size_t buff_size, bool can_override )
{
    // Alloco memoria para el bufffer
    _this->buff = malloc ( buff_size );

    // Inicializo las variables
    _this->head = _this->buff;
    _this->buff_size = buff_size;
    _this->can_override = can_override;
    _this->num_clients = 0;
    _this->clients = NULL;
    _this->count = 0;
    _this->client_num_assign = 0;
    
    return BUFF_OK;
}

ringBufStatus ringBuf_deinit ( ringBuf *_this )
{
    free ( _this->buff );
    _this->buff  = NULL;
    
    free(_this->clients);
    _this->clients = NULL;

    _this->buff_size = 0;
    _this->head = NULL;
    _this->count = 0;
    _this->client_num_assign = 0;
    
    return BUFF_OK;
}

ringBufStatus ringBuf_flush ( ringBuf *_this )
{
    _this->count = 0;
    _this->head	 = _this->buff;
    
    for(int i=0; i<_this->num_clients; i++)
    {
        _this->clients[i].count=0;
        _this->clients[i].ptr = _this->buff;
    }

    return BUFF_OK;
}

ringBufStatus ringBuf_registClient ( ringBuf* _this, size_t read_size, size_t shift_size, osMessageQId msg_id, uint8_t* client_num )
{
    // Me fijo que lo que quiera leer no sea mayor al tamaño del buffer
    assert(_this->buff_size > read_size);
    assert(_this->buff_size > shift_size);
    
    _this->num_clients++;
    *client_num = ++_this->client_num_assign;
    
    // Alloco memoria para el cliente
    if (_this->num_clients > 1)
        _this->clients = realloc(_this->clients, _this->num_clients * sizeof(ringBufClient));
    else
        _this->clients = malloc(sizeof(ringBufClient));   
    
    // Inicializo las variables del cliente
    _this->clients[_this->num_clients-1].client_num = *client_num;
    _this->clients[_this->num_clients-1].count = 0;
    _this->clients[_this->num_clients-1].ptr = _this->head;
    _this->clients[_this->num_clients-1].overrun = false;
    _this->clients[_this->num_clients-1].read_size = read_size;
    _this->clients[_this->num_clients-1].shift_size = shift_size;
		_this->clients[_this->num_clients-1].msg_id = msg_id;
		
    return BUFF_OK;
}

ringBufStatus ringBuf_unregistClient ( ringBuf *_this, const uint8_t client_num  )
{
    // Resto la cantidad de clientes
    _this->num_clients--;
    
    if (_this->num_clients > 0)
    {
        ringBufClient *aux = _this->clients;

        // Creo un nuevo espacio de memoria
        _this->clients = malloc(_this->num_clients * sizeof(ringBufClient));
        
        // Copio la info de todos los clientes menos del que estoy des-registrando
        memcpy(_this->clients, aux, (client_num-1) * sizeof(ringBufClient));
        memcpy(&_this->clients[client_num], &aux[client_num+1], (_this->num_clients+1 - client_num) * sizeof(ringBufClient));
        
        // libero la memoria vieja
        free(aux);
    }
    else
        free(_this->clients);

    
    return BUFF_OK;
}

ringBufStatus ringBuf_findClient ( ringBuf *_this, const uint8_t client, uint8_t *idx )
{
    // Busco el cliente
    uint8_t i = 0;
    bool client_found = false;
    for(; i < _this->num_clients; i++)
    {
        if (_this->clients[i].client_num == client)
        {
            client_found = true;
            break;
        }
    }        
    if(!client_found)
        return BUFF_NOT_A_CLIENT;
    
    *idx = i;
    
    return BUFF_OK;
}

bool is_ringBuf_empty ( ringBuf *_this )
{
    return ( 0 == _this->count );
}

bool is_ringBuf_full ( ringBuf *_this )
{
    return ( _this->count >= _this->buff_size );
}

ringBufStatus ringBuf_write ( ringBuf *_this, const uint8_t *input, const size_t write_size )
{
    // Si la cantidad de datos a copiar es más grande que el tamaño del buffer
    // genero un assert porque se estarían sobreescribiendo datos antes de poder
    // leerlos
    assert(_this->buff_size > write_size);
    
    // Pregunto si el buffer esta lleno o si puedo sobreescribir
    if ( is_ringBuf_full ( _this ) && !_this->can_override )
        return BUFF_FULL;

    // Obtengo el espacio que queda hacia la derecha del buffer
    uint32_t right_space = _this->buff_size -  (_this->head - _this->buff);
    
    // Pregunto si lo que hay para escribir es menor que lo que queda hacia la derecha
    if( write_size < right_space )
    {
        // Copio el dato
        memcpy ( _this->head, input, write_size );
        
        // Actualizo el puntero
        _this->head += write_size;
    }
    else
    {
        // Copio una parte y después el resto al principio del buffer
        memcpy ( _this->head, input, right_space );
        memcpy( _this->buff, input, write_size - right_space);
        
        // Actualizo el puntero
        _this->head = &_this->buff[ write_size - right_space ];
    }

    // Incremento el contador
    _this->count += write_size;
    
    // Update info de clientes
    for(uint8_t idx=0; idx < _this->num_clients ; idx++)
    {
        // Incremento el contador
        _this->clients[idx].count += write_size;
        
        // Chequeo overrun
        if(_this->clients[idx].count > _this->buff_size)
            _this->clients[idx].overrun = true;
				
				if(_this->clients[idx].msg_id != NULL)
					osMessagePut(_this->clients[idx].msg_id, RING_BUFFER_READY, 0);
    }

    return BUFF_OK;
}

ringBufStatus ringBuf_read ( ringBuf *_this, uint8_t **ptr, uint32_t *count, size_t read_size, size_t shift_size, uint8_t *output )
{
    // Me fijo que lo que quiera leer no sea mayor al tamaño del buffer
    assert(_this->buff_size > read_size);    
    
    // Me fijo que se puedan leer la cantidad de datos que se piden
    if ( *count < read_size )
        return BUFF_NOT_READY;
    
    // Obtengo el espacio que queda hacia la derecha del buffer
    uint32_t right_space = _this->buff_size -  (*ptr - _this->buff);
    
    // Pregunto si lo que se quiere leer es menor que lo que hay a la derecha
    if( read_size < right_space )
    {
        // Copio el dato
        memcpy ( output, *ptr, read_size );
    }
    else
    {
        // Copio una parte y después leo desde el principo del buffer
        memcpy ( output, *ptr, right_space );
        memcpy ( &output[right_space], _this->buff, read_size - right_space);
    }
    
    // Actualizo el puntero (no con la cantidad de datos que leyó, sino con lo que quiera shiftear)
    if(shift_size < right_space)
        *ptr += shift_size;
    else
        *ptr = &_this->buff[ shift_size - right_space ];
    
    // Decremento el contador
    *count -= shift_size;
    
    return BUFF_OK;
}

ringBufStatus ringBuf_read_var ( ringBuf *_this, uint8_t client, size_t read_size, size_t shift_size, uint8_t *output )
{
    // Busco el cliente
    uint8_t idx = 0;
    if (ringBuf_findClient ( _this,client, &idx ) == BUFF_NOT_A_CLIENT)
        return BUFF_NOT_A_CLIENT;
    
    // Leo
    ringBuf_read(_this, &_this->clients[idx].ptr, &_this->clients[idx].count, read_size, shift_size, output);
    
    return BUFF_OK;
}

ringBufStatus ringBuf_read_const ( ringBuf *_this, uint8_t client, uint8_t *output )
{
    // Busco el cliente
    uint8_t idx = 0;
    if (ringBuf_findClient ( _this,client, &idx ) == BUFF_NOT_A_CLIENT)
        return BUFF_NOT_A_CLIENT;
        
    // Leo
    ringBuf_read(_this, &_this->clients[idx].ptr, &_this->clients[idx].count, _this->clients[idx].read_size, _this->clients[idx].shift_size, output);
    
    return BUFF_OK;
}

ringBufStatus ringBuf_read_all ( ringBuf *_this, uint8_t client, uint8_t *output, size_t *read_size )
{
    // Busco el cliente
    uint8_t idx = 0;
    if (ringBuf_findClient ( _this,client, &idx ) == BUFF_NOT_A_CLIENT)
        return BUFF_NOT_A_CLIENT;
    
    // Leo
    ringBuf_read(_this, &_this->clients[idx].ptr, &_this->clients[idx].count, _this->clients[idx].count, _this->clients[idx].count, output);
    
    // Devuelvo la cantidad leida
    *read_size = _this->clients[idx].count;
    
    return BUFF_OK;
}
