#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <pq.h>
#include <pqev.h>

INDEX_INFO*	i_f;
PROTO_INFO*	arry[NIX];
EV_INFO	db;
Biobuf*	db_ptr;

char*	create_I(int, int);
char*	o_file(int, int);
char*	ch_bin(int, char*, char*);
char*	mk_lev(int, int, char*);
char*	mk_treenode(char*, char*, int, int, int, int);
char*	strip_key(char*, int, int);
void	get_int(int, unsigned char*);

void
main(int argc, char **argv)
{
	int i = 0, j = 0, flds, top, num;
	char ev_t[256];

	USED(argc);
	*ev_err_msg = 0;
	ev_err_no = 0;
	if ((i_f = malloc(sizeof(INDEX_INFO))) == nil) {
		pq_error(SYSERR, "pqgen: ");
		print("%s\n", pq_errmsg);
		exits(pq_errmsg);
	}
	if (ev_paths(argv+1, &db) == -1) {
		print("pqgen: %s\n", ev_err_msg);
		exits(pq_errmsg);
	}
	if (ev_ld(db.p_nm, arry) == -1) {
		pq_error(ev_err_no, "pqgen: %s", db.p_nm);
		print("%s\n", pq_errmsg);
		exits(pq_errmsg);
	}
	if ((db_ptr = Bopen(db.db_nm, OREAD)) == nil) {
		pq_error(SYSERR,"%s", db.db_nm);
		print("pqgen: %s\n", pq_errmsg);
		exits(pq_errmsg);
	}
	for (flds = number_fields, i_f->num_index = 0; i < flds; i++)
		if (arry[i]->p_make == 1)
			i_f->key[j++] = i;
	strcpy(ev_t, ev_file(db.path_name, "ev_temp", -1));
	i_f->num_index = j;
	for (top = 0; top < i_f->num_index; top += num) {
		Bseek(db_ptr, 0, 0);
		num = (i_f->num_index-top < NIX) ? i_f->num_index-top : NIX-1;
		if (create_I(top, num) == nil) {
			print("create_I: %s\n", pq_errmsg);
			exits(pq_errmsg);
		}
	}
	for (i = 0; i < i_f->num_index; i++)  {
		if (fork() == 0) {
			char *av[64];
			int ac = 0;
			av[ac++] = "/bin/sort";
//			av[ac++] = "-y";
			if (arry[i_f->key[i]]->p_match->numeric)
				av[ac++] = "-n";
			av[ac++] = "-o";
			av[ac++] = ev_t;
			av[ac++] = ev_file(db.path_name, arry[i_f->key[i]]->p_type, 0);
			av[ac] = nil;
			exec(av[0], av);
			exits("exec failed");
		}
		waitpid();
		if (ch_bin(arry[i_f->key[i]]->i_size, ev_t,
			ev_file(db.path_name, arry[i_f->key[i]]->p_type, 0)) == nil) {
			print("ch_bin: %s\n", pq_errmsg);
			exits(pq_errmsg);
		}
		if (mk_lev(arry[i_f->key[i]]->i_size,
			arry[i_f->key[i]]->i2_size,
			arry[i_f->key[i]]->p_type) == nil) {
			print("mk_lev: %s\n", pq_errmsg);
			exits(pq_errmsg);
		}
	}
	free(i_f); 
	for (i = 0; i < flds; i++)
		free(arry[i]);
	if (i_f->num_index != 0)
		if (remove(ev_t) == -1)
			print("remove %s: %r\n", ev_t);
	Bterm(db_ptr);
	exits(nil);
}

/*
 * Creates the first level index files.
 */
char *
create_I(int begin, int max)
{
	int j, offset=1, len, s, xtoken;
	char *in, out[PQ_REC], *arr[PQ_VEC];
	static char t[PQ_REC];

	if (o_file(begin, max) == nil)
		return nil;
	for (*t = 0; in = Brdline(db_ptr, '\n'); offset += len) {
		if ((len = ev_len(in, 0)+1) >= PQ_REC)  
			print("warning: Data file line too long\n");
		if (ev_addr(in, db.sep, arr) != number_fields) {
			pq_error(0, "%c: bad delimiter", db.sep);
			print("pqgen: %s or corrupt data file\n", pq_errmsg);
			return(nil);
		}
		for (j = begin; j < begin+max; j++) {
			s = arry[i_f->key[j]]->i_size;
			xtoken = arry[i_f->key[j]]->p_match->xtoken;
			if (arry[i_f->key[j]]->p_match->token || xtoken) {
				char *c = arr[arry[i_f->key[j]]->p_field];
				char *p;
				/*
				 * have to do this once before entering the
				 * while loop, in case the value is null
				 */
				strcpy(t, c);
				if (xtoken)
					if ((p = strpbrk(t, TOKEN_SEPS)) != nil)
						*p = 0;
				sprint(out,"%-*.*s%*d\n",s,s,
					ev_modify(i_f->key[j], t,arry), OFF_MAX, offset);
				Bwrite(i_f->f_ptr[j], out, strlen(out));
				c = strpbrk(c, TOKEN_SEPS);
				if (c != nil)
					while (*c && !isalnum(*c))
						c++;
				/*
				 * create additional index entries for this
				 * value, beginning at the first alphanumeric
				 * character after each token separator
				 */
				while (c && *c) {
					strcpy(t, c);
					if (xtoken)
						if ((p = strpbrk(t, TOKEN_SEPS)) != nil)
							*p = 0;
					sprint(out,"%-*.*s%*d\n", s, s,
						ev_modify(i_f->key[j], t,arry), OFF_MAX, offset);
					Bwrite(i_f->f_ptr[j], out, strlen(out));
					c = strpbrk(c, TOKEN_SEPS);
					if (c != nil)
						while (*c && !isalnum(*c))
							c++;
				}
			}
			else {
				strcpy(t, arr[arry[i_f->key[j]]->p_field]);
				s = arry[i_f->key[j]]->i_size;
				sprint(out, "%-*.*s%*d\n", s, s,
					ev_modify(i_f->key[j], t, arry), OFF_MAX, offset);
				Bwrite(i_f->f_ptr[j], out, strlen(out));
			}
		}
	}
	for (j = begin; j < begin+max; j++)
		Bterm(i_f->f_ptr[j]);
	return("");
}
	
/*
 * Opens first level index files
 */
char *
o_file(int begin, int max)
{
	int i;

	for (i = begin; i < begin + max; i++) 
		if ((i_f->f_ptr[i] = Bopen(ev_file(db.path_name, arry[i_f->key[i]]->
				p_type, 0), OWRITE)) == nil) {
			pq_error(SYSERR,"o_file: %s",ev_file(db.path_name, arry[
						 i_f->key[i]]->p_type,0));
			print("pqgen: %s\n",pq_errmsg);
			return(nil);
		}
	return("");
}

/*
 * Converts first level index file offsets into four byte binary and
 * removes newlines
 */
char *
ch_bin(int key_siz, char *in_file, char *out_file)
{
	Dir *d;
	vlong length;
	Biobuf *input,*output;
	char bin_num[OFF_LEN], buf2[OFF_MAX+1], buf1[PQ_REC];
	long off_addr = 0;

	if ((input = Bopen(in_file,OREAD)) == nil)  {
		pq_error(SYSERR,"ch_bin: %s", in_file);
		print("pqgen: %s\n", pq_errmsg);
		return nil;
	}
	if ((output = Bopen(out_file,OWRITE)) == nil) {
		pq_error(SYSERR, "ch_bin: %s", out_file);
		print("pqgen: %s\n", pq_errmsg);
		return nil;
	}
	d = dirfstat(BFILDES(input));
	length = d->length;
	free(d);
	buf2[OFF_MAX] = 0;
	for (; off_addr * (OFF_MAX+key_siz+1) < length; off_addr++) {
		if ((Bread(input,buf1,key_siz) != key_siz)
		  || (Bread(input,buf2,OFF_MAX) != OFF_MAX)){
			pq_error(SYSERR, "ch_bin read: ");
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
		get_int(atoi(buf2),(unsigned char*)bin_num);
		if ((Bwrite(output,buf1,key_siz) != key_siz)
		  || (Bwrite(output,bin_num,OFF_LEN) != OFF_LEN)){
			pq_error(SYSERR, "ch_bin write: ");
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
		if (BGETC(input) == Beof) {
			pq_error(0, "ch_bin: end of file");
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
	}
	Bterm(input);
	Bterm(output);
	return "";
}

/* 
 * Sets up the creation of secondary level index files.
 */
char *
mk_lev(int key1, int key2, char *base)
{
	int file_no = 0, e_siz, is_root = 0;
	char file_1[256], file_2[256];
	char buf[INDX_BLK];
	Biobuf *b1, *b2;
	int n;

	strcpy(file_1, ev_file(db.path_name, base, file_no));
	if (ev_root(file_1, 0) == 1) {
		b1 = Bopen(file_1, OREAD);
		if (b1 == nil) {
			pq_error(SYSERR, "mk_lev: open %s", file_1);
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
		strcpy(file_2, ev_file(db.path_name, base, -1));
		b2 = Bopen(file_2, OWRITE);
		if (b2 == nil) {
			pq_error(SYSERR, "mk_lev: open %s", file_2);
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
		while ((n = Bread(b1, buf, sizeof buf)) > 0)
			if (Bwrite(b2, buf, n) != n) {
				pq_error(SYSERR, "mk_lev: write %s", file_2);
				print("pqgen: %s\n", pq_errmsg);
				return nil;
			}
		if (n < 0) {
			pq_error(SYSERR, "mk_lev: read %s", file_1);
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
		if (BPUTC(b2, '1') == Beof) {
			pq_error(SYSERR, "mk_lev: writec %s", file_2);
			print("pqgen: %s\n", pq_errmsg);
			return(nil);
		}
		Bterm(b1);
		Bterm(b2);
		if (remove(file_1) < 0) {
			pq_error(SYSERR, "mk_lev: remove %s", file_1);
			print("pqgen: %s\n", pq_errmsg);
			return nil;
		}
		return "";
	}
	for (e_siz = key1 + 4; is_root == 0; file_no++, e_siz=key2) {
		if (ev_root(file_1, key2) == 1) {
			strcpy(file_2, ev_file(db.path_name, base, -1)); 
			is_root = 1;
		} else
			strcpy(file_2, ev_file(db.path_name, base, file_no + 1));
		if (mk_treenode(file_1, file_2, e_siz, key2, is_root, file_no + 2) == nil) 
			return nil;
		strcpy(file_1, file_2);
	}	
	return "";
}

/* 
 * Called by MK_NODEFILE to create secondary index files from previously
 * formed index files.
 */
char *
mk_treenode(char *in_file, char *out_file,
	int in_len, int out_len, int add_to_root, int num_lev)
{
	Biobuf *f_input,*f_output;
	int num_entries = 1;
	char info_buf[PQ_REC];
	char line[PQ_REC];
	Dir *d;
	vlong length;
	int majorhack = (num_lev==2) ? 4:0;

	if ((f_input=Bopen(in_file,OREAD))==nil) {
		pq_error(SYSERR,"mk_treenode: %s",in_file);
		print("pqgen: %s\n",pq_errmsg);
		return(nil);
	}
	if ((f_output=Bopen(out_file,OWRITE))==nil) {
		pq_error(SYSERR,"mk_treenode: %s",out_file);
		print("pqgen: %s\n",pq_errmsg);
		return(nil);
	}
	d = dirfstat(BFILDES(f_input));
	length = d->length;
	free(d);
	for (;(num_entries*INDX_BLK)<length;num_entries++) {
		if (Bseek(f_input,((INDX_BLK*num_entries)-in_len),0)==Beof) {
			pq_error(SYSERR,"mk_treenode:");
			print("pqgen: %s\n",pq_errmsg);
			return(nil);
		}
		if (Bread(f_input,line,in_len-majorhack)==Beof) {
			pq_error(SYSERR,"mk_treenode:");
			print("pqgen: %s\n",pq_errmsg);
			return(nil);
		}
		line[in_len-majorhack] = 0;
		sprint(info_buf,"%-*.*s",out_len,out_len,line);
		if (Bwrite(f_output,info_buf,strlen(info_buf))==Beof) {
			pq_error(SYSERR,"mk_treenode:");
			print("pqgen: %s\n",pq_errmsg);
			return(nil);
		}
	}
	if (Bseek(f_input,length-in_len,0)==Beof){
		print("pqgen1: %lld %d\n", length, in_len);
		return 0;
	}
	if(Bread(f_input,line,in_len-majorhack)==Beof){
		print("in_file=%s out_file=%s, length=%lld in_len=%d\n",
			in_file, out_file, length, in_len);
		return 0;
	}
	line[in_len-majorhack]=0;
	if(Bwrite(f_output,strip_key(line,out_len,in_len-majorhack),out_len)!=out_len) {
		pq_error(SYSERR,"mk_treenode:");
		print("pqgen: %s\n",pq_errmsg);
		return(nil);
	}
	if (add_to_root==1)  {
		sprint(info_buf,"%d",num_lev);
		if (Bwrite(f_output,info_buf,strlen(info_buf))<=0) {
			pq_error(SYSERR,"mk_treenode:");
			print("pqgen: %s\n",pq_errmsg);
			return(nil);
		}
	}
	Bterm(f_input);
	Bterm(f_output);
	return("");
}

/*
 * Restricts the length of the key put into the index files.
 */
char *
strip_key(char *orig_str, int key_siz, int orig_siz)
{
	if (orig_siz>key_siz)
		orig_str[key_siz] = 0;
	else{
		char foo[128];
		strcpy(foo,orig_str);
		sprint(orig_str,"%-*.*s",key_siz,key_siz,foo);
	}
	return(orig_str);
}

/*
 * Converts integers into 4 byte binary characters
 */
void
get_int(int i_val, unsigned char *c_val)
{
        int n = OFF_LEN;

        c_val += OFF_LEN;
        while (n--) {
                *--c_val = i_val;
                i_val >>= NB;
        }
}
