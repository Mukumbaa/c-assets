#include "libbit.h"

/* ========================================================================= */
/*                              BIT WRITER                                   */
/* ========================================================================= */

extern void bit_writer_init(bit_writer *bw, FILE *stream) {
    bw->stream = stream;
    bw->accumulator = 0;
    bw->bits_count = 0;
}

extern void bit_writer_write(bit_writer *bw, uint32_t code, int len) {
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

extern void bit_writer_flush(bit_writer *bw) {
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

extern void bit_reader_init(bit_reader *br, FILE *stream) {
    br->stream = stream;
    br->accumulator = 0;
    br->bits_count = 0;
}

extern int bit_reader_read_bit(bit_reader *br) {
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

extern int64_t bit_reader_read_bits(bit_reader *br, int len) {
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
