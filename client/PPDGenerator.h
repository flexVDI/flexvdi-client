/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

typedef struct PPDGenerator PPDGenerator;

PPDGenerator * newPPDGenerator(const char * printerName);
void deletePPDGenerator(PPDGenerator * ppd);
void ppdSetColor(PPDGenerator * ppd, int color);
void ppdSetDuplex(PPDGenerator * ppd, int duplex);
void ppdAddPaperSize(PPDGenerator * ppd, char * name, int width, int length);
void ppdSetDefaultPaperSize(PPDGenerator * ppd, char * name);
void ppdAddResolution(PPDGenerator * ppd, int resolution);
void ppdSetDefaultResolution(PPDGenerator * ppd, int resolution);
void ppdAddMediaType(PPDGenerator * ppd, char * media);
void ppdSetDefaultMediaType(PPDGenerator * ppd, char * media);
void ppdAddTray(PPDGenerator * ppd, char * tray);
void ppdSetDefaultTray(PPDGenerator * ppd, char * tray);
void ppdSetHWMargins(PPDGenerator * ppd, int left, int down, int right, int top);
char * generatePPD(PPDGenerator * ppd);
