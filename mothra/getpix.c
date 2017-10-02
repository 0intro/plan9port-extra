#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <event.h>
#include <panel.h>
#include "mothra.h"
#define	NPIX	500
typedef struct Pix Pix;
struct Pix{
	char name[NNAME];
	Image *b;
};
Pix pix[NPIX];
Pix *epix=pix;
char *pixcmd[]={
[GIF]	"gif -9v",
[JPEG]	"jpg -9v",
[PNG]	"png -9v",
[PIC]	"fb/3to1 /lib/fb/cmap/rgbv",
[TIFF]	"/sys/lib/mothra/tiffcvt",
[XBM]	"fb/xbm2pic",
};
void storebitmap(Rtext *t, Image *b){
	Action *a;
	a=t->user;
	t->b=b;
	free(t->text);
	t->text=0;
	free(a->image);
	if(a->link==0){
		free(a);
		t->user=0;
	}
}
void getimage(Rtext *t, Www *w){
	int f;
	int pfd[2];
	char fdname[30];
	char *bits;
	int nx, ny, y;
	int nb;
	Action *ap;
	Url url;
	Memimage *b;
	int fd;
	char err[512];
	Pix *p;
	ap=t->user;
	crackurl(&url, ap->image, w->base);
	free(ap->image);
	ap->image=strdup(url.fullname);
	for(p=pix;p!=epix;p++)
		if(strcmp(url.fullname, p->name)==0){
			storebitmap(t, p->b);
			w->changed=1;
			return;
		}
	if(!inlinepix && !ap->ismap) return;
	fd=urlopen(&url, GET, 0);
	if(fd==-1){
	Err:
		sprint(err, "[%s: %r]", url.fullname);
		close(fd);
		free(t->text);
		t->text=strdup(err);
		free(ap->image);
		if(ap->link==0){
			free(ap);
			t->user=0;
		}
		return;
	}
	if(url.type!=GIF
	&& url.type!=JPEG
	&& url.type!=PNG
	&& url.type!=PIC
	&& url.type!=TIFF
	&& url.type!=XBM){
		werrstr("unknown image type");
		goto Err;
	}
	if(pipe(pfd)==-1){
		werrstr("can't make pipe");
		goto Err;
	}
	switch(fork()){
	case -1:
		close(pfd[0]);
		close(pfd[1]);
		break;
	case 0:
		dup(fd, 0);
		dup(pfd[0], 1);
		close(pfd[0]);
		close(pfd[1]);
		execl("/bin/rc", "rc", "-c", pixcmd[url.type], 0);
		_exits(0);
	default:
		close(pfd[0]);
		sprint(fdname, "/fd/%d", pfd[1]);
		f=open(fdname, OREAD);
		close(pfd[1]);
		if(f==0){
			werrstr("error in picopen_r");
			goto Err;
		}
		b=readmemimage(f);
		if(b==0){
			werrstr("can't read image");
			close(f);
			goto Err;
		}
		nx=Dx(b->r);
		ny=Dy(b->r);
		nb=bytesperline(b->r, b->depth)*ny;
		bits=emalloc(nb);
		if(bits==0){
			freememimage(b);
			close(f);
			werrstr("no space for image");
			goto Err;
		}
		unloadmemimage(b, b->r, (uchar *)bits, nb);
		if(b->depth == 16){
			/* get rid of alpha channel from transparent gif */
			for(y=1; y<nb; y+=2)
				bits[y>>1] = bits[y];
		}
		freememimage(b);
		close(f);
		ap->r=Rect(0, 0, nx, ny);
		ap->imagebits=bits;
		close(fd);
		w->changed=1;
		break;
	}
}
void setbitmap(Rtext *t){
	Action *a;
	Image *b;
	a=t->user;
	b=floyd(a->r, screen->depth, (uchar *)a->imagebits);
	if(b==0) message("can't balloc!");
	else{
		free(a->imagebits);
		a->imagebits=0;
		if(epix!=&pix[NPIX]){
			strcpy(epix->name, a->image);
			epix->b=b;
			epix++;
		}
		storebitmap(t, b);
	}
}
void getpix(Rtext *t, Www *w){
	Action *ap;
	for(;t!=0;t=t->next){
		ap=t->user;
		if(ap && ap->image)
			getimage(t, w);
	}
}
