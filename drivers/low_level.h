#ifndef LOW_LEVEL_H
#define LOW_LEVEL_H

// Port I/O wrappers around the x86 in/out instructions (see low_level.c).
// Ports are 16-bit, so they must be `unsigned short` — a byte can't hold a
// port like REG_SCREEN_CTRL (0x3D4).
unsigned char port_byte_in(unsigned short port);
void port_byte_out(unsigned short port, unsigned char data);
unsigned short port_word_in(unsigned short port);
void port_word_out(unsigned short port, unsigned short data);

#endif
