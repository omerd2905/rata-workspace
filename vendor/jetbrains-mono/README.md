# JetBrains Mono NL Regular (v2.304)

- **License**: SIL Open Font License 1.1 (`OFL.txt`)
- **Upstream**: https://github.com/JetBrains/JetBrainsMono/releases/tag/v2.304
- **Source file**: `fonts/ttf/JetBrainsMonoNL-Regular.ttf`
- **SHA-256**: `fb3b2575d7b0657359707993288f12a7360344d39387bb26050e276d61f6bd2a`

Embedded into the application as a read-only C byte array in `src/ui/ui_font_data.c`
using `xxd -i`:

```sh
{
  printf '#include "ui/ui_font_data.h"\n\nstatic const '
  xxd -i -n rata_font_mono_ttf JetBrainsMonoNL-Regular.ttf \
    | sed 's/^unsigned int/static const unsigned int/'
  printf '\nconst uint8_t *ui_font_mono_data(void) { return (const uint8_t *)rata_font_mono_ttf; }\n'
  printf 'size_t ui_font_mono_size(void) { return (size_t)rata_font_mono_ttf_len; }\n'
} > src/ui/ui_font_data.c
```
