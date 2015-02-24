/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

typedef struct PPDGenerator PPDGenerator;

PPDGenerator * newPPDGenerator(const char * printerName);
void deletePPDGenerator(PPDGenerator * ppd);
void ppdSetColor(PPDGenerator * ppd, int color);
void ppdSetDuplex(PPDGenerator * ppd, int duplex);
void ppdAddPaperSize(PPDGenerator * ppd, const char * name, int width, int length);
void ppdSetDefaultPaperSize(PPDGenerator * ppd, const char * name);
void ppdAddResolution(PPDGenerator * ppd, int resolution);
void ppdSetDefaultResolution(PPDGenerator * ppd, int resolution);
void ppdAddMediaType(PPDGenerator * ppd, const char * media);
void ppdSetDefaultMediaType(PPDGenerator * ppd, const char * media);
void ppdAddTray(PPDGenerator * ppd, const char * tray);
void ppdSetDefaultTray(PPDGenerator * ppd, const char * tray);
void ppdSetHWMargins(PPDGenerator * ppd, int top, int down, int left, int right);
char * generatePPD(PPDGenerator * ppd);
