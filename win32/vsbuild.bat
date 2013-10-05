rem
rem build script for win32
rem


rem
rem generate core part of library
rem
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\connect.c -Folib\connect.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\crc32c.c -Folib\crc32c.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\discovery.c -Folib\discovery.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\init.c -Folib\init.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\login.c -Folib\login.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\logging.c -Folib\logging.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\md5.c -Folib\md5.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\nop.c -Folib\nop.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\pdu.c -Folib\pdu.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\iscsi-command.c -Folib\iscsi-command.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\scsi-lowlevel.c -Folib\scsi-lowlevel.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\socket.c -Folib\socket.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\sync.c -Folib\sync.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd lib\task_mgmt.c -Folib\task_mgmt.obj
cl /I. /Iinclude -Zi -Od -c -D_U_="" -DWIN32 -D_WIN32_WINNT=0x0600 -MDd win32\win32_compat.c -Folib\win32_compat.obj




rem
rem create a linklibrary/dll
rem
lib /out:lib\libiscsi.lib /def:lib\libiscsi.def lib\connect.obj lib\crc32c.obj lib\discovery.obj lib\init.obj lib\login.obj lib\logging.obj lib\md5.obj lib\nop.obj lib\pdu.obj lib\iscsi-command.obj lib\scsi-lowlevel.obj lib\socket.obj lib\sync.obj lib\task_mgmt.obj lib\win32_compat.obj

link /DLL /out:lib\libiscsi.dll /DEBUG /DEBUGTYPE:cv lib\libiscsi.exp lib\connect.obj lib\crc32c.obj lib\discovery.obj lib\init.obj lib\login.obj lib\logging.obj lib\md5.obj lib\nop.obj lib\pdu.obj lib\iscsi-command.obj lib\scsi-lowlevel.obj lib\socket.obj lib\sync.obj lib\task_mgmt.obj lib\win32_compat.obj ws2_32.lib kernel32.lib




rem
rem build a test application
rem
cl /I. /Iinclude -Zi -Od -DWIN32 -D_WIN32_WINNT=0x0600 -MDd -D_U_="" examples\iscsiclient.c lib\win32_compat.obj lib\libiscsi.lib WS2_32.lib kernel32.lib mswsock.lib advapi32.lib wsock32.lib advapi32.lib



