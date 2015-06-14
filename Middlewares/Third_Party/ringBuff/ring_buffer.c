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
//#include <assert.h>
#include <stm32f4xx_hal.h>

#include <ale_stdlib.h>

#undef assert
#define assert(expr) assert_param(expr)


osMutexDef ( rw_mutex );

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

    // Creo el mutex
    _this->rw_mutex_id = osMutexCreate ( osMutex ( rw_mutex ) );
	
    return BUFF_OK;
}

ringBufStatus ringBuf_deinit ( ringBuf *_this )
{
    free ( _this->buff );
    _this->buff  = NULL;

    free ( _this->clients );
    _this->clients = NULL;

    _this->buff_size = 0;
    _this->head = NULL;
    _this->count = 0;
    _this->client_num_assign = 0;

    osMutexDelete ( _this->rw_mutex_id );
	
    return BUFF_OK;
}

ringBufStatus ringBuf_flush ( ringBuf *_this )
{
    _this->count = 0;
    _this->head	 = _this->buff;

    for ( int i=0; i<_this->num_clients; i++ ) {
        _this->clients[i].count = 0;
        _this->clients[i].ptr = _this->buff;
    }

    return BUFF_OK;
}

ringBufStatus ringBuf_registClient ( ringBuf* _this, size_t read_size, size_t shift_size, osMessageQId msg_id, uint32_t msg_val, uint8_t* client_num )
{
    // Me fijo que lo que quiera leer no sea mayor al tamaño del buffer
    assert ( _this->buff_size >= read_size );
    assert ( _this->buff_size >= shift_size );

    _this->num_clients++;
    *client_num = ++_this->client_num_assign;

    // Alloco nueva memoria para el clientes
    ringBufClient *aux = malloc ( _this->num_clients * sizeof ( ringBufClient ) );
	
		// Copio todos loss clientes viejos 
    memcpy ( aux, _this->clients, ( _this->num_clients-1 ) * sizeof ( ringBufClient ) );
	
    // Eliminio la memoria anterior
    if ( _this->num_clients > 1 ) {
        free ( _this->clients );
    }
		
		// Guardo el puntero a la nueva memoria
    _this->clients = aux;

//     if ( _this->num_clients > 1 ) {
//         _this->clients = realloc ( _this->clients, _this->num_clients * sizeof ( ringBufClient ) );
//     } else {
//         _this->clients = malloc ( sizeof ( ringBufClient ) );
//     }

    // Inicializo las variables del cliente
    _this->clients[_this->num_clients-1].client_num = *client_num;
    _this->clients[_this->num_clients-1].count = 0;
    _this->clients[_this->num_clients-1].ptr = _this->head;
    _this->clients[_this->num_clients-1].overrun = false;
    _this->clients[_this->num_clients-1].read_size = read_size;
    _this->clients[_this->num_clients-1].shift_size = shift_size;
    _this->clients[_this->num_clients-1].msg_id = msg_id;
    _this->clients[_this->num_clients-1].msg_val = msg_val;

    return BUFF_OK;
}

ringBufStatus ringBuf_unregistClient ( ringBuf *_this, const uint8_t client_num )
{
	   uint8_t client_idx;

    if ( _this->num_clients > 1 ) {

        // Busco el índice de este cliente
        ringBuf_findClient ( _this, client_num, &client_idx );

        // Creo un nuevo espacio de memoria con un cliente menos
        ringBufClient *aux = malloc ( (_this->num_clients-1) * sizeof ( ringBufClient ) );

        // Copio la info de todos los clientes menos del que estoy des-registrando
        //Copio los que estan antes
        memcpy ( aux, _this->clients, client_idx * sizeof ( ringBufClient ) );
        //Copio los que estan despues
        memcpy ( &aux[client_idx], &_this->clients[client_idx+1], ( _this->num_clients - ( client_idx+1 ) ) * sizeof ( ringBufClient ) );

        // libero la memoria vieja
        free ( _this->clients );
        _this->clients = aux;

    } else {
        free ( _this->clients );
        _this->clients = NULL;
        ringBuf_flush ( _this );
    }

    // Resto la cantidad de clientes
    _this->num_clients--;

    return BUFF_OK;
}

ringBufStatus ringBuf_findClient ( ringBuf *_this, const uint8_t client, uint8_t *idx )
{
    // Busco el cliente
    uint8_t i = 0;
    bool client_found = false;
    for ( ; i < _this->num_clients; i++ ) {
        if ( _this->clients[i].client_num == client ) {
            client_found = true;
            break;
        }
    }
    if ( !client_found ) {
        return BUFF_NOT_A_CLIENT;
    }

    *idx = i;

    return BUFF_OK;
}

bool is_ringBuf_empty ( ringBuf *_this )
{
    ringBuf_updateCount ( _this );
    return ( 0 == _this->count );
}

bool is_ringBuf_full ( ringBuf *_this )
{
    ringBuf_updateCount ( _this );
    return ( _this->count >= _this->buff_size );
}

ringBufStatus ringBuf_write ( ringBuf *_this, const uint8_t *input, const size_t write_size )
{
    // Si la cantidad de datos a copiar es más grande que el tamaño del buffer
    // genero un assert porque se estarían sobreescribiendo datos antes de poder
    // leerlos
    assert ( _this->buff_size >= write_size );

    // Pregunto si puedo sobreescribir
    if ( !_this->can_override ) {
        // Pregunto si el buffer esta lleno
        if ( is_ringBuf_full ( _this ) ) {
            return BUFF_FULL;
        }
        // Pregunto si hay espacio disponible
        if ( write_size > _this->buff_size - _this->count && !_this->can_override ) {
            return BUFF_NOT_ENOGH_SPACE;
        }
    }

    // Pregunto si existen clientes
    if ( _this->num_clients == 0 ) {
        return BUFF_NO_CLIENTS;
    }


    // Obtengo el espacio que queda hacia la derecha del buffer
    uint32_t right_space = _this->buff_size - ( _this->head - _this->buff );

    //Bloqueo el mutex aca
    osMutexWait ( _this->rw_mutex_id, osWaitForever );

    // Pregunto si lo que hay para escribir es menor que lo que queda hacia la derecha
    if ( write_size < right_space ) {
        // Copio el dato
        memcpy ( _this->head, input, write_size );

        // Actualizo el puntero
        _this->head += write_size;
    } else {
        // Copio una parte y después el resto al principio del buffer
        memcpy ( _this->head, input, right_space );
        memcpy ( _this->buff, input, write_size - right_space );

        // Actualizo el puntero
        _this->head = &_this->buff[ write_size - right_space ];
    }

    // Libero el mutex aca
    osMutexRelease ( _this->rw_mutex_id );

    // Update info de clientes
    for ( uint8_t idx=0; idx < _this->num_clients ; idx++ ) {
        // Incremento el contador
        _this->clients[idx].count += write_size;

        // Chequeo overrun
        if ( _this->clients[idx].count > _this->buff_size ) {
            _this->clients[idx].overrun = true;
        }

        // Envío un mensaje al cliente avisando que puede leer
        if ( _this->clients[idx].msg_id != NULL ) {
            osMessagePut ( _this->clients[idx].msg_id, _this->clients[idx].msg_val, 0 );
        }
    }

    return BUFF_OK;
}

ringBufStatus ringBuf_read ( ringBuf *_this, uint8_t **ptr, uint32_t *count, size_t read_size, size_t shift_size, uint8_t *output )
{
    // Me fijo que lo que quiera leer no sea mayor al tamaño del buffer
    assert ( _this->buff_size >= read_size );

    // Me fijo que se puedan leer la cantidad de datos que se piden
    if ( *count < read_size ) {
        return BUFF_NOT_READY;
    }

    // Obtengo el espacio que queda hacia la derecha del buffer
    uint32_t right_space = _this->buff_size - ( *ptr - _this->buff );

    //Bloqueo el mutex aca
    osMutexWait ( _this->rw_mutex_id, osWaitForever );

    // Pregunto si lo que se quiere leer es menor que lo que hay a la derecha
    if ( read_size < right_space ) {
        // Copio el dato
        memcpy ( output, *ptr, read_size );
    } else {
        // Copio una parte y después leo desde el principo del buffer
        memcpy ( output, *ptr, right_space );
        memcpy ( &output[right_space], _this->buff, read_size - right_space );
    }

    // Libero el mutex aca
    osMutexRelease ( _this->rw_mutex_id );

    // Actualizo el puntero (no con la cantidad de datos que leyó, sino con lo que quiera shiftear)
    if ( shift_size < right_space ) {
        *ptr += shift_size;
    } else {
        *ptr = &_this->buff[ shift_size - right_space ];
    }

    // Decremento el contador
    *count -= shift_size;

    return BUFF_OK;
}

ringBufStatus ringBuf_read_var ( ringBuf *_this, uint8_t client, size_t read_size, size_t shift_size, uint8_t *output )
{
    ringBufStatus status;

    // Busco el cliente
    uint8_t idx = 0;
    status = ringBuf_findClient ( _this,client, &idx );

    // Si no hubo problemas leo el buffer
    if ( status == BUFF_OK ) {
        status = ringBuf_read ( _this, &_this->clients[idx].ptr, &_this->clients[idx].count, read_size, shift_size, output );
    }

    return status;
}

ringBufStatus ringBuf_read_const ( ringBuf *_this, uint8_t client, uint8_t *output )
{
    ringBufStatus status;

    // Busco el cliente
    uint8_t idx = 0;
    status = ringBuf_findClient ( _this,client, &idx );

    // Si no hubo problemas leo el buffer
    if ( status == BUFF_OK ) {
        status = ringBuf_read ( _this, &_this->clients[idx].ptr, &_this->clients[idx].count, _this->clients[idx].read_size, _this->clients[idx].shift_size, output );
    }

    return status;
}

ringBufStatus ringBuf_read_all ( ringBuf *_this, uint8_t client, uint8_t *output, size_t *read_size )
{
    ringBufStatus status;

    // Busco el cliente
    uint8_t idx = 0;
    status = ringBuf_findClient ( _this,client, &idx );

    // Si no hubo problemas leo el buffer
    if ( status == BUFF_OK ) {
        status = ringBuf_read ( _this, &_this->clients[idx].ptr, &_this->clients[idx].count, _this->clients[idx].count, _this->clients[idx].count, output );

        // Devuelvo la cantidad leida
        *read_size = _this->clients[idx].count;
    }

    return status;
}

void ringBuf_updateCount ( ringBuf *_this )
{
    // Actualizo el contador
    uint32_t max_count = 0;
    for ( uint8_t idx=0; idx < _this->num_clients ; idx++ )
        if ( _this->clients[idx].count > max_count ) {
            max_count = _this->clients[idx].count;
        }
    _this->count = max_count;
}
