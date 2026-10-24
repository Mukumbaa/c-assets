#ifndef BIT_IO_H
#define BIT_IO_H

#include <stdio.h>
#include <stdint.h>

/* --- BIT WRITER (Per la compressione) --- */
typedef struct {
    FILE *stream;           // File su cui scrivere
    uint64_t accumulator;  // Buffer per raccogliere i bit
    int bits_count;         // Quanti bit sono attualmente nel buffer
} bit_writer;

extern void bit_writer_init(bit_writer *bw, FILE *stream);
extern void bit_writer_write(bit_writer *bw, uint32_t code, int len);
extern void bit_writer_flush(bit_writer *bw);


/* --- BIT READER (Per la decompressione) --- */
typedef struct {
    FILE *stream;           // File da cui leggere
    uint64_t accumulator;  // Buffer dei bit letti
    int bits_count;         // Quanti bit rimangono da consumare
} bit_reader;

extern void bit_reader_init(bit_reader *br, FILE *stream);
extern int  bit_reader_read_bit(bit_reader *br);               // Ritorna 0, 1 oppure -1 su EOF
extern int64_t bit_reader_read_bits(bit_reader *br, int len);  // Legge 'len' bit consecutivi

#endif // BIT_IO_H
