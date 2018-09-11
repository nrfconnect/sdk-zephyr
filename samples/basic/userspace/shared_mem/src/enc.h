/* enc.h */

/*
 *   SPDX-License-Identifier: Apache-2.0
 */


#ifndef ENC_H
#define ENC_H

#include <misc/printk.h>

#define WHEEL_SIZE 26
#define IMOD(a, b) ((a + b) % WHEEL_SIZE)

#ifndef BYTE
#define BYTE unsigned char
#endif

void update_wheel_index(void);
char index_to_char(short i);
short char_to_index(char c);
int calc_rev_wheel(BYTE *wheel, BYTE *backpath);
char enig_enc(char pt);

extern BYTE W1[26];
extern BYTE W2[26];
extern BYTE W3[26];
extern BYTE R[26];
extern BYTE W1R[26];
extern BYTE W2R[26];
extern BYTE W3R[26];
extern int IW1;
extern int IW2;
extern int IW3;

#endif
