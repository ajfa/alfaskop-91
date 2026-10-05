#!/usr/bin/env python3
"""read-length.py <comm3705.c|i3705_chan_T2.c>... - a channel protocol change for READ.

The 3705 answers a READ with the data and then one status byte, and comm3705.c cannot tell where the data
ends: when TCP delivers both together, recv() takes the status as data and the next recv waits forever.
Here the 3705 sends the data length in two bytes (high, low) before the data, and comm3705.c reads exactly
that many bytes. Both sides must carry this patch; neither then talks to an unpatched counterpart."""
import sys

FIXES = {
	'comm3705.c': [(
		'            rc = recv(dev->commadpt->busfd, dev->commadpt->inpbuf, count, 0);\n'
		'\n'
		'            dev->commadpt->read_ccw_count++;\n',
		'            /* length-prefixed READ: the 3705 sends the data length in two bytes first */\n'
		'            {\n'
		'               BYTE lenb[2];\n'
		'               U32  want, got = 0;\n'
		'               int  n = 1;\n'
		'               while (got < 2 && (n = recv(dev->commadpt->busfd, lenb + got, 2 - got, 0)) > 0)\n'
		'                  got += n;\n'
		'               want = (got == 2) ? ((U32)lenb[0] << 8) | lenb[1] : 0;\n'
		'               got = 0;\n'
		'               while (got < want && (n = recv(dev->commadpt->busfd, dev->commadpt->inpbuf + got, want - got, 0)) > 0)\n'
		'                  got += n;\n'
		'               rc = (got > count) ? count : got;\n'
		'            }\n'
		'\n'
		'            dev->commadpt->read_ccw_count++;\n')],
	'i3705_chan_T2.c': [(
		'            rc = send(iob->bus_socket[iob->abswitch], (void*)&iob->buffer, wdcnttot, 0);\n',
		'            {                                            // length-prefixed READ: data length first\n'
		'               uint8_t lenb[2] = { (wdcnttot >> 8) & 0xFF, wdcnttot & 0xFF };\n'
		'               send(iob->bus_socket[iob->abswitch], (void*)lenb, 2, 0);\n'
		'            }\n'
		'            rc = send(iob->bus_socket[iob->abswitch], (void*)&iob->buffer, wdcnttot, 0);\n')],
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
