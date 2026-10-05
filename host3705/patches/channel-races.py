#!/usr/bin/env python3
"""channel-races.py <file>... - socket races between Hercules' comm3705.c and the 3705 emulator.

comm3705.c: BASIC SENSE read up to 256 bytes, so when the one sense byte and the status byte arrive
together the status is swallowed and the next recv waits forever. Read exactly the one sense byte.
READ has the same problem; read-length.py fixes it with a change on both sides.

i3705_chan_T2.c: the 8-byte CCW was read with a read() as big as the buffer, which also takes the write
data when it arrives in the same segment; read exactly 8 bytes. Before reading write data it spins
until FIONREAD reports exactly the CCW count; on Windows FIONREAD may report less than what is queued
and the loop never ends, so read the count in a loop. Accepted sockets are made blocking, as on Linux
(on Windows they inherit O_NONBLOCK and the device number read on connect could come back empty).

i3705_lib.c: the accepted line sockets are made blocking too. The scanner took the end of every read
as the end of a frame, so two SDLC frames that TCP joined became one and a split frame was cut short;
each read now hands it exactly one frame, up to its closing 47 0F 7E.

Not a race but in the same file: the channel adapter loop polled without ever sleeping and kept a whole
core busy; a pass that finds no attention, PCI or channel command now sleeps 100 us."""
import sys

FIXES = {
	'comm3705.c': [(
		'            rc = recv(dev->commadpt->busfd, dev->sense, 256, 0);\n',
		'            rc = recv(dev->commadpt->busfd, dev->sense, 1, 0);   /* the 3705 sends one sense byte */\n')],
	'i3705_lib.c': [(
		'#include <sys/ioctl.h>\n',
		'#include <sys/ioctl.h>\n#include <fcntl.h>\n'), (
		'            pthread_mutex_lock(&line_lock);\n'
		'            LIBline[k]->LIBrlen = read(LIBline[k]->d327x_fd, LIBline[k]->LIB_rbuf, BUFLEN_327x); // If data available, read it\n'
		'            pthread_mutex_unlock(&line_lock);\n',
		'            // The scanner takes the end of each read as the end of a frame: hand it one SDLC frame\n'
		'            // (7E ... 47 0F 7E) per read, whatever TCP joined or split. Other data is read as before.\n'
		'            int got = recv(LIBline[k]->d327x_fd, LIBline[k]->LIB_rbuf, BUFLEN_327x, MSG_PEEK);\n'
		'            int want = got;\n'
		'            if (got > 0 && LIBline[k]->LIB_rbuf[0] == 0x7E) {\n'
		'               want = 0;\n'
		'               for (int i = 3; i < got; i++)\n'
		'                  if (LIBline[k]->LIB_rbuf[i - 2] == 0x47 && LIBline[k]->LIB_rbuf[i - 1] == 0x0F && LIBline[k]->LIB_rbuf[i] == 0x7E) {\n'
		'                     want = i + 1;\n'
		'                     break;\n'
		'                  }\n'
		'               if (want == 0 && got == BUFLEN_327x)\n'
		'                  want = got;                  // no end in a full buffer: as before\n'
		'            }\n'
		'            if (want > 0) {\n'
		'               pthread_mutex_lock(&line_lock);\n'
		'               LIBline[k]->LIBrlen = read(LIBline[k]->d327x_fd, LIBline[k]->LIB_rbuf, want); // If data available, read it\n'
		'               pthread_mutex_unlock(&line_lock);\n'
		'            } else\n'
		'               LIBline[k]->LIBrlen = 0;        // frame not complete yet\n'), (
		'            LIBline[k]->d327x_fd = accept(LIBline[k]->line_fd, NULL, 0);\n',
		'            LIBline[k]->d327x_fd = accept(LIBline[k]->line_fd, NULL, 0);\n'
		'            if (LIBline[k]->d327x_fd >= 0)          // blocking, as accept() gives on Linux\n'
		'               fcntl(LIBline[k]->d327x_fd, F_SETFL, fcntl(LIBline[k]->d327x_fd, F_GETFL, 0) & ~O_NONBLOCK);\n')],
	'i3705_chan_T2.c': [(
		'#include <sys/ioctl.h>\n',
		'#include <sys/ioctl.h>\n#include <fcntl.h>\n'), (
		'   while(1) {\n'
		'      // We do this for ever and ever...\n',
		'   while(1) {\n'
		'      // We do this for ever and ever...\n'
		'      int busy = 0;                          // a pass that finds nothing to do sleeps 100 us\n'), (
		'               exec_attn();\n'
		'            }\n',
		'               exec_attn();\n'
		'               busy = 1;\n'
		'            }\n'), (
		'               exec_pci();\n'
		'            }\n',
		'               exec_pci();\n'
		'               busy = 1;\n'
		'            }\n'), (
		'                  exec_ccw(iobs[j]);\n'
		'               } // End if pendingrcv\n'
		'            }  // End if iobs[j]\n'
		'         }  // End if (iobs[j]->abswitch != -1)\n'
		'      }  // End for int j\n',
		'                  exec_ccw(iobs[j]);\n'
		'                  busy = 1;\n'
		'               } // End if pendingrcv\n'
		'            }  // End if iobs[j]\n'
		'         }  // End if (iobs[j]->abswitch != -1)\n'
		'      }  // End for int j\n'
		'      if (!busy)\n'
		'         usleep(100);\n'), (
		'   iob->bus_socket[abport] = accept(iob->CA_socket[abport], (struct sockaddr *)&iob->address[abport], (socklen_t*)&iob->addrlen[abport]);\n',
		'   iob->bus_socket[abport] = accept(iob->CA_socket[abport], (struct sockaddr *)&iob->address[abport], (socklen_t*)&iob->addrlen[abport]);\n'
		'   if (iob->bus_socket[abport] >= 0)        // blocking, as accept() gives on Linux; on Windows it inherits O_NONBLOCK\n'
		'      fcntl(iob->bus_socket[abport], F_SETFL, fcntl(iob->bus_socket[abport], F_GETFL, 0) & ~O_NONBLOCK);\n'), (
		'      iob->tag_socket[abport] = accept(iob->CA_socket[abport], (struct sockaddr *)&iob->address[abport], (socklen_t*)&iob->addrlen[abport]);\n',
		'      iob->tag_socket[abport] = accept(iob->CA_socket[abport], (struct sockaddr *)&iob->address[abport], (socklen_t*)&iob->addrlen[abport]);\n'
		'      if (iob->tag_socket[abport] >= 0)\n'
		'         fcntl(iob->tag_socket[abport], F_SETFL, fcntl(iob->tag_socket[abport], F_GETFL, 0) & ~O_NONBLOCK);\n'), (
		'   rc = read_socket( iob->bus_socket[iob->abswitch], iob->buffer, sizeof(iob->buffer));\n',
		'   // read only the 8-byte CCW: write data follows it and must stay in the socket\n'
		'   rc = read_socket( iob->bus_socket[iob->abswitch], iob->buffer, 8);\n'
		'   while (rc > 0 && rc < 8) {\n'
		'      int got = read(iob->bus_socket[iob->abswitch], iob->buffer + rc, 8 - rc);\n'
		'      if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {\n'
		'         usleep(50);                            // on Windows the socket is non-blocking\n'
		'         continue;\n'
		'      }\n'
		'      if (got <= 0)\n'
		'         break;\n'
		'      rc += got;\n'
		'   }\n'), (
		'   Adbg_reg = 0x00;\n',
		'   Adbg_reg = getenv("I3705_CADBG") ? strtol(getenv("I3705_CADBG"), NULL, 16) : 0x00;\n'
		'   setvbuf(A_trace, NULL, _IOLBF, 0);\n'), (
		'            pendingrcv = 0;\n'
		'            while (pendingrcv != ccw.count)\n'
		'               ioctl(iob->bus_socket[iob->abswitch], FIONREAD, &pendingrcv);\n'
		'            rc = recv( iob->bus_socket[iob->abswitch], iob->chainbuf + iob->chainbl, sizeof(iob->chainbuf)-iob->chainbl, 0);\n',
		'            // read exactly the CCW byte count; FIONREAD may report less than what is queued\n'
		'            rc = 0;\n'
		'            while (rc < ccw.count && rc < (int)sizeof(iob->chainbuf) - iob->chainbl) {\n'
		'               int got = recv(iob->bus_socket[iob->abswitch], iob->chainbuf + iob->chainbl + rc,\n'
		'                              ccw.count - rc, 0);\n'
		'               if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {\n'
		'                  usleep(50);                   // on Windows the socket is non-blocking\n'
		'                  continue;\n'
		'               }\n'
		'               if (got <= 0)\n'
		'                  break;\n'
		'               rc += got;\n'
		'            }\n')],
}

for p in sys.argv[1:]:
	name = p.replace('\\', '/').split('/')[-1]
	s = open(p, encoding='latin-1').read()
	for old, new in FIXES[name]:
		if new in s:
			print('already patched', p)
			continue
		assert s.count(old) == 1, 'pattern not found once in ' + p
		s = s.replace(old, new)
		print('patched', p)
	open(p, 'w', encoding='latin-1', newline='').write(s)
