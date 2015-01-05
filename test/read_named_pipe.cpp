#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#define BUFSIZE 512

int main(int argc, char *argv[])
{
    HANDLE hPipe;
    TCHAR  chBuf[BUFSIZE];
    BOOL   fSuccess = FALSE;
    DWORD  cbRead, dwMode;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\flexvdi_creds");

    // Try to open a named pipe; wait for it, if necessary.

    while (1)
    {
        hPipe = CreateFile(
        lpszPipename,   // pipe name
        GENERIC_READ |  // read and write access
        GENERIC_WRITE,
        0,              // no sharing
        NULL,           // default security attributes
        OPEN_EXISTING,  // opens existing pipe
        FILE_ATTRIBUTE_NORMAL,              // default attributes
        NULL);          // no template file

        // Break if the pipe handle is valid.

        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs.

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf( TEXT("Could not open pipe. GLE=%d\n"), GetLastError() );
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds.

        if ( ! WaitNamedPipe(lpszPipename, 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }

    // The pipe connected; change to message-read mode.

    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
    hPipe,    // pipe handle
    &dwMode,  // new pipe mode
    NULL,     // don't set maximum bytes
    NULL);    // don't set maximum time
    if ( ! fSuccess)
    {
        _tprintf( TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError() );
        return -1;
    }

    do
    {
        // Read from the pipe.

        fSuccess = ReadFile(
        hPipe,    // pipe handle
        chBuf,    // buffer to receive reply
        BUFSIZE*sizeof(TCHAR),  // size of buffer
                            &cbRead,  // number of bytes read
                             NULL);    // not overlapped

        if ( ! fSuccess && GetLastError() != ERROR_MORE_DATA )
            break;

        _tprintf( TEXT("\"%s\"\n"), chBuf );
    } while ( ! fSuccess);  // repeat loop if ERROR_MORE_DATA

    if ( ! fSuccess)
    {
        _tprintf( TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError() );
        return -1;
    }

    printf("\n<End of message, press ENTER to terminate connection and exit>");
    _getch();

    CloseHandle(hPipe);

    return 0;
}
