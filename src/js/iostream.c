/*
 * I/O streams.
 * Copyright (c) 1998 New Generation Software (NGS)
 *
 * Author: Markku Rossi <mtr@ngs.fi>
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 */

/*
 * $Source: /usr/local/cvsroot/ngs/js/src/iostream.c,v $
 * $Id: iostream.c,v 1.3 1998/08/26 08:05:59 mtr Exp $
 */

#include "jsint.h"

#ifdef JS_IOSTREAM

/*
 * Types and definitions.
 */
#ifndef DEFAULT_BUFFER_SIZE
#define DEFAULT_BUFFER_SIZE 4096
#endif /* not DEFAULT_BUFFER_SIZE */

/*
 * Global functions.
 */

JSIOStream *js_iostream_new()
{
    JSIOStream *stream;

    stream = js_calloc(NULL, 1, sizeof(*stream));
    if (stream == NULL)
        return NULL;

    stream->buflen = DEFAULT_BUFFER_SIZE;
    stream->buffer = js_malloc(NULL, stream->buflen);
    if (stream->buffer == NULL) {
        js_free(stream);
        return NULL;
    }

    return stream;
}


size_t js_iostream_read(JSIOStream * stream, void *ptr, size_t size)
{
    size_t total = 0;
    int got;

    if (stream->writep) {
        /* We have buffered output data. */
        if (js_iostream_flush(stream) == EOF)
            return 0;
#ifdef JS_RUNTIME_DEBUG
        assert(stream->writep == 0);
#endif
    }

    while (size > 0) {
        /* First, take everything from the buffer. */
        if (stream->bufpos < stream->data_in_buf) {
            got = stream->data_in_buf - stream->bufpos;

            if (size < got)
                got = size;

            memcpy(ptr, stream->buffer + stream->bufpos, got);

            stream->bufpos += got;
            size -= got;
            //(unsigned char *) ptr += got;
            ptr = (unsigned char*)ptr + got;
            total += got;
        } else {
            if (stream->at_eof)
                /* EOF seen, can't read more. */
                break;

            js_iostream_fill_buffer(stream);
        }
    }

    return total;
}

size_t js_iostream_write(JSIOStream * stream, void *ptr, size_t size)
{
    int space;
    size_t total = 0;

    if (stream->write == NULL) {
        //stream->error = EBADF;
        return 0;
    }

    if (!stream->writep && stream->bufpos < stream->data_in_buf) {
        /*
         * We have some buffered data in the stream => the actual stream
         * position in stream->context is not in sync with stream->bufpos.
         * Seek back.
         */

        if ((*stream->seek) (stream->context, SEEK_CUR, stream->bufpos - stream->data_in_buf) < 0)
            /* XXX Error value. */
            return 0;

        stream->bufpos = 0;
        stream->data_in_buf = 0;
    }

    while (size > 0) {
        space = stream->buflen - stream->data_in_buf;
        if (size < space)
            space = size;

        /* Append data to the buffer. */
        memcpy(stream->buffer + stream->data_in_buf, ptr, space);
        stream->data_in_buf += space;
        total += space;
        size -= space;
        //(unsigned char *) ptr += space;
        ptr = (unsigned char *) ptr + space;

        /* Now the buffer contains buffered write data. */
        stream->writep = 1;

        if (size > 0) {
            /* Still some data left.  Must flush  */
            if (js_iostream_flush(stream) == EOF)
                return total;
        }
    }

    /* Autoflush. */
    if (stream->autoflush && stream->writep)
        if (js_iostream_flush(stream) == EOF)
            /* Failed.  Just return something smaller than <size> */
            return total - stream->data_in_buf;

    return total;
}


int js_iostream_flush(JSIOStream * stream)
{
    if (stream == NULL || stream->write == NULL || !stream->writep)
        return 0;

    stream->writep = 0;
#ifdef JS_RUNTIME_DEBUG
    assert(stream->bufpos == 0);
#endif

    if (stream->data_in_buf > 0) {
        int to_write = stream->data_in_buf;

        stream->data_in_buf = 0;
        if ((*stream->write) (stream->context, stream->buffer, to_write, &stream->error) < to_write) {
            stream->error = errno;
            return EOF;
        }
    }

    return 0;
}


int js_iostream_unget(JSIOStream * stream, int byte)
{
    if (stream->writep) {
        /* We have buffered output data. */
        if (js_iostream_flush(stream) == EOF)
            return EOF;

#ifdef JS_RUNTIME_DEBUG
        assert(stream->writep == 0);
#endif
    }

    if (stream->bufpos > 0) {
        /* It fits. */
        stream->buffer[--stream->bufpos] = byte;
    } else if (stream->data_in_buf < stream->buflen) {
      move:
        memmove(stream->buffer + 1, stream->buffer, stream->data_in_buf);
        stream->data_in_buf++;
        stream->buffer[0] = byte;
    } else {
        /* Allocate a bigger buffer.  */
        unsigned char *new_buffer = js_realloc(NULL, stream->buffer,
                                               stream->buflen + 1);
        if (new_buffer == NULL) {
            stream->error = errno;
            return EOF;
        }

        stream->buflen++;
        stream->buffer = new_buffer;
        goto move;
    }

    /* Upon successful completion, we must return the byte. */
    return byte;
}


int js_iostream_close(JSIOStream * stream)

{
    int result = 0;

    if (stream == NULL)
        return result;

    if (js_iostream_flush(stream) == EOF)
        result = EOF;

    if (stream->close)
        (*stream->close) (stream->context);

    js_free(stream->buffer);
    js_free(stream);

    return result;
}


int js_iostream_seek(JSIOStream * stream, long offset, int whence)
{
    int result;

    if (js_iostream_flush(stream) == EOF)
        return -1;

    result = (*stream->seek) (stream->context, offset, whence);
    if (result == 0)
        /* Successful.  Clear the eof flag. */
        stream->at_eof = 0;

    return result;
}


long js_iostream_get_position(JSIOStream * stream)
{
    long pos;

    /* Flush the possible buffered output. */
    if (js_iostream_flush(stream) == EOF)
        return -1;

    pos = (*stream->get_position) (stream->context);
    if (pos < 0)
        return pos;

    /*
     * The logical position if at <bufpos>, the context's idea is at
     * <data_in_buf>.  Adjust.
     */
    return pos - (stream->data_in_buf - stream->bufpos);
}


long js_iostream_get_length(JSIOStream * stream)
{
    /* Flush the possible buffered output. */
    if (js_iostream_flush(stream) == EOF)
        return -1;

    return (*stream->get_length) (stream->context);
}


void js_iostream_fill_buffer(JSIOStream * stream)
{
    if (stream->read == NULL) {
        stream->at_eof = 1;
        return;
    }

    stream->data_in_buf = (*stream->read) (stream->context, stream->buffer,
                                           stream->buflen, &stream->error);
    stream->bufpos = 0;
    if (stream->data_in_buf == 0)
        stream->at_eof = 1;
}

#endif
