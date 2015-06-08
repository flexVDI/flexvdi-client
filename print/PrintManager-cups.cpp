
/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include <cups/cups.h>
#include <boost/locale/utf.hpp>
#include "PrintManager.hpp"
#include "util.hpp"

using namespace flexvm;
using std::string;


void PrintManager::resetPrinters() {
    cups_dest_t * dests;
    int numDests = cupsGetDests(&dests);
    for (int i = 0; i < numDests; ++i) {
        if (cupsGetOption("flexvdi-shared-printer", dests[i].num_options, dests[i].options)) {
            uninstallPrinter(dests[i].name);
        }
    }
    cupsFreeDests(numDests, dests);
}


static http_t * cupsConnection() {
    return httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC,
                        cupsEncryption(), 1, 30000, NULL);
}


static string getPrinterURI(const string & printer) {
    char uri[HTTP_MAX_URI];
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, HTTP_MAX_URI, "ipp", NULL, "localhost",
                     ippPort(), "/printers/%s", printer.c_str());
    Log(L_DEBUG) << "URI: " << uri;
    return string(uri);
}


static ipp_t * newRequest(ipp_op_t op, const string & printer) {
    ipp_t * request = ippNewRequest(op);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                 "printer-uri", NULL, getPrinterURI(printer).c_str());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser());
    return request;
}


static string normalizeName(const string & printer) {
    string result;
    typedef boost::locale::utf::utf_traits<char> utf8;
    for (auto i = printer.begin(); i != printer.end() && result.length() < 127;) {
        auto cp = utf8::decode(i, printer.end());
        if (cp > ' ' && cp != '@' && cp != 127 && cp != '/' && cp != '#')
            result += (char)cp;
        else result += '_';
    }
    return result;
}


bool PrintManager::installPrinter(const string & printer, const string & ppd) {
    Log(L_DEBUG) << "Installing printer " << printer << " from " << ppd;

    int numOptions = 0;
    cups_option_t * options = NULL;
    numOptions = cupsAddOption("device-uri", "flexvdiprint:", numOptions, &options);
    numOptions = cupsAddOption("printer-info", sharedPrinterDescription,
                               numOptions, &options);
    numOptions = cupsAddOption("printer-location", sharedPrinterLocation,
                               numOptions, &options);

    string normPrinter = normalizeName(printer);
    ipp_t * request = newRequest(IPP_OP_CUPS_ADD_MODIFY_PRINTER, normPrinter);
    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", IPP_PSTATE_IDLE);
    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);
    cupsEncodeOptions2(request, numOptions, options, IPP_TAG_OPERATION);
    cupsEncodeOptions2(request, numOptions, options, IPP_TAG_PRINTER);
    cupsFreeOptions(numOptions, options);

    http_t * http = cupsConnection();
    ippDelete(cupsDoFileRequest(http, request, "/admin/", ppd.c_str()));
    if (cupsLastError() > IPP_STATUS_OK_CONFLICTING) {
        Log(L_ERROR) << "Failed to install printer: " << cupsLastErrorString();
        httpClose(http);
        return false;
    }

    cups_dest_t * dest, * dests;
    int numDests = cupsGetDests(&dests);
    dest = cupsGetDest(normPrinter.c_str(), NULL, numDests, dests);
    if (!dest) {
        Log(L_ERROR) << "Failed to install printer";
        return false;
    }
    dest->num_options = cupsAddOption("flexvdi-shared-printer", printer.c_str(),
                                      dest->num_options, &dest->options);
    cupsSetDests(numDests, dests);
    cupsFreeDests(numDests, dests);
    httpClose(http);
    return true;
}


bool PrintManager::uninstallPrinter(const string & printer) {
    bool result = true;
    ipp_t * request = newRequest(IPP_OP_CUPS_DELETE_PRINTER, normalizeName(printer));
    http_t * http = cupsConnection();
    ippDelete(cupsDoRequest(http, request, "/admin/"));
    if (cupsLastError() > IPP_STATUS_OK_CONFLICTING) {
        Log(L_ERROR) << "Failed to uninstall printer: " << cupsLastErrorString();
        result = false;
    }
    httpClose(http);
    return result;
}
