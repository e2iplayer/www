#ifndef _exteplayer3_external_plugins_png_
#define _exteplayer3_external_plugins_png_

int PNGPlugin_saveRGBAImage(const char *filename, const unsigned char *data, int width, int height);
int PNGPlugin_init(void);
int PNGPlugin_term(void);

#endif // _exteplayer3_external_plugins_png_
