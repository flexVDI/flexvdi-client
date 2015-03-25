Abstract
========

This sample illustrates a pass-thru "stub" GINA which can be used in some
cases to simplify GINA development.

A common use for a GINA is to implement code which requires the credentials
of the user logging onto the workstation.  The credentials may be required
for syncronization with foreign account databases or custom authentication
activities.

In this example case, it is possible to implement a simple GINA stub layer
which simply passes control for the required functions to the previously
installed GINA, and captures the interesting parameters from that GINA.
In this scenario, the existing functionality in the existent GINA is
retained.  In addition, the development time is reduced drastically, as
existing functionality does not need to be duplicated.

When dealing with credentials, take steps to maintain the security of the
credentials.  For instance, if transporting credentials over a network, be
sure to encrypt the credentials.

Files included in this sample
=============================

1) ReadMe.txt   - This file
2) GinaStub.c   - C source file
3) GinaStub.h   - C header file
4) GinaStub.def - Definition file
5) MAKEFILE     - Makefile for NMAKE utility

Winlogon Versions supported
===========================

This sample can support WINLOGON versions from 1.0 up to 1.3.

Version 1.0 = NT 3.5x
Version 1.1 = NT 4.0, SP1, SP2, and SP3
Version 1.2 = NT 4.0 SP4 and SP5
Version 1.3 = Windows 2000

How to compile
==============

1) Make sure MSSDK and your C compiler environment are properly setup.
2) From the GinaStub directory, run NMAKE to make GinaStub.dll.

How to install GINASTUB
=======================

1) Copy GinaStub.dll to %SystemRoot%\System32 directory.
2) Run RegEdit (or RegEdt32).
3) Create the following value under
   HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\Winlogon.
   Value Name: GinaDLL
   Value Type: REG_SZ
   Value Data: "GinaStub.dll"
4) Exit RegEdit.
5) Reboot.

Remarks
=======

Make sure to create an Emergency Repair Disk before deploying GINASTUB.DLL,
since any mishap may cause your system unuseable. It is recommended that you
test your GINA under a FAT system. This way you can always boot to DOS, copy
over MSGINA.DLL to GINASTUB.DLL, and reboot to restore your system.
