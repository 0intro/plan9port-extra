/*
 * Trivial web browser
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <event.h>
#include <plumb.h>
#include <cursor.h>
#include <panel.h>
#include "mothra.h"
#include "rtext.h"
int verbose=0;		/* -v flag causes html errors to appear in error log */
int inlinepix=1;	/* get inline pix, by default */
int defdisplay=1;	/* is the default (initial) display visible? */
Panel *root;	/* the whole display */
Panel *alt;	/* the alternate display */
Panel *alttext;	/* the alternate text window */
Panel *cmd;	/* command entry */
Panel *curttl;	/* label giving the title of the visible text */
Panel *cururl;	/* label giving the url of the visible text */
Panel *list;	/* list of previously acquired www pages */
Panel *msg;	/* message display */
Panel *selbox;	/* show selection here */
Panel *menu3;	/* button 3 menu */
Mouse mouse;	/* current mouse data */
Www www[NWWW];
char helpfile[] = "file:/sys/lib/mothra/help.html";
Url defurl={
	"http://plan9.bell-labs.com/",
	0,
	"plan9.bell-labs.com",
	"/",
	"",
	"", "", "",
	80,
	HTTP,
	HTML
};
Url badurl={
	"No file loaded",
	0,
	"",
	"/dev/null",
	"", "", "",
	"",
	0,
	FILE,
	HTML
};
Cursor patientcurs={
	0, 0,
	0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x07, 0xe0,
	0x07, 0xe0, 0x07, 0xe0, 0x03, 0xc0, 0x0F, 0xF0,
	0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8,
	0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC, 0x3F, 0xFC,

	0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x04, 0x20,
	0x04, 0x20, 0x06, 0x60, 0x02, 0x40, 0x0C, 0x30,
	0x10, 0x08, 0x14, 0x08, 0x14, 0x28, 0x12, 0x28,
	0x0A, 0x50, 0x16, 0x68, 0x20, 0x04, 0x3F, 0xFC,
};
Cursor confirmcurs={
	0, 0,
	0x0F, 0xBF, 0x0F, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFE,
	0xFF, 0xFE, 0xFF, 0xFF, 0x00, 0x03, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFC,

	0x00, 0x0E, 0x07, 0x1F, 0x03, 0x17, 0x73, 0x6F,
	0xFB, 0xCE, 0xDB, 0x8C, 0xDB, 0xC0, 0xFB, 0x6C,
	0x77, 0xFC, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03,
	0x94, 0xA6, 0x63, 0x3C, 0x63, 0x18, 0x94, 0x90
};
Cursor readingcurs={
	-10, -3,
	0x00, 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x0F, 0xF0,
	0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x1F, 0xF0,
	0x3F, 0xF0, 0x7F, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFB, 0xFF, 0xF3, 0xFF, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xE0,
	0x07, 0xE0, 0x01, 0xE0, 0x03, 0xE0, 0x07, 0x60,
	0x0E, 0x60, 0x1C, 0x00, 0x38, 0x00, 0x71, 0xB6,
	0x61, 0xB6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
int nwww=1;
Www *current=0;
Url *selection=0;
int logfile;
void docmd(Panel *, char *);
void doprev(Panel *, int, int);
void selurl(char *);
void setcurrent(int, char *);
char *genwww(Panel *, int);
void updtext(Www *);
void dolink(Panel *, int, Rtext *);
void hit3(int, int);
char *buttons[]={
	"alt display",
	"snarf url",
	"paste",
	"inline pix",
	"fix cmap",
	"save hit",
	"hit list",
	"exit",
	0
};
void err(Display *d, char *msg){
	USED(d);
	fprint(2, "err: %s (%r)\n", msg);
	abort();
}
int subpanel(Panel *obj, Panel *subj){
	if(obj==0) return 0;
	if(obj==subj) return 1;
	for(obj=obj->child;obj;obj=obj->next)
		if(subpanel(obj, subj)) return 1;
	return 0;
}
/*
 * Make sure that the keyboard focus is on-screen, by adjusting it to
 * be the cmd entry if necessary.
 */
void adjkb(void){
	Rtext *t;
	int yoffs;
	extern Panel *pl_kbfocus;	/* this is a secret panel library name */
	yoffs=text->r.min.y-plgetpostextview(text);
	for(t=current->text;t;t=t->next) if(!eqrect(t->r, Rect(0,0,0,0))){
		if(t->r.max.y+yoffs>text->r.max.y) break;
		if(t->r.min.y+yoffs>=text->r.min.y
		&& t->b==0
		&& subpanel(t->p, pl_kbfocus)) return;
	}
	plgrabkb(cmd);
}
void mkpanels(void){
	Panel *p, *bar;
	menu3=plmenu(0, 0, (Icon**)buttons, PACKN|FILLX, hit3);
	root=plpopup(root, EXPAND, 0, 0, menu3);
		p=plgroup(root, PACKN|FILLX);
			msg=pllabel(p, PACKN|FILLX, "mothra");
			plplacelabel(msg, PLACEW);
			selbox=pllabel(p, PACKN|FILLX, "");
			plplacelabel(selbox, PLACEW);
			pllabel(p, PACKW, "command:");
			cmd=plentry(p, PACKN|FILLX, 0, "", docmd);
		p=plgroup(root, PACKN|FILLX);
			bar=plscrollbar(p, PACKW);
			list=pllist(p, PACKN|FILLX, genwww, 8, doprev);
			plscroll(list, 0, bar);
		p=plgroup(root, PACKN|FILLX);
			pllabel(p, PACKW, "Title:");
			curttl=pllabel(p, PACKE|EXPAND, "Initializing");
			plplacelabel(curttl, PLACEW);
		p=plgroup(root, PACKN|FILLX);
			pllabel(p, PACKW, "Url:");
			cururl=pllabel(p, PACKE|EXPAND, "---");
			plplacelabel(cururl, PLACEW);
		p=plgroup(root, PACKN|EXPAND);
			bar=plscrollbar(p, PACKW);
			text=pltextview(p, PACKE|EXPAND, Pt(0, 0), 0, dolink);
			plscroll(text, 0, bar);
	plgrabkb(cmd);
	alt=plpopup(0, PACKE|EXPAND, 0, 0, menu3);
		bar=plscrollbar(alt, PACKW);
		alttext=pltextview(alt, PACKE|EXPAND, Pt(0, 0), 0, dolink);
		plscroll(alttext, 0, bar);
}
#ifdef notyet
/* replicate (from top) value in v (n bits) until it fills a ulong */
ulong rep(ulong v, int n){
	int o;
	ulong rv;
	rv=0;
	for(o=32-n;o>=0;o-=n) rv|=v<<o;
	if(o!=-n) rv|=v>>-o;
	return rv;
}
int getmap(char *cmapname){
	int nbit, npix;
	int i, j;
	if(!getcmap(cmapname, cmap)) return 0;
	nbit=screen->depth;
	npix=1<<nbit;
	for(j=0;j!=npix;j++){
		i=3*255*j/(npix-1);
		map[npix-1-j].red=rep(cmap[i], 8);
		map[npix-1-j].green=rep(cmap[i+1], 8);
		map[npix-1-j].blue=rep(cmap[i+2], 8);
	}
	return 1;
}
#endif
void killcohort(void){
	int i;
	for(i=0;i!=3;i++){	/* It's a long way to the kitchen */
		postnote(PNGROUP, getpid(), "kill\n");
		sleep(1);
	}
}
void dienow(void *v, char *c){
	USED(v);
	USED(c);
	noted(NDFLT);
}
int mkmfile(char *stem, int mode){
	char *henv;
	char filename[512];
	int f;
	if(home[0]=='\0'){
		henv=getenv("home");
		if(henv){
			sprint(home, "%s/lib", henv);
			f=create(home, OREAD, DMDIR|0777);
			if(f!=-1) close(f);
			sprint(home, "%s/lib/mothra", henv);
			f=create(home, OREAD, DMDIR|0777);
			if(f!=-1) close(f);
		}
		else
			strcpy(home, "/tmp");
	}
	sprint(filename, "%s/%s", home, stem);
	f=create(filename, OWRITE, mode);
	if(f==-1)
		f=create(stem, OWRITE, mode);
	return f;
}
void threadmain(int argc, char *argv[]){
	Event e;
	enum { Eplumb = 128 };
	Plumbmsg *pm;
	char *url;
	int errfile;
	ARGBEGIN{
	case 'd': debug++; break;
	case 'i': inlinepix=0; break;
	case 'v': verbose=1; break;
	default:  goto Usage;
	}ARGEND
	/*
	 * so that we can stop all subprocesses with a note,
	 * and to isolate rendezvous from other processes
	 */
	rfork(RFNOTEG|RFNAMEG|RFREND);
	atexit(killcohort);
	switch(argc){
	default:
	Usage:
		fprint(2, "Usage: %s [-d] [url]\n", argv[0]);
		exits("usage");
	case 0:
		url=getenv("url");
		if(url==0 || url[0]=='\0')
			url="file:/sys/lib/mothra/start.html";
		break;
	case 1: url=argv[0]; break;
	}
	errfile=mkmfile("mothra.err", 0666);
	if(errfile!=-1){
		dup(errfile, 2);
		close(errfile);
	}
	logfile=mkmfile("mothra.log", 0666|DMAPPEND);
	initdraw(err,0,"mothra");
	memimageinit();
	
	chrwidth=stringwidth(font, "0");
	pltabsize(chrwidth, 8*chrwidth);
#ifdef notyet
	if(getmap("/lib/fb/cmap/rgbv") && screen->depth==8) wrcolmap(screen, map);
#endif
	einit(Emouse|Ekeyboard);
	eplumb(Eplumb, "web");
	etimer(0, 1000);
	plinit(screen->depth);
	if(debug) notify(dienow);
	getfonts();
	hrule=allocimage(display, Rect(0, 0, 2048, 5), screen->chan, 0, DWhite);
	if(hrule==0){
		fprint(2, "%s: can't allocimage!\n", argv[0]);
		exits("no mem");
	}
	draw(hrule, Rect(0,1,1280,3), display->black, 0, ZP);
	linespace=allocimage(display, Rect(0, 0, 2048, 5), screen->chan, 0, DWhite);
	if(linespace==0){
		fprint(2, "%s: can't allocimage!\n", argv[0]);
		exits("no mem");
	}
	bullet=allocimage(display, Rect(0,0,25, 8), screen->chan, 0, DWhite);
	fillellipse(bullet, Pt(4,4), 3, 3, display->black, ZP);
	www[0].text=0;
	www[0].url=&badurl;
	www[0].base=&badurl;
	strcpy(www[0].title, "See error message above");
	plrtstr(&www[0].text, 0, 0, font, "See error message above", 0, 0);
	www[0].alldone=1;
	mkpanels();
	eresized(0);
	geturl(url, GET, 0, 1, 0);
	if(logfile==-1) message("Can't open log file");
	mouse.buttons=0;
	for(;;){
		if(mouse.buttons==0 && current){
			if(current->finished){
				updtext(current);
				current->finished=0;
				current->changed=0;
				current->alldone=1;
				esetcursor(0);
			}
			else if(current->changed){
				updtext(current);
				current->changed=0;
			}
		}
		switch(event(&e)){
		case Ekeyboard:
			adjkb();
			plkeyboard(e.kbdc);
			break;
		case Emouse:
			mouse=e.mouse;
			plmouse(root, e.mouse);
			break;
		case Eplumb:
			pm=e.v;
			if(pm->ndata > 0)
				geturl(pm->data, GET, 0, 1, 0);
			plumbfree(pm);
			break;
		}
	}
}
void message(char *s, ...){
	static char buf[1024];
	char *out;
	va_list args;
	va_start(args, s);
	out = buf + vsnprint(buf, sizeof(buf), s, args);
	va_end(args);
	*out='\0';
	plinitlabel(msg, PACKN|FILLX, buf);
	if(defdisplay) pldraw(msg, screen);
}
void showsel(char *s){
	static char buf[1024];
	sprint(buf, "selection: %s", s);
	plinitlabel(selbox, PACKN|FILLX, buf);
	if(defdisplay) pldraw(selbox, screen);
}
void htmlerror(char *name, int line, char *m, ...){
	static char buf[1024];
	char *out;
	va_list args;
	if(verbose){
		va_start(args, m);
		out=buf+sprint(buf, "%s: line %d: ", name, line);
		out+=vsnprint(out, sizeof(buf)-(out-buf)-1, m, args);
		va_end(args);
		*out='\0';
		fprint(2, "%s\n", buf);
	}
}
void eresized(int new){
	Rectangle r;

	if(new && getwindow(display, Refnone) == -1) {
		fprint(2, "getwindow: %r\n");
		exits("getwindow");
	}
	r=screen->r;
	r=insetrect(r, 4);
	message("");
	plinitlabel(curttl, PACKE|EXPAND, "---");
	plinitlabel(cururl, PACKE|EXPAND, "---");
	plpack(root, r);
	if(current){
		plinitlabel(curttl, PACKE|EXPAND, current->title);
		plinitlabel(cururl, PACKE|EXPAND, current->url->fullname);
	}
	draw(screen, r, display->white, 0, ZP);
	pldraw(root, screen);
}
void *emalloc(int n){
	void *v;
	v=malloc(n);
	if(v==0){
		fprint(2, "out of space\n");
		exits("no mem");
	}
	return v;
}
char *genwww(Panel *p, int index){
	USED(p);
	static char buf[1024];
	int i;
	for(i=1;i!=nwww;i++) if(index==www[i].index){
		sprint(buf, "%2d %s", i, www[i].title);
		return buf;
	}
	return 0;
}
void donecurs(void){
	esetcursor(current && current->alldone?0:&readingcurs);
}
/*
 * selected text should be a url.
 * get the document, scroll to the given tag
 */
void setcurrent(int index, char *tag){
	Www *new;
	Rtext *tp;
	Action *ap;
	int i;
	new=&www[index];
	if(new==current && (tag==0 || tag[0]==0)) return;
	if(current)
		current->yoffs=plgetpostextview(text);
	current=new;
	plinitlabel(curttl, PACKE|EXPAND, current->title);
	if(defdisplay) pldraw(curttl, screen);
	plinitlabel(cururl, PACKE|EXPAND, current->url->fullname);
	if(defdisplay) pldraw(cururl, screen);
	plinittextview(text, PACKE|EXPAND, Pt(0, 0), current->text, dolink);
	if(tag && tag[0]){
		for(tp=current->text;tp;tp=tp->next){
			ap=tp->user;
			if(ap && ap->name && strcmp(ap->name, tag)==0){
				current->yoffs=tp->topy;
				break;
			}
		}
	}
	plsetpostextview(text, current->yoffs);
	if(index!=0){
		for(i=1;i!=nwww;i++) if(www[i].index<current->index) www[i].index++;
		current->index=0;
		plinitlist(list, PACKN|FILLX, genwww, 8, doprev);
		if(defdisplay) pldraw(list, screen);
	}
	donecurs();
	flushimage(display, 1);
}
char *arg(char *s){
	do ++s; while(*s==' ' || *s=='\t');
	return s;
}
void save(Url *url, char *name){
	int ofd, ifd, n;
	char buf[4096];
	ofd=create(name, OWRITE, 0666);
	if(ofd==-1){
		message("save: %s: %r", name);
		return;
	}
	esetcursor(&patientcurs);
	ifd=urlopen(url, GET, 0);
	donecurs();
	if(ifd==-1){
		message("save: %s: %r", selection->fullname);
		close(ofd);
	}
	switch(rfork(RFNOTEG|RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		message("Can't fork -- please wait");
		esetcursor(&patientcurs);
		while((n=read(ifd, buf, 4096))>0)
			write(ofd, buf, n);
		donecurs();
		break;
	case 0:
		while((n=read(ifd, buf, 4096))>0)
			write(ofd, buf, n);
		if(n==-1) fprint(2, "save: %s: %r\n", url->fullname);
		_exits(0);
	}
	close(ifd);
	close(ofd);
}
void screendump(char *name, int full){
	Image *b;
	int fd;
	Rectangle r;
	fd=create(name, OWRITE, 0666);
	if(fd==-1){
		message("can't create %s", name);
		return;
	}
	if(full) r=screen->clipr;
	else r=text->r;
	b=allocimage(display, rectsubpt(r, r.min), screen->chan, 0, DNofill);
	if(b){
		draw(b, r, screen, 0, ZP);
		writeimage(fd, b, 0);
		freeimage(b);
	}
	else
		writeimage(fd, screen, 0);
	close(fd);
}
/*
 * user typed a command.
 */
void docmd(Panel *p, char *s){
	USED(p);
	while(*s==' ' || *s=='\t') s++;
	/*
	 * Non-command does a get on the url
	 */
	if(s[0]!='\0' && s[1]!='\0' && s[1]!=' ')
		geturl(s, GET, 0, 1, 0);
	else switch(s[0]){
	default:
		message("Unknown command %s, type h for help", s);
		break;
	case '?':
	case 'h':
		geturl(helpfile, GET, 0, 1, 0);
		break;
	case 'g':
		s=arg(s);
		if(*s=='\0'){
			if(selection)
				geturl(selection->fullname, GET, 0, 1, 0);
			else
				message("no url selected");
		}
		else geturl(s, GET, 0, 1, 0);
		break;
	case 'r':
		s = arg(s);
		if(*s == '\0')
			s = selection ? selection->fullname : helpfile;
		geturl(s, GET, 0, 0, 0);
		break;
	case 'W':
		s=arg(s);
		if(s=='\0'){
			message("Usage: W file");
			break;
		}
		screendump(s, 1);
		break;
	case 'w':
		s=arg(s);
		if(s=='\0'){
			message("Usage: w file");
			break;
		}
		screendump(s, 0);
		break;
	case 's':
		s=arg(s);
		if(*s=='\0'){
			if(selection){
				s=strrchr(selection->fullname, '/');
				if(s) s++;
			}
			if(s==0 || *s=='\0'){
				message("Usage: s file");
				break;
			}
		}
		save(selection, s);
		break;
	case 'q':
		draw(screen, screen->r, display->white, 0, ZP);
		exits(0);
	}
	plinitentry(cmd, EXPAND, 0, "", docmd);
	if(defdisplay) pldraw(cmd, screen);
}
void hiturl(int buttons, char *url, int map){
	message("");
	switch(buttons){
	case 1: geturl(url, GET, 0, 1, map); break;
	case 2: selurl(url); break;
	case 4: message("Button 3 hit on url can't happen!"); break;
	}
}
/*
 * user selected from the list of available pages
 */
void doprev(Panel *p, int buttons, int index){
	int i;
	USED(p);
	message("");
	for(i=1;i!=nwww;i++) if(www[i].index==index){
		switch(buttons){
		case 1: setcurrent(i, 0);	/* no break ... */
		case 2: selurl(www[i].url->fullname); break;
		case 4: message("Button 3 hit on page can't happen!"); break;
		}
		break;
	}
}
/*
 * Follow an html link
 */
void dolink(Panel *p, int buttons, Rtext *word){
	char mapurl[512];
	Action *a;
	Point coord;
	int yoffs;
	USED(p);
	a=word->user;
	if(a->link){
		if(a->ismap){
			yoffs=plgetpostextview(p);
			coord=subpt(subpt(mouse.xy, word->r.min), p->r.min);
			sprint(mapurl, "%s?%d,%d", a->link, coord.x, coord.y+yoffs);
			hiturl(buttons, mapurl, 1);
		}
		else
			hiturl(buttons, a->link, 0);
	}
}
void filter(char *cmd, int fd){
	flushimage(display, 1);
	switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		message("Can't fork!");
		break;
	case 0:
		close(0);
		dup(fd, 0);
		close(fd);
		execl("/bin/rc", "rc", "-c", cmd, 0);
		message("Can't exec /bin/rc!");
		_exits(0);
	default:
		break;
	}
	close(fd);
}
void gettext(Www *w, int fd, int type){
	flushimage(display, 1);
	switch(rfork(RFFDG|RFPROC|RFNOWAIT|RFMEM)){
	case -1:
		message("Can't fork, please wait");
		if(type==HTML)
			plrdhtml(w->url->fullname, fd, w);
		else
			plrdplain(w->url->fullname, fd, w);
		break;
	case 0:
		if(type==HTML)
			plrdhtml(w->url->fullname, fd, w);
		else
			plrdplain(w->url->fullname, fd, w);
		_exits(0);
	}
	close(fd);
}
void popwin(char *cmd){
	flushimage(display, 1);
	switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		message("sorry, can't fork to %s", cmd);
		break;
	case 0:
		execl("/bin/window", "window", "100 100 800 800", "rc", "-c", cmd, 0);
		_exits(0);
	}
}
int urlopen(Url *url, int method, char *body){
#ifdef securityHole
	int pfd[2];
#endif
	int fd;
	Url prev;
	int nredir;
	Dir *dir;
	nredir=0;
Again:
	if(++nredir==NREDIR){
		werrstr("redir loop");
		return -1;
	}
	seek(logfile, 0, 2);
	fprint(logfile, "%s\n", url->fullname);
	switch(url->access){
	default:
		werrstr("unknown access type");
		return -1;
	case FTP:
		url->type = suffix2type(url->reltext);
		return ftp(url);
	case HTTP:
		fd=http(url, method, body);
		if(url->type==FORWARD){
			prev=*url;
			crackurl(url, prev.redirname, &prev);
			/*
			 * I'm not convinced that the following two lines are right,
			 * but once I got a redir loop because they were missing.
			 */
			method=GET;
			body=0;
			goto Again;
		}
		return fd;	
	case FILE:
		url->type=suffix2type(url->reltext);
		fd=open(url->reltext, OREAD);
		if(fd!=-1){
			dir=dirfstat(fd);
			if(dir->mode&DMDIR){
				url->type=HTML;
				free(dir);
				return dir2html(url->reltext, fd);
			}
			free(dir);
		}
		return fd;
	case GOPHER:
		return gopher(url);
#ifdef securityHole
	case EXEC:
		/*
		 * Can you say `security hole'?  I knew you could.
		 */
		if(pipe(pfd)==-1) return -1;
		switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
		case -1:
			close(pfd[0]);
			close(pfd[1]);
			return -1;
		case 0:
			dup(pfd[1], 1);
			close(pfd[0]);
			close(pfd[1]);
			execl("/bin/rc", "rc", "-c", url->reltext, 0);
			_exits("no exec!");
		default:
			close(pfd[1]);
			return pfd[0];
		}
#endif
	}
}
int uncompress(char *cmd, int fd){
	int pfd[2];
	if(pipe(pfd)==-1){
		close(fd);
		return -1;
	}
	switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		close(pfd[0]);
		close(pfd[1]);
		close(fd);
		return -1;
	case 0:
		dup(fd, 0);
		dup(pfd[1], 1);
		close(pfd[0]);
		close(pfd[1]);
		close(fd);
		execl(cmd, cmd, 0);
		_exits("no exec!");
	default:
		close(pfd[1]);
		close(fd);
		return pfd[0];
	}
}
/*
 * select the file at the given url
 */
void selurl(char *urlname){
	Url *cur;
	static Url url;
	if(current){
		cur=current->base;
		/*
		 * I believe that the following test should never succeed
		 */
		if(cur==0){
			cur=current->url;
			if(cur==0){
				fprint(2, "bad base & url, current %d, getting %s\n",
					current-www, urlname);
				cur=&defurl;
			}
			else
				fprint(2, "bad base, current %s, getting %s\n",
					current->url->fullname, urlname);
		}
	}
	else cur=&defurl;
	crackurl(&url, urlname, cur);
	selection=&url;
	showsel(selection->fullname);
}
Url *copyurl(Url *u){
	Url *v;
	v=emalloc(sizeof(Url));
	*v=*u;
	return v;
}
/*
 * get the file at the given url
 */
void geturl(char *urlname, int method, char *body, int cache, int map){
	int i, fd;
	char cmd[512];
	int pfd[2];
	selurl(urlname);
	selection->map=map;
	if(cache){
		for(i=1;i!=nwww;i++){
			if(strcmp(www[i].url->fullname, selection->fullname)==0){
				setcurrent(i, selection->tag);
				return;
			}
		}
	}
	message("getting %s", selection->fullname);
	esetcursor(&patientcurs);
	switch(selection->access){
	default:
		message("unknown access %d", selection->access);
		break;
	case TELNET:
		sprint(cmd, "telnet %s", selection->reltext);
		popwin(cmd);
		break;
	case MAILTO:
		if(body){
			/*
			 * Undocumented Mozilla feature
			 */
			pipe(pfd);
			switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
			case -1:
				message("Can't fork!");
				break;
			case 0:
				close(0);
				dup(pfd[1], 0);
				close(pfd[1]);
				close(pfd[0]);
				execl("/bin/upas/send",
					"sendmail", selection->reltext, 0);
				message("Can't exec sendmail");
				_exits(0);
			default:
				close(pfd[1]);
				fprint(pfd[0],
				    "Content-type: application/x-www-form-urlencoded\n"
				    "Subject: Form posted from Mothra\n"
				    "\n"
				    "%s\n", body);
				close(pfd[0]);
				break;
			}
		}
		else{
			sprint(cmd, "mail %s", selection->reltext);
			popwin(cmd);
		}
		break;
	case FTP:
	case HTTP:
	case FILE:
	case EXEC:
	case GOPHER:
		fd=urlopen(selection, method, body);
		if(fd==-1){
			message("%r");
			setcurrent(0, 0);
			break;
		}
		if(selection->type&COMPRESS)
			fd=uncompress("/bin/uncompress", fd);
		else if(selection->type&GUNZIP)
			fd=uncompress("/bin/gunzip", fd);
		else if(selection->type&BZIP2)
			fd=uncompress("/bin/bunzip2", fd);
		switch(selection->type&~COMPRESSION){
		default:
			message("Bad type %x in geturl", selection->type);
			break;
		case PLAIN:
		case HTML:
			if(selection->map){
				if(current && current->base)	/* always succeeds */
					www[nwww].url=copyurl(current->base);
				else{
					fprint(2, "no base for map!\n");
					www[nwww].url=copyurl(selection);
				}
			}
			else
				www[nwww].url=copyurl(selection);
			www[nwww].index=nwww;
			gettext(&www[nwww], fd, selection->type&~COMPRESSION);
			nwww++;
			plinitlist(list, PACKN|FILLX, genwww, 8, doprev);
			if(defdisplay) pldraw(list, screen);
			setcurrent(nwww-1, selection->tag);
			break;
		case POSTSCRIPT:
		case GIF:
		case JPEG:
		case PNG:
		case PDF:
			filter("page -w", fd);
			break;
		case TIFF:
			filter("/sys/lib/mothra/tiffview", fd);
			break;
		case XBM:
			filter("fb/xbm2pic|fb/9v", fd);
			break;
		}
	}
	donecurs();
}
void updtext(Www *w){
	Rtext *t;
	Action *a;
	for(t=w->text;t;t=t->next){
		a=t->user;
		if(a){
			if(a->imagebits)
				setbitmap(t);
			if(a->field)
				mkfieldpanel(t);
			a->field=0;
		}
	}
	w->yoffs=plgetpostextview(text);
	plinittextview(text, PACKE|EXPAND, Pt(0, 0), w->text, dolink);
	plsetpostextview(text, w->yoffs);
	pldraw(root, screen);
}
Cursor confirmcursor={
	0, 0,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

	0x00, 0x0E, 0x07, 0x1F, 0x03, 0x17, 0x73, 0x6F,
	0xFB, 0xCE, 0xDB, 0x8C, 0xDB, 0xC0, 0xFB, 0x6C,
	0x77, 0xFC, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03,
	0x94, 0xA6, 0x63, 0x3C, 0x63, 0x18, 0x94, 0x90,
};
int confirm(int b){
	Mouse down, up;
	esetcursor(&confirmcursor);
	do down=emouse(); while(!down.buttons);
	do up=emouse(); while(up.buttons);
	donecurs();
	return down.buttons==(1<<(b-1));
}
void snarf(Panel *p){
	int fd;
	fd=create("/dev/snarf", OWRITE, 0666);
	if(fd>=0){
		fprint(fd, "%s", selection->fullname);
		close(fd);
	}
}
void paste(Panel *p){
	char buf[1024];
	int n, len, fd;
	fd=open("/dev/snarf", OREAD);
	strcpy(buf, plentryval(p));
	len=strlen(buf);
	n=read(fd, buf+len, 1023-len);
	if(n>0){
		buf[len+n]='\0';
		plinitentry(cmd, PACKE|EXPAND, 0, buf, docmd);
		pldraw(cmd, screen);
	}
	close(fd);
}
void hit3(int button, int item){
	char name[100], *home;
	Panel *swap;
	int fd;
	USED(button);
	message("");
	switch(item){
	case 0:
		swap=root;
		root=alt;
		alt=swap;
		current->yoffs=plgetpostextview(text);
		swap=text;
		text=alttext;
		alttext=swap;
		defdisplay=!defdisplay;
		plpack(root, screen->r);
		plinittextview(text, PACKE|EXPAND, Pt(0, 0), current->text, dolink);
		plsetpostextview(text, current->yoffs);
		pldraw(root, screen);
		break;
	case 1:
		snarf(cmd);
		break;
	case 2:
		paste(cmd);
		break;
	case 3:
		inlinepix=!inlinepix;
		break;
	case 4:
#ifdef notyet
		if(screen->depth==8) wrcolmap(screen, map);
#endif
		break;
	case 5:
		home=getenv("home");
		if(home==0){
			message("no $home");
			return;
		}
		sprint(name, "%s/lib/mothra/hit.html", home);
		fd=open(name, OWRITE);
		if(fd==-1){
			fd=create(name, OWRITE, 0666);
			if(fd==-1){
				message("can't open %s", name);
				return;
			}
			fprint(fd, "<head><title>Hit List</title></head>\n");
			fprint(fd, "<body><h1>Hit list</h1>\n");
		}
		seek(fd, 0, 2);
		fprint(fd, "<p><a href=\"%s\">%s</a>\n",
			selection->fullname, selection->fullname);
		close(fd);
		break;
	case 6:
		home=getenv("home");
		if(home==0){
			message("no $home");
			return;
		}
		sprint(name, "file:%s/lib/mothra/hit.html", home);
		geturl(name, GET, 0, 1, 0);
		break;
	case 7:
		if(confirm(3)){
			draw(screen, screen->r, display->white, 0, ZP);
			exits(0);
		}
		break;
	}
}
