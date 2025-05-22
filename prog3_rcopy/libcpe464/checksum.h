/* Checksum declaration 
 * shadows@whitefang.com
 */

#include <stdbool.h>
#include <stdint.h>

unsigned short in_cksum(unsigned short *addr,int len);
bool verifyChecksum(const uint8_t *buffer, int length);



