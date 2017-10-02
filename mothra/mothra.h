#define	NWWW	200	/* # of pages we can get before failure */
#define	NNAME	512
#define NAUTH	128
#define	NTITLE	81	/* length of title (including nul at end) */
#define	NREDIR	10	/* # of redirections we'll tolerate before declaring a loop */
typedef struct Action Action;
typedef struct Url Url;
typedef struct Www Www;
typedef struct Scheme Scheme;
typedef struct Field Field;
struct Scheme{
	char *name;
	int type;
	int flags;
	int port;
};
struct Action{
	char *image;
	Rectangle r;
	char *imagebits;
	Field *field;
	char *link;
	char *name;
	int ismap;
};
struct Url{
	char fullname[NNAME];
	Scheme *scheme;
	char ipaddr[NNAME];
	char reltext[NNAME];
	char tag[NNAME];
	char redirname[NNAME];
	char autharg[NAUTH];
	char authtype[NTITLE];
	int port;
	int access;
	int type;
	int map;			/* is this an image map? */
};
struct Www{
	Url *url;
	Url *base;
	char title[NTITLE];
	Rtext *text;
	int yoffs;
	int changed;		/* reader sets this every time it updates page */
	int finished;		/* reader sets this when done */
	int alldone;		/* page will not change further -- used to adjust cursor */
	int index;
};
/*
 * url reference types -- COMPRESS, GUNZIP and BZ2 are flags that can modify any other type
 * Changing these in a non-downward compatible way spoils cache entries
 */
#define	GIF		1
#define	HTML		2
#define	JPEG		3
#define	PIC		4
#define	TIFF		5
#define	AUDIO		6
#define	PLAIN		7
#define	XBM		8
#define	POSTSCRIPT	9
#define	FORWARD		10
#define	PDF		11
#define	SUFFIX		12
#define ZIP		13	/* rm */
#define PNG		14
#define	COMPRESS	16
#define	GUNZIP		32
#define	BZIP2		64
#define	COMPRESSION	(COMPRESS|GUNZIP|BZIP2)
/*
 * url access types
 */
#define	HTTP		1
#define	FTP		2
#define	FILE		3
#define	TELNET		4
#define	MAILTO		5
#define	EXEC		6
#define	GOPHER		7
/*
 *  authentication types
 */
#define ANONE 0
#define ABASIC 1

Image *hrule, *bullet, *linespace;
char home[512];		/* where to put files */
int chrwidth;		/* nominal width of characters in font */
Panel *text;		/* Panel displaying the current www page */
int debug;		/* command line flag */
int inlinepix;		/* flag set if you want to fetch inline images */
/*
 * HTTP methods
 */
#define	GET	1
#define	POST	2
void plrdhtml(char *, int, Www *);
void plrdplain(char *, int, Www *);
void htmlerror(char *, int, char *, ...);	/* user-supplied routine */
void crackurl(Url *, char *, Url *);
void getpix(Rtext *, Www *);
int urlopen(Url *, int, char *);
void getfonts(void);
void *emalloc(int);
void setbitmap(Rtext *);
void message(char *, ...);
int ftp(Url *);
int http(Url *, int, char *);
int gopher(Url *);
int cistrcmp(char *, char *);
int cistrncmp(char *, char *, int);
int suffix2type(char *);
int content2type(char *, char *);
int encoding2type(char *);
void mkfieldpanel(Rtext *);
void geturl(char *, int, char *, int, int);
int dir2html(char *, int);
Image *floyd(Rectangle, int, uchar *);
int auth(Url*, char*, int);
uchar cmap[256*3];
RGB map[256];
char version[];
#ifndef brazil
#	define	RFREND	0
#endif
#ifndef	nil
#define	nil	0
#endif
