/* 
 * File:   newfile.h
 * Author: spanceac
 *
 * Created on April 13, 2023, 2:58 PM
 */

#ifndef NEWFILE_H
#define	NEWFILE_H

#ifdef	__cplusplus
extern "C" {
#endif

/*************************** HEADER FILES ***************************/
#include <stddef.h>
#include <string.h>
/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

/**************************** DATA TYPES ****************************/
typedef unsigned char BYTE;             // 8-bit byte
typedef unsigned long  WORD;             // 32-bit word, change to "long" for 16-bit machines

typedef struct {
	BYTE data[64];
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];
} SHA256_CTX;

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[]);

#ifdef	__cplusplus
}
#endif

#endif	/* NEWFILE_H */

