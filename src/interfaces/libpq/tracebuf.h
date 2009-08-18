#ifndef LIBPQ_TRACEBUF_H
#define LIBPQ_TRACEBUF_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#define PQTRACEBUF_SIZE 4096

typedef struct {
	uint8_t data[PQTRACEBUF_SIZE];
	ssize_t start;
	size_t end;
} PQTraceBuf;

inline static void pqtracebuf_init(PQTraceBuf *trace)
{
	trace->start = -1;
	trace->end = 0;
}

inline static void pqtracebuf_append(PQTraceBuf *trace, const uint8_t *data, size_t size)
{
	size_t end_space = PQTRACEBUF_SIZE - trace->end;

	trace->start = trace->end;

	if (size > PQTRACEBUF_SIZE) {
		data += size - PQTRACEBUF_SIZE;
		size = PQTRACEBUF_SIZE;
	}

	if (size <= end_space) {
		memcpy(trace->data + trace->end, data, size);
		trace->end += size;
	} else {
		memcpy(trace->data + trace->end, data, end_space);
		data += end_space;
		size -= end_space;
		memcpy(trace->data, data, size);
		trace->end = size;
	}
}

inline static bool pqtracebuf_empty(const PQTraceBuf *trace)
{
	return trace->start < 0;
}

inline static void pqtracebuf_dump(const PQTraceBuf *trace, const char *suffix)
{
	mode_t mask;
	char path[36];
	int fd;

	if (pqtracebuf_empty(trace))
		return;

	mask = umask(0);
	mkdir("/tmp/libpq-trace", 0777);
	umask(mask);

	snprintf(path, sizeof (path), "/tmp/libpq-trace/%d%s", getpid(), suffix);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0)
		return;

	if ((unsigned int) trace->start < trace->end) {
		write(fd, trace->data + trace->start, trace->end - trace->start);
	} else {
		write(fd, trace->data + trace->start, PQTRACEBUF_SIZE - trace->start);
		write(fd, trace->data, trace->end);
	}

	close(fd);
}

typedef struct {
	PQTraceBuf recv;
	PQTraceBuf send;
} PQTraceIO;

inline static void pqtraceio_init(PQTraceIO *io)
{
	pqtracebuf_init(&io->recv);
	pqtracebuf_init(&io->send);
}

inline static void pqtraceio_set_active(PQTraceIO *io)
{
	extern PQTraceIO *pqtraceio_active;

	pqtraceio_active = io;
}

inline static void pqtraceio_clear_active(const PQTraceIO *io)
{
	extern PQTraceIO *pqtraceio_active;

	if (pqtraceio_active == io)
		pqtraceio_active = NULL;
}

inline static void pqtraceio_dump_active(void)
{
	extern PQTraceIO *pqtraceio_active;

	if (pqtraceio_active) {
		pqtracebuf_dump(&pqtraceio_active->recv, "-recv");
		pqtracebuf_dump(&pqtraceio_active->send, "-send");
	}
}

#endif
