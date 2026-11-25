#ifndef RATA_UI_FONT_LORA_H
#define RATA_UI_FONT_LORA_H

#include <stddef.h>
#include <stdint.h>

/* Embedded Lora Regular (Cyreal, SIL OFL 1.1; see vendor/lora/OFL.txt).
 *
 * Regenerate src/ui/ui_font_lora.c from Lora-Regular.ttf using:
 *   {
 *     printf '#include "ui/ui_font_lora.h"\n\nstatic const '
 *     xxd -i -n rata_font_lora_ttf Lora-Regular.ttf | sed 's/^unsigned int/static const unsigned int/'
 *     printf '\nconst uint8_t *ui_font_lora_data(void) { return (const uint8_t *)rata_font_lora_ttf; }\n'
 *     printf 'size_t ui_font_lora_size(void) { return (size_t)rata_font_lora_ttf_len; }\n'
 *   } > src/ui/ui_font_lora.c
 */

/* Returns pointer to static read-only TTF byte buffer (caller does not free). */
const uint8_t *ui_font_lora_data(void);

/* Returns byte length of embedded TTF buffer. */
size_t ui_font_lora_size(void);

#endif /* RATA_UI_FONT_LORA_H */
