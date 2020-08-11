/*
 * Copyright (C) 2009 Red Hat Inc, Steven Rostedt <srostedt@redhat.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License (not later!)
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not,  see <http://www.gnu.org/licenses>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include "event-parse.h"

#include "../gpuvis_macros.h"

/**
 * trace_seq_init - initialize the trace_seq structure
 * @s: a pointer to the trace_seq structure to initialize
 */
void trace_seq_init(struct trace_seq *s)
{
	s->len = 0;
	s->buffer_size = TRACE_SEQ_BUF_SIZE;
	s->buffer = s->buf;
}

/**
 * trace_seq_reset - re-initialize the trace_seq structure
 * @s: a pointer to the trace_seq structure to reset
 */
void trace_seq_reset(struct trace_seq *s)
{
	if (s)
		s->len = 0;
}

/**
 * trace_seq_destroy - free up memory of a trace_seq
 * @s: a pointer to the trace_seq to free the buffer
 *
 * Only frees the buffer, not the trace_seq struct itself.
 */
void trace_seq_destroy(struct trace_seq *s)
{
	if (s->buffer != s->buf)
		free(s->buffer);
}

static void expand_buffer(struct trace_seq *s)
{
	char *buf;

	if (s->buffer == s->buf) {
		buf = malloc(s->buffer_size + TRACE_SEQ_BUF_SIZE);
		memcpy(buf, s->buf, s->len);
	} else {
		buf = realloc(s->buffer, s->buffer_size + TRACE_SEQ_BUF_SIZE);
	}

	s->buffer = buf;
	s->buffer_size += TRACE_SEQ_BUF_SIZE;
}

// Fast format_decimal code taken from fmt library (and modified for C):
//   https://github.com/fmtlib/fmt/blob/master/include/fmt/format.h#L1079
//   http://fmtlib.net/latest/index.html
//   http://www.zverovich.net/2013/09/07/integer-to-string-conversion-in-cplusplus.html
#define BUFFER_SIZE 64

// Formats value in reverse and returns a pointer to the beginning.
static char *format_decimal(char buf[BUFFER_SIZE], unsigned long long value)
{
    char *ptr = buf + BUFFER_SIZE;
    static const char DIGITS[] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";

    *--ptr = 0;

    while (value >= 100)
    {
        // Integer division is slow so do it for a group of two digits instead
        // of for every digit. The idea comes from the talk by Alexandrescu
        // "Three Optimization Tips for C++". See speed-test for a comparison.
        unsigned index = (unsigned int)((value % 100) * 2);

        value /= 100;
        *--ptr = DIGITS[index + 1];
        *--ptr = DIGITS[index];
    }

    if (value < 10)
    {
        *--ptr = (char)('0' + value);
        return ptr;
    }

    unsigned index = (unsigned int)(value * 2);
    *--ptr = DIGITS[index + 1];
    *--ptr = DIGITS[index];
    return ptr;
}

static char *format_signed(char buf[BUFFER_SIZE], long long value)
{
    char *str;
    bool negative = (value < 0);
    unsigned long long abs_value = (unsigned long long)(value);

    if (negative)
        abs_value = 0 - abs_value;

    str = format_decimal(buf, abs_value);

    if (negative)
        *--str = '-';

#if 0
    char buf2[64];
    sprintf(buf2, "%lld", value);
    if (strcmp(str, buf2))
        printf("%s ERROR: %s != %s\n", __func__, buf, buf2);
#endif
    return str;
}

int trace_seq_put_sval(struct trace_seq *s, long long val)
{
    const char *str;
    char buf[BUFFER_SIZE];

    str = format_signed(buf, val);
    trace_seq_puts(s, str);
    return 1;
}

int trace_seq_put_uval(struct trace_seq *s, unsigned long long val)
{
    const char *str;
    char buf[BUFFER_SIZE];

    str = format_decimal(buf, val);
    trace_seq_puts(s, str);
    return 1;
}

/**
 * trace_seq_printf - sequence printing of trace information
 * @s: trace sequence descriptor
 * @fmt: printf format string
 *
 * It returns 0 if the trace oversizes the buffer's free
 * space, 1 otherwise.
 *
 * The tracer may use either sequence operations or its own
 * copy to user routines. To simplify formating of a trace
 * trace_seq_printf is used to store strings into a special
 * buffer (@s). Then the output may be either used by
 * the sequencer or pulled into another buffer.
 */
int
trace_seq_printf(struct trace_seq *s, const char *fmt, ...)
{
    // static int s_ctx = 1;
    // gpuvis_trace_begin_ctx_printf( ++s_ctx, "%s", __func__ );

    GPUVIS_COUNT_HOT_FUNC_CALLS();

	va_list ap;
	int len;
	int ret;

    if (fmt[ 0 ] == '%' && fmt[ 1 ] == 's' && fmt[ 2 ] == '\0')
    {
        const char *str;

        va_start(ap, fmt);
        str = va_arg(ap, const char *);
        va_end(ap);

        trace_seq_puts(s, str);

        // gpuvis_trace_end_ctx_printf( s_ctx, "%s", __func__ );
        return 1;
    }

 try_again:
	len = (s->buffer_size - 1) - s->len;

	va_start(ap, fmt);
	ret = vsnprintf(s->buffer + s->len, len, fmt, ap);
	va_end(ap);

	if (ret >= len) {
		expand_buffer(s);
		goto try_again;
	}

	s->len += ret;

    // gpuvis_trace_end_ctx_printf( s_ctx, "%s", __func__ );
	return 1;
}

/**
 * trace_seq_vprintf - sequence printing of trace information
 * @s: trace sequence descriptor
 * @fmt: printf format string
 *
 * The tracer may use either sequence operations or its own
 * copy to user routines. To simplify formating of a trace
 * trace_seq_printf is used to store strings into a special
 * buffer (@s). Then the output may be either used by
 * the sequencer or pulled into another buffer.
 */
int
trace_seq_vprintf(struct trace_seq *s, const char *fmt, va_list args)
{
	int len;
	int ret;

 try_again:
	len = (s->buffer_size - 1) - s->len;

	ret = vsnprintf(s->buffer + s->len, len, fmt, args);

	if (ret >= len) {
		expand_buffer(s);
		goto try_again;
	}

	s->len += ret;

	return len;
}

/**
 * trace_seq_puts - trace sequence printing of simple string
 * @s: trace sequence descriptor
 * @str: simple string to record
 *
 * The tracer may use either the sequence operations or its own
 * copy to user routines. This function records a simple string
 * into a special buffer (@s) for later retrieval by a sequencer
 * or other mechanism.
 */
int trace_seq_puts(struct trace_seq *s, const char *str)
{
	size_t len;

	len = strlen(str);

	while (len > ((s->buffer_size - 1) - s->len))
		expand_buffer(s);

	memcpy(s->buffer + s->len, str, len);
	s->len += ( unsigned long )len;

	return ( int )len;
}

int trace_seq_putc(struct trace_seq *s, unsigned char c)
{
	while (s->len >= (s->buffer_size - 1))
		expand_buffer(s);

	s->buffer[s->len++] = c;

	return 1;
}

void trace_seq_terminate(struct trace_seq *s)
{
	/* There's always one character left on the buffer */
	s->buffer[s->len] = 0;
}

int trace_seq_do_fprintf(struct trace_seq *s, FILE *fp)
{
    return fprintf(fp, "%.*s", s->len, s->buffer);
}

int trace_seq_do_printf(struct trace_seq *s)
{
	return trace_seq_do_fprintf(s, stdout);
}
