
void drawPNG(const char *filename, int x, int y) {

    const uint16_t *image = (const uint16_t *)filename;
    tft.writeRect(x, y, 64, 64, image);
 
}

void drawKeyboard(const char *filename, int x, int y) {

    const uint16_t *image = (const uint16_t *)filename;
    tft.writeRect(x, y, 40, 14, image);
 
}