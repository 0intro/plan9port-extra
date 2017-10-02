#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <panel.h>
#include "mothra.h"
#define	NBIT	8		/* bits per uchar, a source of non-portability */
/*
 * Copy an 8 bit image into a newly allocated bitmap, using Floyd-Steinberg
 * if depth<8
 */
Image *floyd(Rectangle r, int depth, uchar *pixels){
	int wid=r.max.x-r.min.x;
	int hgt=r.max.y-r.min.y;
	int w=depth;
	int nval=1<<w;
	uchar lummap[1<<NBIT];
	uchar *data, *dp, *p;
	int *error, *ep, *eerror, cerror, y, value, shift, pv, i;
	Image *b;
	if(depth>8) depth=8;
	b=allocimage(display, r, w<8? CHAN1(CGrey, w) : CHAN1(CMap, 8), 0, DNofill);
	if(b==0) return 0;
	if(depth==8){
		loadimage(b, r, pixels, bytesperline(r, w)*Dy(r));
		return b;
	}
	data=malloc((wid*w+NBIT-1)/NBIT);
	error=malloc((wid+2)*sizeof(int));
	if(error==0 || data==0){
		if(error) free(error);
		if(data) free(data);
		freeimage(b);
		return 0;
	}
	eerror=error+wid+1;
	for(i=0,p=cmap;i!=1<<NBIT;i++,p+=3)
		lummap[i]=(p[0]*299+p[1]*587+p[2]*114)/1000;
	for(ep=error;ep<=eerror;ep++) *ep=0;
	p=pixels;
	for(y=0;y!=hgt;y++){
		cerror=0;
		dp=data-1;
		shift=0;
		for(ep=error+1;ep!=eerror;ep++){
			shift-=w;
			if(shift<0){
				shift+=NBIT;
				*++dp=0;
			}
			value=lummap[p[0]&255];
			value+=ep[0]/16;
			pv=value<=0?0:value<255?value*nval/256:nval-1;
			p++;
			*dp|=(nval-1-pv)<<shift;
			value-=pv*255/(nval-1);
			ep[1]+=value*7;
			ep[-1]+=value*3;
			ep[0]=cerror+value*5;
			cerror=value;
		}
		loadimage(b, Rect(r.min.x, y, r.max.x, y+1), data, wid);
	}
	free(error);
	free(data);
	return b;
}
