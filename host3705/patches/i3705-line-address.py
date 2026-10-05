#!/usr/bin/env python3
"""i3705-line-address.py <i3705_lib.c|i3705_chan_T2.c>... - with I3705_LINE_ADDR set, bind the line sockets and the
channel adapter socket to that address instead of the first interface or every interface (the scripts use 127.0.0.1,
so nothing listens outside the machine)."""
import sys

for p in sys.argv[1:]:
	s = open(p, encoding='latin-1').read()
	if 'I3705_LINE_ADDR' in s:
		print('already patched', p)
		continue
	if p.endswith('i3705_lib.c'):
		old = '   getifaddrs(&nwaddr);      /* Get TCP network address */\n'
		end = '   printf("\\rLIB: Using TCP network Address %s on %s for 327x connections\\n", ipaddr, ifa->ifa_name);\n'
		a = s.index(old)
		b = s.index(end, a) + len(end)
		new = ('   if ((ipaddr = getenv("I3705_LINE_ADDR")) != NULL) {   /* fixed address for the line sockets */\n'
			'      printf("\\rLIB: Using TCP network Address %s on %s for 327x connections\\n", ipaddr, "I3705_LINE_ADDR");\n'
			'   } else {\n' + s[a:b] + '   }\n')
		s = s[:a] + new + s[b:]
	else:
		old = '   iob->address[abport].sin_addr.s_addr = INADDR_ANY;\n'
		assert s.count(old) == 1, p
		s = s.replace(old, '   iob->address[abport].sin_addr.s_addr = getenv("I3705_LINE_ADDR") ?   // channel on that address only\n'
			'      inet_addr(getenv("I3705_LINE_ADDR")) : INADDR_ANY;\n')
	open(p, 'w', encoding='latin-1', newline='').write(s)
	print('patched', p)
