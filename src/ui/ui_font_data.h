#ifndef RATA_UI_FONT_DATA_H
#define RATA_UI_FONT_DATA_H

#include <stddef.h>
#include <stdint.h>

/* Embedded JetBrains Mono NL Regular v2.304 (SIL OFL 1.1; see vendor/jetbrains-mono/OFL.txt).
 *
 * Regenerate src/ui/ui_font_data.c from JetBrainsMonoNL-Regular.ttf using:
 *   {
 *     printf '#include "ui/ui_font_data.h"\n\nstatic const '
 *     xxd -i -n rata_font_mono_ttf JetBrainsMonoNL-Regular.ttf | sed 's/^unsigned int/static const unsigned int/'
 *     printf '\nconst uint8_t *ui_font_mono_data(void) { return (const uint8_t *)rata_font_mono_ttf; }\n'
 *     printf 'size_t ui_font_mono_size(void) { return (size_t)rata_font_mono_ttf_len; }\n'
 *   } > src/ui/ui_font_data.c
 */

/* Returns pointer to static read-only TTF byte buffer (caller does not free). */
const uint8_t *ui_font_mono_data(void);

/* Returns byte length of embedded TTF buffer. */
size_t ui_font_mono_size(void);

#endif /* RATA_UI_FONT_DATA_H */
