#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "/sys/src/cmd/nusb/lib/usb.h"


typedef struct Devfile Devfile;
typedef struct Pset Pset;


struct Devfile {
	char	*name;
	char*	(*doread)(Req*);
	char*	(*dowrite)(Req*);
	int		mode;
};


struct Pset {
	int dofeed;
	int docut;
	uchar set;
	uchar just;
	uchar feed;
};


void	rstart(Srv *);
void	rend(Srv *);
void	ropen(Req *r);
void	rread(Req *r);
void	rwrite(Req *r);
char*	wrprint(Req *r);
char*	rdctl(Req *r);
char*	wrctl(Req *r);


Srv s = {
	.start = rstart,
	.open = ropen,
	.read = rread,
	.write = rwrite,
	.end = rend,
};


Devfile files[] = {
	{ "ctl", rdctl, wrctl, 0666 },
	{ "print", nil, wrprint, 0222 },
};


Pset pset;
char *devno;
int epfd;
Dev *d;		/* control */
Dev *dbo;	/* bulk out */



/* standard ESC/POS commands */
static char init[2] = {0x1B, 0x40};
static char bang[3] = {0x1B, 0x21, 0x00};
static char cut[3] = {0x1D, 0x56, 0x00};
static char lj[3] = {0x1B, 0x61, 0x00};
static char cj[3] = {0x1B, 0x61, 0x01};
static char rj[3] = {0x1B, 0x61, 0x02};
static char unln[3] = {0x1B, 0x2D, 0x01};
static char dbls[3] = {0x1B, 0x47, 0x01};
static char emph[3] = {0x1B, 0x45, 0x01};
static char feed[3] = {0x1B, 0x64, 0x05};


char*
rdctl(Req *r)
{
	char out[512], *s, *e;
	char *pfont, *pbold, *puline, *ptall, *pwide, *pjust;
	

	pfont = (pset.set & 0x01)? "small" : "big";
	pbold = (pset.set & 0x04)? "on" : "off";
	puline = (pset.set & 0x80)? "on" : "off";
	ptall = (pset.set & 0x10)? "on" : "off";
	pwide = (pset.set & 0x20)? "on" : "off";

	switch(pset.just){
	case(0x00):
		pjust = "left";
		break;
	case(0x01):
		pjust = "center";
		break;
	case(0x02):
		pjust = "right";
		break;
	default:
		pjust = "☺";
	}

	s = out;
	e = out + sizeof(out);
	s = seprint(s, e, "font %s\n", pfont);
	s = seprint(s, e, "bold %s\n", pbold);
	s = seprint(s, e, "underline %s\n", puline);
	s = seprint(s, e, "tall %s\n", ptall);
	s = seprint(s, e, "wide %s\n", pwide);
	s = seprint(s, e, "justified %s\n", pjust);
	s = seprint(s, e, "feed %ud\n", pset.feed);
	s = seprint(s, e, "autofeed %s\n", pset.dofeed? "on" : "off");
	s = seprint(s, e, "autocut %s\n", pset.docut? "on" : "off");

	readstr(r, out);
	return(nil);	
}


char*
wrctl(Req *r)
{
	char buf[32];
	char *cmd[2];
	char pos[3];
	int n, t;


	memmove(buf, r->ifcall.data, sizeof(buf));

	t = tokenize(buf, cmd, 2);

	if(!t)
		return("no command");

	n = -1;

	if(strcmp(cmd[0], "font") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x21;
		if(strcmp(cmd[1], "big") == 0){
			pset.set &= ~0x01;
		}
		if(strcmp(cmd[1], "small") == 0){
			pset.set |= 0x01;
		}
		pos[2] = pset.set;
		n = write(epfd, pos, 3);
		goto Done;
	}

	if(strcmp(cmd[0], "bold") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x21;
		if(strcmp(cmd[1], "off") == 0){
			pset.set &= ~0x08;
		}
		if(strcmp(cmd[1], "on") == 0){
			pset.set |= 0x08;
		}
		pos[2] = pset.set;
		n = write(epfd, pos, 3);
		goto Done;
	}

	if(strcmp(cmd[0], "underline") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x21;
		if(strcmp(cmd[1], "off") == 0){
			pset.set &= ~0x80;
		}
		if(strcmp(cmd[1], "on") == 0){
			pset.set |= 0x80;
		}
		pos[2] = pset.set;
		n = write(epfd, pos, 3);
		goto Done;
	}

	if(strcmp(cmd[0], "tall") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x21;
		if(strcmp(cmd[1], "off") == 0){
			pset.set &= ~0x10;
		}
		if(strcmp(cmd[1], "on") == 0){
			pset.set |= 0x10;
		}
		pos[2] = pset.set;
		n = write(epfd, pos, 3);
		goto Done;
	}

	if(strcmp(cmd[0], "wide") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x21;
		if(strcmp(cmd[1], "off") == 0){
			pset.set &= ~0x20;
		}
		if(strcmp(cmd[1], "on") == 0){
			pset.set |= 020;
		}
		pos[2] = pset.set;
		n = write(epfd, pos, 3);
		goto Done;
	}

	if(strcmp(cmd[0], "autofeed") == 0){
		if(strcmp(cmd[1], "off") == 0){
			pset.dofeed = 0;
		}
		if(strcmp(cmd[1], "on") == 0){
			pset.dofeed = 1;
		}
		n = 1;
		goto Done;
	}

	if(strcmp(cmd[0], "autocut") == 0){
		if(strcmp(cmd[1], "off") == 0){
			pset.docut = 0;
		}
		if(strcmp(cmd[1], "on") == 0){
			pset.docut = 1;
		}
		n = 1;
		goto Done;
	}

	if(strcmp(cmd[0], "justified") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x61;
		if(strcmp(cmd[1], "left") == 0){
			pset.just = 0x00;
		}
		if(strcmp(cmd[1], "center") == 0){
			pset.just = 0x01;
		}
		if(strcmp(cmd[1], "right") == 0){
			pset.just = 0x02;
		}
		pos[2] = pset.just;
		n = write(epfd, pos, 3);
	}

	if(strcmp(cmd[0], "feed") == 0){
		pset.feed = (uchar)strtoul(cmd[1], nil, 0);
		pos[0] = 0x1B;
		pos[1] = 0x64;
		pos[2] = pset.feed;
		n = write(epfd, pos, 3);
		goto Done;
	}

	if(strcmp(cmd[0], "cut") == 0){
		pos[0] = 0x1B;
		pos[1] = 0x64;
		pos[2] = 0x05;
		n = write(epfd, pos, 3);
		pos[0] = 0x1D;
		pos[1] = 0x56;
		pos[2] = 0x00;
		n = write(epfd, pos, 3);
		goto Done;
	}

Done:
	if(n > 0){
		r->ofcall.count = r->ifcall.count;
		return nil;
	}

	return("no such command");
}


char*
wrprint(Req *r)
{
	int n;
	uchar pos[3];


	n = write(epfd, r->ifcall.data, r->ifcall.count);

	if(pset.dofeed){
		pos[0] = 0x1B;
		pos[1] = 0x64;
		pos[2] = pset.feed;
		write(epfd, pos, 3);
	}

	if(pset.docut){
		pos[0] = 0x1B;
		pos[1] = 0x64;
		pos[2] = pset.feed;
		write(epfd, pos, 3);
		pos[0] = 0x1D;
		pos[1] = 0x56;
		pos[2] = 0x00;
		write(epfd, pos, 3);
	}

	if(n < r->ifcall.count)
		return("wrprint: incomplete");

	r->ofcall.count = n;
	return nil;
}


void
rstart(Srv*)
{
	File *root;
	File *devdir;
	char* user;
	int i, n;

/* setup file tree */
	user = getuser();
	s.tree = alloctree(user, user, 0555, nil);
	if(s.tree == nil)
		sysfatal("initfs: alloctree: %r");
	root = s.tree->root;
	if((devdir = createfile(root, "escposfs", user, DMDIR|0555, nil)) == nil)
		sysfatal("initfs: createfile: scalefs: %r");
	for(i = 0; i < nelem(files); i++){
		if(createfile(devdir, files[i].name, user, files[i].mode, files + i) == nil)
			sysfatal("initfs: createfile: %s: %r", files[i].name);
	}


/* init printer to known state */
	n = 0;
	/* initialize */
	n += write(epfd, init, sizeof(init));
	/* ESC ! print mode 0x00 */
	n += write(epfd, bang, sizeof(bang));
	/* left justified */
	n += write(epfd, lj, sizeof(lj));
	/* set feed to 5 for default */
	n += write(epfd, feed, sizeof(feed));

	if(n != (sizeof(init) + sizeof(bang) + sizeof(lj) + sizeof(feed))){
		fprint(2, "print init failed %d\n", n);
		rend(nil);
	}

/* set Pset */
	pset.set = 0;
	pset.just = 0;
	pset.feed = 5;
	pset.dofeed = 0;
	pset.docut = 0;
}


void
rread(Req *r)
{
	Devfile *f;

	r->ofcall.count = 0;
	f = r->fid->file->aux;
	respond(r, f->doread(r));
}


void
rwrite(Req *r)
{
	Devfile *f;

	if(r->ifcall.count == 0){
		r->ofcall.count = 0;
		respond(r, nil);
		return;
	}

	f = r->fid->file->aux;
	respond(r, f->dowrite(r));
}


void
ropen(Req *r)
{
	respond(r, nil);
}


void
rend(Srv*)
{
	close(epfd);
	closedev(dbo);
	closedev(d);

	postnote(PNGROUP, getpid(), "shutdown");
	threadexitsall(nil);
}


void
usage(void)
{
	fprint(2, "usage: %s [-u devno] [-m mtpt] [-s service]\n", argv0);
	exits("usage");
}


void
threadmain(int argc, char *argv[])
{
	char buf[20];
	Usbdev *ud;
	Ep *epi, *epo;
	int i;
	char *srvname, *mtpt;


	mtpt = "/mnt";
	srvname = "escposfs";

	ARGBEGIN {
	case 'u':
		devno = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND;

	if(devno == nil)
		usage();

	d = getdev(devno);
	if(d == nil)
		sysfatal("getdev: %r");

	ud = d->usb;

	/* setup bulk out endpoint */
	for(i = 0; i < nelem(ud->ep); i++){
		if((epo = ud->ep[i]) == nil)
			continue;
		if(epo->type == Ebulk && epo->dir == Eout)
			break;
	}

	dbo = openep(d, epo);

	if(dbo == nil)
		sysfatal("openep failed: %r");

	if(opendevdata(dbo, OWRITE) < 0)
		sysfatal("opendevdata: %r");

	/* for sending tare command */
	epfd = dbo->dfd;

	snprint(buf, sizeof buf, "%d.pos", d->id);
	threadpostmountsrv(&s, srvname, mtpt, MBEFORE);
	exits(nil);
}
