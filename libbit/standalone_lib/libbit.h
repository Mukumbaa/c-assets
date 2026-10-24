/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define LIBBIT_IMPLEMENTATION before including this file:
 * #define LIBBIT_IMPLEMENTATION
 * #include "libbit.h"
 */

#ifndef LIBBIT_DEF
#ifdef __cplusplus
#define LIBBIT_DEF extern "C"
#else
#define LIBBIT_DEF extern
#endif
#endif

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

LIBBIT_DEF void bit_writer_init(bit_writer *bw, FILE *stream);
LIBBIT_DEF void bit_writer_write(bit_writer *bw, uint32_t code, int len);
LIBBIT_DEF void bit_writer_flush(bit_writer *bw);


/* --- BIT READER (Per la decompressione) --- */
typedef struct {
    FILE *stream;           // File da cui leggere
    uint64_t accumulator;  // Buffer dei bit letti
    int bits_count;         // Quanti bit rimangono da consumare
} bit_reader;

LIBBIT_DEF void bit_reader_init(bit_reader *br, FILE *stream);
LIBBIT_DEF int  bit_reader_read_bit(bit_reader *br);               // Ritorna 0, 1 oppure -1 su EOF
LIBBIT_DEF int64_t bit_reader_read_bits(bit_reader *br, int len);  // Legge 'len' bit consecutivi

#endif // BIT_IO_H

#ifdef LIBBIT_IMPLEMENTATION

/* ========================================================================= */
/*                              BIT WRITER                                   */
/* ========================================================================= */

LIBBIT_DEF void bit_writer_init(bit_writer *bw, FILE *stream) {
    bw->stream = stream;
    bw->accumulator = 0;
    bw->bits_count = 0;
}

LIBBIT_DEF void bit_writer_write(bit_writer *bw, uint32_t code, int len) {
    if (len <= 0) return;

    // Maschera per essere certi di non avere bit sporchi oltre 'len'
    uint64_t mask = (len >= 32) ? 0xFFFFFFFFULL : ((1ULL << len) - 1);
    
    // Inserisci i nuovi bit nell'accumulatore
    bw->accumulator = (bw->accumulator << len) | ((uint64_t)code & mask);
    bw->bits_count += len;

    // Scarica su file tutti i byte interi (8 bit) completati
    while (bw->bits_count >= 8) {
        uint8_t byte = (uint8_t)((bw->accumulator >> (bw->bits_count - 8)) & 0xFF);
        fputc(byte, bw->stream);
        bw->bits_count -= 8;
    }
}

LIBBIT_DEF void bit_writer_flush(bit_writer *bw) {
    // Se sono rimasti dei bit orfani nel secchio (< 8), li allinea a sinistra e scrive l'ultimo byte
    if (bw->bits_count > 0) {
        uint8_t byte = (uint8_t)((bw->accumulator << (8 - bw->bits_count)) & 0xFF);
        fputc(byte, bw->stream);
        bw->accumulator = 0;
        bw->bits_count = 0;
    }
    fflush(bw->stream);
}

/* ========================================================================= */
/*                              BIT READER                                   */
/* ========================================================================= */

LIBBIT_DEF void bit_reader_init(bit_reader *br, FILE *stream) {
    br->stream = stream;
    br->accumulator = 0;
    br->bits_count = 0;
}

LIBBIT_DEF int bit_reader_read_bit(bit_reader *br) {
    // Se non abbiamo bit disponibili, leggiamo un byte dal file
    if (br->bits_count == 0) {
        int byte = fgetc(br->stream);
        if (byte == EOF) {
            return -1; // Fine del file
        }
        br->accumulator = (uint8_t)byte;
        br->bits_count = 8;
    }

    // Estrae il bit più significativo (MSB)
    br->bits_count--;
    return (int)((br->accumulator >> br->bits_count) & 1);
}

LIBBIT_DEF int64_t bit_reader_read_bits(bit_reader *br, int len) {
    if (len <= 0) return 0;
    if (len > 32) return -1; // Supporta fino a 32 bit

    uint32_t val = 0;
    for (int i = 0; i < len; i++) {
        int bit = bit_reader_read_bit(br);
        if (bit < 0) return -1; // EOF imprevisto
        val = (val << 1) | bit;
    }
    return (int64_t)val;
}

#endif // LIBBIT_IMPLEMENTATION
