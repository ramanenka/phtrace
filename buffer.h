#ifndef PHTRACE_BUFFER_H
#define PHTRACE_BUFFER_H

typedef struct _phtrace_buffer_t {
    size_t size;
    size_t used;
    unsigned char *data;
} phtrace_buffer_t;

extern phtrace_buffer_t phtrace_buffer;

#define PHTRACE_BUFFER_CURRENT (phtrace_buffer.data + phtrace_buffer.used)

void phtrace_buffer_allocate();
void phtrace_buffer_free();
void phtrace_buffer_flush();
void phtrace_buffer_close();

static inline void phtrace_buffer_ensure_size(size_t size) {
    if (phtrace_buffer.size - size < phtrace_buffer.used) {
        phtrace_buffer_flush();
    }
}

#define PHTRACE_ALLOC_EVENT(VAR, TYPE)                      \
    do {                                            \
        phtrace_buffer_ensure_size(1 + sizeof(TYPE));       \
        phtrace_buffer.data[phtrace_buffer.used] = EventTypes.TYPE; \
        phtrace_buffer.used++;                              \
        VAR = (TYPE *) PHTRACE_BUFFER_CURRENT;              \
        phtrace_buffer.used += sizeof(TYPE);                \
    } while(0)

#endif
