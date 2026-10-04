#!/usr/bin/env python3
"""tk5jobs.py - build the TK5 jobs from members punched out of the running system.

  tk5jobs.py setup  LNKLST00 VATLST00 ATCCON01   > setup.jcl
      add SYS1.SSPLIB to the link list and NCPSSP to the volume attribute list, take the
      fake NCPs out of the VTAM start list, keep backups, remove the fake IFLOADRN from
      SYS1.LINKLIB, copy the sample NCPs N16A and N16B to SYS1.VTAMLST
  tk5jobs.py stage2 PUNCHED-DECK                 > ncpgen2.jcl
      the NCP stage 2 deck punched by stage 1, made runnable while VTAM is up
  tk5jobs.py n16a   N16A                         > updn16a.jcl
      N16A with the SDLC PU P16A20A set up for the Alfaskop 91
"""
import re
import sys

JOBCARD = ["//{name:<8} JOB (1),'{title}',CLASS=A,MSGCLASS=A,MSGLEVEL=(1,1),",
	"//         USER=HERC01,PASSWORD=CUL8TR"]
FAKE_NCP = re.compile(r'^N(0[7-9]|1[0-5])\b')


def cards(path):
	"""the punched cards of one member: columns 1 to 72, no trailing blank cards"""
	out = [l.rstrip('\r\n')[:72].rstrip() for l in open(path, encoding='ascii', errors='replace')]
	while out and not out[-1]:
		out.pop()
	return out


def job(name, title):
	return [l.format(name=name, title=title) for l in JOBCARD]


def iebupdte(step, lib, members):
	out = ['//%-8s EXEC PGM=IEBUPDTE,PARM=MOD' % step, '//SYSPRINT DD SYSOUT=A',
		'//SYSUT1   DD DSN=%s,DISP=SHR' % lib, '//SYSUT2   DD DSN=%s,DISP=SHR' % lib, '//SYSIN    DD DATA']
	for name, lines in members:
		assert all(not l.startswith('./') and len(l) <= 80 for l in lines)
		out += ['./ REPL NAME=%s,LIST=ALL' % name] + lines
	return out + ['./ ENDUP', '/*']


def lnklst(lines):
	"""append SYS1.SSPLIB after the last data set"""
	if any('SYS1.SSPLIB' in l for l in lines):
		return lines
	last = max(i for i, l in enumerate(lines) if l.strip() and not l.startswith('*'))
	lines = list(lines)
	lines[last] = lines[last].rstrip().rstrip(',') + ','
	lines.insert(last + 1, ' SYS1.SSPLIB')
	return lines


def vatlst(lines):
	if any(l.startswith('NCPSSP') for l in lines):
		return lines
	return lines + ['NCPSSP,0,2,3350    ,N                  IBM3705_R5 NCP/SSP volume']


def atccon(lines):
	"""drop the fake NCPs N07 to N15 from the start list, keeping the continuation columns right"""
	head = [l for l in lines if l.startswith('*')]
	entries = []
	for l in lines:
		if l.startswith('*') or not l.strip():
			continue
		m = re.match(r'^(\S+?),?\s*(/\*.*?\*/)?\s*X?$', l[:72].rstrip())
		assert m, 'unexpected ATCCON01 card: ' + l
		if not FAKE_NCP.match(m.group(1)):
			entries.append((m.group(1), m.group(2) or ''))
	out = head
	for i, (name, comment) in enumerate(entries):
		last = i == len(entries) - 1
		text = (name + ('' if last else ',')).ljust(35) + comment
		out.append((text.ljust(71) + ('' if last else 'X')).rstrip())
	return out


def setup(lnk, vat, con):
	out = job('SETUP', 'NCP SETUP')
	out += ['//CATALOG  EXEC PGM=IDCAMS', '//SYSPRINT DD SYSOUT=A', '//SYSIN    DD *']
	for ds in ('GEN3705', 'MAC3705', 'NCPOBJ1', 'NCPSAMP', 'NCPSTG1', 'OBJ3705', 'SSPLIB'):
		out.append('  DEFINE NONVSAM (NAME(SYS1.%s) DEVT(3350) VOL(NCPSSP))' % ds)
	out.append('/*')
	for lib, mem, bk in (('SYS1.PARMLIB', 'LNKLST00', 'LNKLSTBK'), ('SYS1.PARMLIB', 'VATLST00', 'VATLSTBK'),
			('SYS1.VTAMLST', 'ATCCON01', 'ATCCONBK')):
		out += ['//BK%-6s EXEC PGM=IEBGENER' % mem[:6], '//SYSPRINT DD SYSOUT=A', '//SYSIN    DD DUMMY',
			'//SYSUT1   DD DSN=%s(%s),DISP=SHR' % (lib, mem), '//SYSUT2   DD DSN=%s(%s),DISP=SHR' % (lib, bk)]
	out += iebupdte('PARMLIB', 'SYS1.PARMLIB', [('LNKLST00', lnklst(cards(lnk))), ('VATLST00', vatlst(cards(vat)))])
	out += iebupdte('VTAMLST', 'SYS1.VTAMLST', [('ATCCON01', atccon(cards(con)))])
	out += ['//NOFAKE   EXEC PGM=IEHPROGM', '//SYSPRINT DD SYSOUT=A', '//DD1      DD UNIT=3390,VOL=SER=TK5RES,DISP=OLD',
		'//SYSIN    DD *', '  SCRATCH DSNAME=SYS1.LINKLIB,VOL=3390=TK5RES,MEMBER=IFLOADRN', '/*']
	out += ['//COPYNCP  EXEC PGM=IEBCOPY', '//SYSPRINT DD SYSOUT=A',
		'//IN       DD DSN=SYS1.NCPSAMP,DISP=SHR,UNIT=3350,VOL=SER=NCPSSP', '//OUT      DD DSN=SYS1.VTAMLST,DISP=SHR',
		'//SYSUT3   DD UNIT=SYSDA,SPACE=(TRK,(5,5))', '//SYSIN    DD *',
		'  COPY OUTDD=OUT,INDD=IN', '  SELECT MEMBER=(N16A,N16B)', '/*']
	# room in SYS1.VTAMLIB for the NCP: the resource tables of the fake NCPs go, then compress
	out += ['//NOFAKRRT EXEC PGM=IEHPROGM', '//SYSPRINT DD SYSOUT=A', '//DD1      DD UNIT=3390,VOL=SER=TK5RES,DISP=OLD',
		'//SYSIN    DD *']
	out += ['  SCRATCH DSNAME=SYS1.VTAMLIB,VOL=3390=TK5RES,MEMBER=%s' % m
		for m in ('N08R', 'N10R', 'N11R', 'N12R', 'N13R', 'N14R', 'N15R')]
	out += ['/*', '//COMPRESS EXEC PGM=IEBCOPY,COND=EVEN', '//SYSPRINT DD SYSOUT=A',
		'//LIB      DD DSN=SYS1.VTAMLIB,DISP=SHR', '//SYSUT3   DD UNIT=SYSDA,SPACE=(TRK,(5,5))', '//SYSIN    DD *',
		'  COPY OUTDD=LIB,INDD=LIB', '/*', '//']
	return out


def stage2(deck):
	lines = [l.rstrip('\r\n') for l in open(deck, encoding='ascii', errors='replace')]
	while lines and not lines[-1].strip():
		lines.pop()
	assert lines[0].startswith('//') and ' JOB ' in lines[0], lines[0]
	name = lines[0][2:].split()[0]
	lines[0] = '//%s JOB 1,NCPSYSGEN,MSGLEVEL=1,CLASS=A,MSGCLASS=A,' % name
	lines.insert(1, '//         USER=HERC01,PASSWORD=CUL8TR')
	n = 0
	for i, l in enumerate(lines):
		# the link into SYS1.VTAMLIB would wait for VTAM to free the library
		if l.startswith('//SYSLMOD DD DSN=SYS1.VTAMLIB,DISP=OLD'):
			lines[i] = l.replace('DISP=OLD', 'DISP=SHR')
			n += 1
	assert n, 'no SYSLMOD for SYS1.VTAMLIB in the stage 2 deck'
	return lines


def n16a(member):
	lines = cards(member)
	pu = next(i for i, l in enumerate(lines) if l.startswith('P16A20A  PU'))
	end = next(i for i in range(pu, len(lines)) if lines[i].startswith('T16A20A1'))
	done = set()

	def card(text):
		return (text.ljust(71) + 'X').rstrip()

	for i in range(pu, end):
		if 'SSCPFM=' in lines[i]:
			lines[i] = card('               SSCPFM=USSSCS,')  # the A91 talks to the SSCP in characters
			done.add('sscpfm')
	for i in range(pu, end):
		if 'MODETAB=' in lines[i]:
			lines[i] = card('               MODETAB=BSPLMT02,')
			lines.insert(i + 1, card('               DLOGMOD=MHP3278E,'))
			done.add('modetab')
			break
	assert done == {'sscpfm', 'modetab'}, done
	return job('UPDN16A', 'N16A FOR A91') + iebupdte('UPDATE', 'SYS1.VTAMLST', [('N16A', lines)]) + ['//']


if __name__ == '__main__':
	cmd, args = sys.argv[1], sys.argv[2:]
	out = {'setup': lambda: setup(*args), 'stage2': lambda: stage2(*args), 'n16a': lambda: n16a(*args)}[cmd]()
	for l in out:
		assert len(l) <= 80, l
	print('\n'.join(out))
