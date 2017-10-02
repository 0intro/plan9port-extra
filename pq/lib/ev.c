#include "all.h"
#include <pqev.h>

#define min(x, y)	((int)(x) < (int)(y) ? (x) : (y))

static EV_INFO*	db_in;

static char*	pre_process(char**);
static int		smallest(int, int);
static char*	add_qual(int, char*, int);
static int		dup_type(int, char*);
static int		find_dup(int);
static char*	r_file(char*, int);
static char*	ev_select(void);
static int		search(char*, int, int);
static int		db_increm(char**);
static int		check_qual(char*, char**);
static int		is_match(char*, char*, FLAGS*, int, int);
static int		exact(char*, char*);
static int		exact_a(char*, char*);
static int		exact_i(char*, char*);
static int		exact_ai(char*, char*);
static int		prefix_a(char*, char*);
static int		prefix_i(char*, char*);
static int		prefix_ai(char*, char*);
static int		ev_word_i(char*, char*);
static int		token(char*, char*, FLAGS*);
static char*	rd_db(char*);
static int		get_char(unsigned char*);
static void		copy_key(EV_INFO*, int);

/*
 * RETURNS:  
 *	open_db - a pointer to an EV_INFO structure that is initialized
 * 	nil - an error occurred. 
 *
 *
 * Operations on a Database are initialized by calling EV_OPEN.  Argument(s)
 * specify the data and proto file, location of any index files and the
 * database delimiter. A structure containing initialized strings, flags,
 * structures, memory and file pointers is returned.  All memory allocation 
 * is done in EV_OPEN.  An error is returned if there is a problem with 
 * memory allocation, reading in the proto file, or openning the data file.
 */
void * 
ev_open(char **directory)
{
	EV_INFO *open_db;
	IN_INFO *t_blk;
	int i;

	ev_err_no = 0;
	*ev_err = 0;

	if ((open_db = malloc(sizeof(EV_INFO))) == nil) {
		pq_error(0, "ev: open: No memory");
		return(nil);
	}

	/* process arguments passed to ev: assign pathnames to Data, proto
	 * and index files & assign the separator
	 */ 	
	if (ev_paths(directory, open_db) == -1) {
		pq_error(ev_err_no, "ev: %s", ev_err_msg);
		return nil;
	}

	/* process the proto file:
	 * load the proto file into the proto data structure 
  	 */
	if((open_db->x_fld = ev_ld(open_db->p_nm, open_db->pptr))==-1){
		free(open_db);
		pq_error(ev_err_no, "ev: open: %s", open_db->p_nm); 
		return nil;
	}
	open_db->num_flds = number_fields; /* in the proto file records*/
	open_db->valid_extra = 1;		 
	
	/* see if the is an m= field in the proto file */
	if (open_db->x_fld == -2) 
		open_db->valid_extra = 0;
	else
		strcpy(open_db->x_vector, x_val);
       
	/* open the Data file */
	if ((open_db->db_ptr = open(open_db->db_nm,OREAD)) == -1) {
		ev_err_no = SYSERR;
		pq_error(ev_err_no,"ev: open: %s",open_db->db_nm);
		for (i = 0; i < open_db->num_flds; i++) {
			free(open_db->pptr[i]->p_match);
			free(open_db->pptr[i]);
		}
		free(open_db);
		return nil;
	}
	if ((open_db->in_blk = malloc(PQ_VEC * sizeof(IN_INFO))) == nil) {
		pq_error(0,"ev: open: No memory");
		for (i = 0; i < open_db->num_flds; i++) {
			free(open_db->pptr[i]->p_match);
			free(open_db->pptr[i]);
		}
		free(open_db);
		return nil;
	}

	for (t_blk = open_db->in_blk, i = 0; i < PQ_VEC; open_db->in_info[i] = t_blk++, i++)
		;
	return open_db; 
}

/* FUNCTION: ev_close(close_db)
 * RETURNS: 0 - always 
 *
 * EV_CLOSE frees all the structures, pointers and memory and closes the
 * database file.
 */
int
ev_close(void *vclose_db)
{
	EV_INFO *close_db = vclose_db;
	int i;

	if (close_db) {
		close(close_db->db_ptr);
		if(close_db->lockfd != -1)
			close(close_db->lockfd);
		for (i=0;i<close_db->num_flds;i++) {
			free(close_db->pptr[i]->p_match);
			free(close_db->pptr[i]);
		}
		free(close_db->in_blk);
		free(close_db);
	}
	return(0);
}

/* FUNCTION: ev_write(write_db, input_str)
 * RETURNS:  0 - on success
 *	    -1 - on failure 
 *
 * EV_WRITE preprocesses an input array (one qualifier per array element) and
 * invokes ev_select.  An EV_WRITE does not return any entries from the
 * database,
 * but sets up the index file pointer for a EV_READ.  An error is returned if 
 * the database has not been initialized, for invalid input vectors.  A sample
 * valid input vector would be: "last=smith|loc=wh|room=3A-351".
 */
int
ev_write(void *vwrite_db, char **input_str)
{
	EV_INFO *write_db = vwrite_db;

	db_in = write_db;
	db_in->buf_ptr = db_in->read_buf;
	db_in->free_ptr = db_in->free_blk;
	db_in->maj_loc = -1; 
	db_in->maj_val = nil;
	db_in->db_offset = 0;  
	db_in->no_index = 1; 
	db_in->attribute = 0;
	db_in->add_vector = 1;
	*ev_err = 0;
	ev_err_no = 0;
	db_in->in_num = 0; 

	if (pre_process(input_str) == nil) {
		db_in->maj_loc = -1;
		pq_error(ev_err_no, "ev: write: %s", ev_err);
		return -1;
	}

	/* if the input stream has a major key - one that has an index file(s)
	 * associated with it
	 */
	if (db_in->maj_loc != -1)
		copy_key(db_in, 0);

	/* find the location of this key value in the database */
	if (ev_select() == nil) {
		db_in->maj_loc = -1;
		pq_error(ev_err_no, "ev: write: %s", ev_err);
		return -1;
	}
	return 0;
}

/*
 * RETURNS: nil - on failure
 *	    ""   - on success
 *
 * Receives the input vector and determines/assigns the major key type
 * (db_in->maj_loc) to be the attribute type and value which would most 
 * likely to quickly return the least number of entries. Attribute types which
 * are specified more than once are rearranged to performed an OR operations.
 * The attributes and their values are stored in an array of IN_INFO 
 * structures(db_in->in_info) - each array element containing one qualifer.
 * Attributes are check for validity.
 */
static char *
pre_process(char **input_str)
{
	static char val[PQ_REC];
	char type[ATTR_NAME_SZ+1];
	int indx = 0, i, maj, t_loc;

	for(maj = -1,*val=0; *input_str != nil && indx < PQ_VEC;
	    input_str++, *val = 0, indx++, db_in->in_num++) {

		ev_cpy(type, *input_str, ev_len(*input_str, '=')); 
				/* type holds a */
				/* single attribute without a value */

		if (strcmp(type, ATTRIBUTE) == 0) {
			db_in->attribute = 1;
			break;
		}

		/* t_loc is the position in the profile array where this
		 * this attribute type is found
		 */ 
		if ((t_loc = ev_proto(type, db_in->pptr, db_in->num_flds)) < 0) {
			sprint(ev_err_msg, "%s: Invalid attribute", type);
			return ev_error_proc(ev_err_msg);
		}

		/* if the attribute has a value, copy it into val 
		 * (include the delimiter, also)
		 */
		if ((*input_str)[strlen(type)] !=0 ) {
			strcpy(val, *input_str + strlen(type));
		}

		if (add_qual(t_loc, val, indx) == nil) {
		/* then exceeded max num of allowed values */
			return nil;
		}
	
		/* determine which attribute will be used as the primary
		 * search key
		 */
		if (db_in->in_info[indx]->equal && smallest(t_loc, maj) > 0)
			maj = t_loc;

		/* If this is an attribue that can have multiple values, 
		 * clear the add_vector 
		 */
		if (db_in->add_vector == 1 && *db_in->pptr[
		     db_in->in_info[indx]->proto_no]->multiple == 'm')
			db_in->add_vector = 0;
	}
	if (indx >= PQ_VEC) {
		sprint(ev_err_msg, "pre_process: exceeded %d attributes", PQ_VEC);
		return ev_error_proc(ev_err_msg);
	}

	/* assign the key info to the db struct*/
	if (maj == -1 || db_in->attribute == 1)
		db_in->maj_loc = -1;	/* there were no qualifiers or want the attribute function*/
	else {
		/* there was a qualifier */
		db_in->maj_loc = maj;
		if ((i = find_dup(maj)) == -1) {
			sprint(ev_err_msg, "key not found");
			return ev_error_proc(ev_err_msg);
		}
 		db_in->maj_val = db_in->in_info[i];
		db_in->maj_no = 1;
		db_in->mf = db_in->pptr[db_in->maj_loc]->p_match;
		db_in->pre = db_in->in_info[i]->pre[0];
	}
	
	/* process the multi-value key */
	if (db_in->attribute == 0 && db_in->add_vector == 1
	&& db_in->valid_extra == 1) {
		db_in->in_info[indx]->x_vector = 1;
		db_in->in_info[indx]->proto_no = db_in->x_fld;
		db_in->in_info[indx]->num_val = 1;
		db_in->in_info[indx]->equal = 1;
		db_in->in_info[indx]->val[0] = db_in->x_vector;
		db_in->in_num++;
	}
	return "";
}

/* FUNCTION: smallest(type1, type2)
 * RETURNS: 1 - if type1 has a higher priority (its more major) than
 *		type2
 *	   -1 - on failure, or if type1 is not as major as type2
 *
 * Determines the major key.  The proto file key name order determines 
 * which key is the major key. Key type with index files are prefered.
 */
static int 
smallest(int type1, int type2)
{
	if (db_in->pptr[type1]->p_make != 1)
		return -1; 
	if (type2 == -1 || type1 < type2)
		return 1;
	return -1;
}

/*
 * RETURNS: "" - on normal functioning
 *	    nil - if exceeded the maximum allowed values for a type
 * Used by PRE_PROCESS to assign memory for the in_info structure. MODIFY_KEY
 * removes unallowable characters and/or changes case.
 */
static char *
add_qual(int t_loc, char *val, int indx)
{
	int retval;

	db_in->in_info[indx]->proto_no = t_loc;
	db_in->in_info[indx]->x_vector = 0;

	if ((retval = dup_type(t_loc, val)) == -1) {
	 	/* exceeded max allowed values for this type */
		return nil;
	} else if (retval == 1) {
		/* no value was assigned, or a dup was found */
		db_in->in_info[indx]->equal = 0;
		db_in->in_info[indx]->val[0] = 0;
		db_in->in_info[indx]->num_val = 0;
	} else  {
		/* there is a value and no dup was found */
		db_in->in_info[indx]->equal = 1;
		db_in->in_info[indx]->pre[0] =
			(db_in->pptr[t_loc]->p_match->star && val[strlen(val)-1] == PREFIX);
		strcpy(db_in->free_ptr, ev_modify(t_loc, val+1, db_in->pptr));
		db_in->in_info[indx]->val[0] = db_in->free_ptr;
		db_in->in_info[indx]->num_val = 1;
		db_in->free_ptr += strlen(db_in->in_info[indx]->val[0]) + 1;
	}
	return "";
}

/* FUNCTION: dup_type(type, value)
 * RETURNS: 0 - no dup was found
 *          1 - either a dup was found or no value was assigned to this entry
 *	   -1 - exceeded the max allowed values for a type
 * Used by PRE_PROCESS to determine whether multiple values are specified
 * for one key type. 
 */
static int
dup_type(int t_loc, char *value)
{
	int i;

	if (*value == 0) /* no value was assigned to this key type */
		return 1;

	if ((i = find_dup(t_loc)) != -1) {
		/* a duplicate was found which has a value assigned */
		if (db_in->in_info[i]->num_val >= NUM_DUP) {
			sprint(ev_err, "dup_type: exceeded %d attr values", NUM_DUP);
			return -1;
		}
		db_in->in_info[i]->pre[db_in->in_info[i]->num_val] =
			(db_in->pptr[t_loc]->p_match->star && value[strlen(value)-1] == PREFIX);
		strcpy(db_in->free_ptr, ev_modify(t_loc, value+1, db_in->pptr));
		db_in->in_info[i]->val[db_in->in_info[i]->num_val]=db_in->free_ptr;
		db_in->in_info[i]->num_val++;
		db_in->free_ptr += strlen(db_in->free_ptr) + 1;
		return(1);
	}
	return 0;
}

/* FUNCTION: find_dup(t_loc)
 * RETURNS: -1 - if no duplicate is found
 *	   index # of dup - if one is found with equal set
 * Determines whether the key type already exists in the in_info structure.
 */ 
static int
find_dup(int t_loc)
{	
	int in_indx;

	/* look through all of the list of attributes already processed
	 * to see if there is a duplicate 
  	 */
	for (in_indx=0; in_indx < db_in->in_num; in_indx++)  
		if (t_loc == db_in->in_info[in_indx]->proto_no)
			if (db_in->in_info[in_indx]->equal)
				return in_indx;
	return -1;
}

/* FUNCTION: r_file(file_name, beginning_loc)
 * RETURNS: nil - on failure
 *	    pointer to the read_buf at beginning_loc in file_name - on success
 * DESCRIPTION:
 * Reads a INDX_BLK block from the file, file_name, beginning at location, 
 * beginning_loc, adds a marker to the buffer (LEVEL_SEP) to indicate the
 * end of the buffer and returns a pointer to the buffer that was read in.
 */
static char *
r_file(char *file_name, int beginning_loc)
{
	Dir *d;
	vlong length;
	int file_ptr, read_size;

	/* if an index is available for this key, or want attribute fcn */
	if (db_in->no_index != 0 || db_in->attribute ==1) {
		/* open either proto or index file */
		if ((file_ptr = open(file_name, OREAD)) == -1)  {
			ev_err_no = SYSERR;
			sprint(ev_err_msg, "r_file: %s", file_name);
			return ev_error_proc(ev_err_msg);
		}
	} else
		file_ptr = db_in->db_ptr;

	d = dirfstat(file_ptr);
	length = d->length;
	free(d);
	if (length < beginning_loc+2)  {
		/* "zero out" the read buffer by using the LEVEL_SEP 
  		 * character which indicates the end of the read buffer
		 */
		*db_in->read_buf = LEVEL_SEP;
		db_in->db_offset = -1;
		if (db_in->no_index != 0 || db_in->attribute == 1) 
			close(file_ptr);
		return db_in->read_buf;
	}
	if (seek(file_ptr, beginning_loc, 0) == -1) {
		ev_err_no = SYSERR;
		sprint(ev_err_msg, "r_file: seek in %s", file_name);
		if (db_in->no_index != 0 || db_in->attribute == 1) 
			close(file_ptr);
		return ev_error_proc(ev_err_msg);
	}
	if ((read_size = read(file_ptr, db_in->read_buf, INDX_BLK)) == -1) {
		ev_err_no = SYSERR;
		sprint(ev_err_msg, "r_file: %s", file_name);
		if (db_in->no_index != 0 || db_in->attribute == 1) 
			close(file_ptr);
		return ev_error_proc(ev_err_msg);
	}
	db_in->index_end = read_size;
	/* the LEVEL_SEP is used to mark the end of the read buffer */
	db_in->read_buf[read_size] = LEVEL_SEP;
	if (db_in->no_index != 0 || db_in->attribute == 1) 
		close(file_ptr);
	return db_in->read_buf;
}

/* FUNCTION: ev_select()
 * RETURNS: nil - if entry is not found
 * Used during EV_READ (and EV_WRITE) to read in INDX_BLKs of the major key's
 * index files and perform a linear search.  If no index files exist for 
 * the major key, the entire database is searched, INDX_BLK blocks at a time.
 */
static char *
ev_select(void)
{
	int indx_off = 0, lvl = 0, ns, add_off = 0, e_len = 0, k_len;
	char *bs = 0, i_nm[256], *dep, *I_ptr = nil;

	/* if there is a major key */
	if (db_in->maj_loc != -1) {
		e_len = db_in->pptr[db_in->maj_loc]->i2_size;
		bs = db_in->pptr[db_in->maj_loc]->p_type;

		/* append the key name to the index file directory to get
		 * the root index file name
		 */
		strcpy(i_nm, ev_file(db_in->path_name, bs, -1));
	}
	
	if (db_in->maj_loc == -1) { /* if there isn't a major key */
		db_in->no_index = 0;
		if (db_in->attribute == 1) {
			/* open proto file and read in first index blk */
			if ((db_in->buf_ptr = r_file(db_in->p_nm, 0)) == nil)
				return ev_error_proc("ev_select: ");
		}
		else {
			/* open the data file and read in the first block;
			 * since there is no key, need to search the entire
			 * database
			 */
			if ((db_in->buf_ptr = r_file(db_in->db_nm, 0)) == nil)
				return ev_error_proc("ev_select: ");
		}
		db_in->maj_loc = -2; 
		return "";
	}

	/* there is an index - get the appropriate index block for this key */
	for (db_in->depth = 1, k_len = e_len; db_in->depth-lvl > -1; lvl++) {
		indx_off = add_off + (indx_off * INDX_BLK) / e_len;
		if ((I_ptr = r_file(i_nm, indx_off * INDX_BLK)) == nil)
			return(ev_error_proc("ev_select: "));

		if (lvl == 0) {
			/* then this is the root index file.
			 * get the number of index files for this key -
			 * it is the last char in the root index file
			 */
			dep = I_ptr + db_in->index_end - 1;
			db_in->depth = atoi(dep) - 1;
			*dep = LEVEL_SEP;
		}
		if (db_in->depth-lvl < 1) { /* then at primary index level*/
			k_len = db_in->pptr[db_in->maj_loc]->i_size;
			e_len = k_len + OFF_LEN;
		}

		/* loop until find the key at this level of index */
		for (ns = 0; (add_off = search(I_ptr, e_len, k_len)) < 0; ns++) {
			if (add_off == -1) {
				*db_in->buf_ptr = LEVEL_SEP;
				db_in->db_offset = -1;
				return "";
			}
			I_ptr = r_file(i_nm, (indx_off+1+ns) * INDX_BLK);
			if (db_in->db_offset == -1)
				return "";
			if (I_ptr == nil)
				return ev_error_proc("ev_select: ");
		}
		/* get the next level index file name */
		strcpy(i_nm, ev_file(db_in->path_name, bs, db_in->depth-lvl-1));
		indx_off += ns;
	}
	db_in->db_offset = indx_off * INDX_BLK + add_off;
	db_in->buf_ptr = I_ptr + add_off;
	return "";
}

/* FUNCTION: search(l_ptr, entry_len, key_len)
 * RETURNS:  -1
 *	     -2 - key value is not in this index block
 *	offset  - where key can be found in the next lower level
 * 
 * Used during SELECT to linearly search the entries in l_ptr until
 * it is determined that the entry does not exist or an offset into the next
 * index file is calculated and returned.
 */
static int 
search(char *l_ptr, int entry_len, int key_len)
{
	int n_blks = 0, cue;
	char *ptr, tst_key[PQ_REC], tst2_key[PQ_REC];

	ev_cpy(tst2_key, db_in->key, min(key_len, strlen(db_in->key)));
	for (ptr = l_ptr; *ptr != LEVEL_SEP; ptr += entry_len, n_blks++) {
		ev_cpy(tst_key, ptr, key_len); 
		cue = is_match(tst2_key, tst_key, db_in->mf, 0, db_in->pre);
		if (key_len != entry_len) {/* then at primary level*/
			if (cue == 0)
				return n_blks * entry_len;
			if (cue < 0)
				return -1;
		}
		else /* looking at a secondary index file */
			if (cue <= 0)
				return n_blks;
	}
	return -2; /* never found the key in this index block */
}

/* FUNCTION: ev_read(read_db,out_array)
 * RETURNS:  
 * Returns the next database entry which satisfies the input vector.  Calls 
 * DB_INCREM to find the next entry matching the major key. For example for
 * major key equal to ext=3518=3509, DB_INCREM returns entries with 
 * ext=3518(one at a time) and then entries with ext=3509.
 */
int
ev_read(void *vread_db, char **out_array)
{
	EV_INFO *read_db = vread_db;
	int rd_end;

	db_in = read_db;
	if (db_in->maj_loc == -1) {
		pq_error(0, "ev: read: No entries");
		return -1;
	}
	if ((rd_end = db_increm(out_array)) == -1)  {
		db_in->maj_loc = -1;
		pq_error(ev_err_no, "ev: read: %s", ev_err);
		return -1;
	}
	if (rd_end == 1)
		return(1);
	if (db_in->no_index == 0 || db_in->attribute == 1) {
		db_in->maj_loc = -1;
		return 0;
	}

	while (db_in->maj_no < db_in->maj_val->num_val) {
		copy_key(db_in, db_in->maj_no);
		db_in->pre = db_in->maj_val->pre[db_in->maj_no];
		db_in->maj_no++;
		if (ev_select() == nil || (rd_end = db_increm(out_array)) == -1) {
			db_in->maj_loc = -1;
			pq_error(ev_err_no, "ev: read: %s", ev_err);
			return -1;
		}
		if (rd_end == 1)
			return 1;
	}
	db_in->maj_loc = -1;
	return 0;
}

/* FUNCTION: db_increm(out_array)
 * RETURNS: 	-1	(failure)
 *		 0	(no match)
 *		 1	(match)
 * Used by EV_READ to find entries matching the input vector.  Offsets
 * into the database are used to read an entry, and determine whether it 
 * satisfies the input vector. If matching, the entry is returned, otherwise,
 * the next offset's entry is read.  
 */
static int 
db_increm(char **out_array)
{
	char *off_out, ds[PQ_REC], fn[256];
	int add = 0, out = 0;

	if (db_in->attribute == 1)
		strcpy(fn, db_in->p_nm);
	else if (db_in->no_index == 0)
		strcpy(fn, db_in->db_nm);
	else {
		if (db_in->depth == 0)
			add = -1;
		strcpy(fn, ev_file(db_in->path_name, db_in->pptr[db_in->
			maj_loc]->p_type, add));
		add = db_in->pptr[db_in->maj_loc]->i_size + OFF_LEN;
	}

	for (; out != 1; db_in->buf_ptr += add, db_in->db_offset += add) {
		if ((*db_in->buf_ptr == LEVEL_SEP && db_in->db_offset > 0)
		|| (db_in->no_index == 0 && strchr(db_in->buf_ptr, '\n') == nil))
	 		if ((db_in->buf_ptr = r_file(fn, db_in->db_offset)) == nil)
				return -1;
		if (db_in->db_offset == -1)
			return 0;
		if (db_in->no_index == 0) {
			ev_cpy(db_in->db_buf, db_in->buf_ptr, ev_len(db_in->buf_ptr, 0));
			strcat(db_in->db_buf, "\n");			
			add = strlen(db_in->db_buf);
			if ((out = check_qual(db_in->db_buf, out_array)) == -1)
				return -1;
		} else {
			off_out = db_in->buf_ptr + add - OFF_LEN;
			ev_cpy(ds, db_in->buf_ptr, add - OFF_LEN);
			if (is_match(db_in->key, ds, db_in->mf, 0, db_in->pre) < 0) 
				return 0;
			if ((out = check_qual(rd_db(off_out), out_array)) == -1)
				return -1;
		}
	}
	return 1;
}

/* FUNCTION: check_qual(hold_db,out_array)
 * RETURNS: 	-1	(failure)
 *		 0	(no match)
 *		 1	(match)
 * Used by DB_INCREM to determine if the database entry, hold_db, satisfies 
 * all the qualifiers in the input vector. (stored in db_in->in_info).
 */
static int 
check_qual(char *hold_db, char **out_array)
{
	char *match, *db_arr[NIX+1];		/* +1 because db_arr[0] is skipped */
	int  no_arr = 0, bloc, i, j, pre_flg, ck_indx;

	if (db_in->attribute == 1) {
		for (match = hold_db; *match != BLANK && *match != '\t'; match++);
		*match = 0;
		out_array[0] = hold_db;
		out_array[1] = 0;
		return 1;
	}
	if (ev_addr(hold_db, db_in->sep, db_arr) != db_in->num_flds) {
		strcpy(ev_err, "illegal delimiter or incorrect Database file");
		return -1;
	}
	for (ck_indx = 0; ck_indx < db_in->in_num; ck_indx++) {
		bloc = db_in->pptr[j=db_in->in_info[ck_indx]->proto_no]->p_field;
		if (db_in->in_info[ck_indx]->equal) {
			match = db_in->in_info[ck_indx]->val[0];
			pre_flg = db_in->in_info[ck_indx]->pre[0];
			for (i = 1; is_match(match, db_arr[bloc], db_in->pptr[j]->p_match, 1, pre_flg) != 0; i++) {
				if (db_in->in_info[ck_indx]->num_val < i+1)
					return 0;
				match = db_in->in_info[ck_indx]->val[i];
				pre_flg = db_in->in_info[ck_indx]->pre[i];
			}
		}
		if (db_in->in_info[ck_indx]->x_vector == 0) {
			out_array[no_arr] = db_arr[bloc];
			no_arr++;
		}
	}
	out_array[no_arr] = 0;
	return 1;
}

/* FUNCTION: is_match(str1, str2, m_flgs, no_char, pre_flg)
 * RETURNS: 	-1
 *		 0
 *		result of comparison operations
 * Determines if two strings "match."  Depending upon which attribute is being
 * used either a prefix, exact, star, word, or numeric match is used.  Non 
 * alphanumeric characters and case may be ignored.
 */
static int 
is_match(char *str1, char *str2, FLAGS *m_flgs, int no_char, int pre_flg)
{
	char tmp[PQ_REC];
	int num1,num2;

	if (m_flgs->token || m_flgs->xtoken)
		return token(str1, str2, m_flgs);
	if (pre_flg || m_flgs->prefix) {
		if (!m_flgs->alpha)
			if (!m_flgs->ignore) 
				return strncmp(str1, str2, strlen(str1));
			else
				return prefix_i(str1, str2);
		if (!m_flgs->ignore)
			return prefix_a(str1, str2);
		return prefix_ai(str1, str2);
	}		
	if (m_flgs->exact || m_flgs->star) {
		if (!m_flgs->alpha)
			if (!m_flgs->ignore)
				return exact(str1, str2);
			else
				return exact_i(str1, str2);
		if (!m_flgs->ignore)
			return exact_a(str1, str2);
		return exact_ai(str1, str2);
	}		
	if (m_flgs->numeric) {
		if (*str1 == 0)
			return(-1);
		if (m_flgs->alpha && no_char != 0)  {
			strcpy(tmp, ev_alphanum(str2));
			if ((num1 = atoi(str1)) == (num2 = atoi(tmp)))
				return(0);
			if (num1 > num2)
				return(1);
		} else {
			if ((num1 = atoi(str1)) == (num2 = atoi(str2)))
				return(0);
			if (num1 > num2)
				return(1);
		}
		return(-1);
	}
	if (m_flgs->word) {
		if (no_char == 0)
			return (*str1 - *str2);
		if (m_flgs->ignore)
			return ev_word_i(str1, str2);
		return ev_word_match(str1, str2);
	}
	return(-1);
}

static int
exact(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)  
		if (*s1 != *s2) break;
	for (;*s2 && *s2==' ';s2++);
	return(*s1 - *s2);
}

static int
exact_a(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)  {
		for (;*s2 && !isalnum(*s2);s2++);
		if (*s1 != *s2) break;
	}
	for (;*s2 && (*s2==' ' || !isalnum(*s2));s2++);
	return(*s1 - *s2);
}

static int
exact_i(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)  
		if (*s1 != tolower(*s2)) break;
	for (;*s2 && *s2==' ';s2++);
	return(*s1 - tolower(*s2));
}

static int
exact_ai(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)   {
		for (;*s2 && !isalnum(*s2);s2++);
		if (*s1 != tolower(*s2)) break;
	}
	for (;*s2 && (*s2==' ' || !isalnum(*s2));s2++);
	return(*s1 - tolower(*s2));
}

static int
prefix_a(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)  {
		for (;*s2 && !isalnum(*s2);s2++);
		if (*s1 != *s2) break;
	}
	if (!*s1) return(0);
	return(*s1 - *s2);
}

static int
prefix_i(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)  
		if (*s1 != tolower(*s2)) break;
	if (!*s1) return(0);
	return(*s1 - tolower(*s2));
}

static int
prefix_ai(char *s1, char *s2)
{
	for (;*s1 && *s2;s1++,s2++)   {
		for (;*s2 && !isalnum(*s2);s2++);
		if (*s1 != tolower(*s2)) break;
	}
	if (!*s1) return(0);
	return(*s1 - tolower(*s2));
}

static int
ev_word_i(char *s1, char *s2)
{
        while (*s1 || *s2) {
                if (*s1 != tolower(*s2))
                        return 1;
                for (; isalnum(*s2) && *s1 == tolower(*s2); s1++, s2++);
                for (; isalnum(*s2); s2++);
                for (; *s1 && !isalnum(*s1); s1++);
                for (; *s2 && !isalnum(*s2); s2++);
        }
        return 0;
}

static int 
token(char *s1, char *s2, FLAGS *m_flgs)
{
	int	ms = 0;		/* match/no match status (0 == match) */
	int	xtoken;		/* local copy of m_flgs->xtoken */
	int	(*compare)(char*, char*);	/* comp fcn to use */
	char	sc;		/* "saved" character from user value */
	char	sc2;		/* "saved" character from db value */
	char	*s2p = s2;		/* floating pointer to db/index value */
	char	*s2tep;		/* floating pointer to db/index value */
	char	*tsp;		/* points to start of a user token */
	char	*tep;		/* points to end of a user token */
	char	*ep;		/* points to end of user value string */

	xtoken = m_flgs->xtoken;
	if (!*s1)			/* null user value */
		return xtoken ? -1 : 0;
	if (!*s2)			/* null db/index value can't match */
		return *s1;
	compare = m_flgs->ignore ? (xtoken ? exact_ai : prefix_ai) : (xtoken ? exact_a : prefix_a);
	tsp = s1;
	ep = s1 + strlen(s1);
	tep = strpbrk(s1, TOKEN_SEPS);
	if (tep == nil)
		tep = ep;
	sc = *tep;
	*tep = 0;
	s2tep = nil;
	sc2 = 0;

	/*
	 * For each user-specified token, try to find a matching token
	 * in the index or database value.  For the token match type,
	 * a user token may span one or more index/db tokens.  But, for
	 * the tokenexact match type, spanning is not allowed.
	 */
	while (!ms && tsp < ep) {
		/*
		 * see if this token matches the index/db value
		 */
		while (s2p && *s2p) {
			if (xtoken) {
				/* don't allow spanning */
				s2tep = strpbrk(s2p, TOKEN_SEPS);
				if (s2tep == nil)
					s2tep = s2p + strlen(s2p);
				sc2 = *s2tep;
				*s2tep = 0;
			}
			if (!(ms = (*compare)(tsp, s2p)))
				break;
			if (xtoken)
				*s2tep = sc2;
			s2p = strpbrk(s2p, TOKEN_SEPS);
			if (s2p != nil)
				while (*s2p && strchr(TOKEN_SEPS, *s2p))
					s2p++;
		}
		*tep = sc;
		if (xtoken)
			*s2tep = sc2;
		if (ms)
			return ms;	/* unsuccessful match */
		/*
		 * The current user token matched.  Set up tsp/tep around
		 * the next user token, if any.
		 */
		while (*tep && strchr(TOKEN_SEPS, *tep))
			tep++;
		tsp = tep;
		tep = strpbrk(tsp, TOKEN_SEPS);
		if (tep == nil)
			tep = ep;
		sc = *tep;
		*tep = 0;
		s2p = s2;
	}
	*tep = sc;
	return ms;
}

/*
 * Reads a line from the database file beginning at offset_value.
 */
static char *
rd_db(char *offset_value)
{
	int read_end;
	char *ptr, *end;

	if ((read_end = get_char((uchar*) offset_value) - 1) < 0)
		return nil;
	if (seek(db_in->db_ptr, read_end, 0) == -1) {
		ev_err_no = SYSERR;
		sprint(ev_err_msg, "rd_db: seek %d bytes into %s database",
			read_end, db_in->db_nm);
		return ev_error_proc(ev_err_msg);
	}
	ptr = db_in->db_buf;
	end = ptr + PQ_REC;
	do {
		int n = read(db_in->db_ptr, ptr, 128);		/* make it whatever size you want */
		if (n < 0) {
			ev_err_no = SYSERR;
			sprint(ev_err_msg, "rd_db: %s", db_in->db_nm);
			return ev_error_proc(ev_err_msg);
		}
		ptr[n] = 0;
		if (strchr(ptr, '\n') != nil)
			return db_in->db_buf;
		ptr += n;
	} while (ptr < end);
	ev_err_no = 0;
	sprint(ev_err_msg, "rd_db: %s: record too long", db_in->db_nm);
	return ev_error_proc(ev_err_msg);
}

/*
 * Convers 4 byte binary character to an inter offset into the data file.
 * Used by EV_READ to get the offsets from the lowest level index file.
 */
static int 
get_char(unsigned char *c_val)
{
	int n = OFF_LEN;
	int i_val = 0;

	while (n--)  {
		i_val <<= NB;
		i_val |= *c_val++;
	}
	return(i_val);
}

static void
copy_key(EV_INFO *db, int rep)
{
	int	maxlen;		/* biggest token length seen so far */
	int	len;
	FLAGS	*f;
	char	*dst, *src;
	char	*csp;		/* "current" token start pointer */
	char	*cep;		/* "current" token end pointer */
	char	*msp;		/* longest token start pointer */

	dst = db->key;
	src = db->maj_val->val[rep];
	f = db->pptr[db->maj_val->proto_no]->p_match;
	if (!(f->token || f->xtoken) || !strpbrk(src, TOKEN_SEPS)) {
		strcpy(dst, src);
		return;
	}
	if (!*src) {
		*dst = 0;
		return;
	}
	cep = src;
	maxlen = -1;
	msp = nil;
	while (cep && *cep) {
		csp = cep;
		cep = strpbrk(csp, TOKEN_SEPS);
		len = (cep == nil) ? strlen(csp) : cep - csp;
		if (len > maxlen) {
			msp = csp;
			maxlen = len;
		}
		if (cep)
			while (*cep && strchr(TOKEN_SEPS, *cep))
				cep++;
	}
	ev_cpy(dst, msp, maxlen);
}

static char* load;
static int bcount;

/*
 * returns the number of chars before the delim, if any
 */
int 
ev_len(char *str, char delim)	
{
	char *p;

	for (p = str; *p != 0 && *p != '\n' && *p != delim; p++);
	return p - str;
}

/* FUNCTION: ev_addr(db_str, db_sep, arr)
 * RETURNS:
 * OUTPUT: an array with each element a pointer to a database field. 
 * USED BY: CHECK_QUALIFIER and genesis.c 
 * DESCRIPTION: 
 *  used by CHECK_QUALIFIER to match values in the database against the input 
 *  vector, and by genesis.c to create index files.
 */
int
ev_addr(char *db_str, char db_sep, char **arr)
{
	char *ptr, *arr_elem;
	int i = 1, done = 0;

	for (ptr = db_str; !done; arr[i] = arr_elem, *ptr++ = 0, i++) {
		for (arr_elem = ptr; *ptr != '\n' && *ptr != db_sep; ptr++)
			;
		if (*ptr == '\n')  
			done = 1;
	}
	return i - 1;
}

/* FUNCTION: ev_proto(type, top_struct, numflds)
 * RETURNS: -1, if type is not found in the proto file struct
 *	index of type in the proto file struct 
 * Searches the structure containing the proto file and returns the 
 * corresponding index into the structure.
 * 
 */
int
ev_proto(char *type, PROTO_INFO **top_struct, int numflds)
{
	int i;

	for (i = 0; i < numflds; i++)
		if (strcmp(top_struct[i]->p_type, type) == 0)
			return(i);
	return(-1); 
}

/* FUNCTION: ev_file(path_name, file_base, increm)
 * RETURNS: full path file name with levels included for index files.
 */
char *
ev_file(char *path_name, char *file_base, int increm)
{
	static char file_name[256], file2_name[256];

	sprint(file_name, "%s%s", path_name, file_base); 
	if (increm == -1) /* don't want a lower level index file name */
		 return file_name;
	sprint(file2_name, "%c%d", LEVEL_SEP, increm+1);
	strcat(file_name, file2_name);
	return file_name;
}

int
ev_root(char *root_file, int key_len)
{
	Dir *d;
	vlong length;

	d = dirstat(root_file);
	length = d->length;
	free(d);
	if (key_len == 0 && length < INDX_BLK-1)
		return 1;
	if (key_len == 0) 
		return 0;
	if ((length/INDX_BLK + 2) * key_len < INDX_BLK-1) 
		return 1;
	return 0;
}

/* FUNCTION: ev_ld(proto_file, p_ptr)
 * RETURNS: -1, on failure
 *	    -2, if there isn't an "m=" string in the proto file
 *	 index in the proto file struct of the "m=" attribute
 * DESCRIPTION:
 *   Loads the proto file into proto file structure.
 */
int
ev_ld(char *proto_file, PROTO_INFO **p_ptr)
{
	Dir *d;
	char *extra, *ptr, *info;
	int proto_des, pi, mult_fld = -2;
	vlong length;

	if ((proto_des = open(proto_file,OREAD)) == -1) {
		ev_err_no = SYSERR;
		return -1;
	}
	d = dirfstat(proto_des);
	length = d->length;
	free(d);
	if ((load = info = malloc(length+1)) == nil
	|| read(proto_des, info, length) != length) {
		ev_err_no = SYSERR;
		close(proto_des);
		free(info);
		return -1;
	}
	info[length] = 0;
	close(proto_des);

	*x_val = 0;
	bcount = 0;
	pi = 0;
	while (pi < NIX-1 && bcount < length) {
		if (*load != COMMENT && *load != BLANK && *load != '\n') {
			if ((p_ptr[pi] = malloc(sizeof(PROTO_INFO))) == nil) {
				ev_err_no=SYSERR;
				free(info);
				return -1;
			}
			strcpy(p_ptr[pi]->p_type, ev_elem());
			p_ptr[pi]->p_field = atoi(ev_elem());
			p_ptr[pi]->i_size = atoi(ev_elem());
			p_ptr[pi]->i2_size = atoi(ev_elem());
			p_ptr[pi]->p_match = ev_match(ev_elem());
			strcpy(p_ptr[pi]->multiple, ev_elem());
			p_ptr[pi]->p_make = (*ev_elem() == 'i');
			if ((*(extra = p_ptr[pi]->multiple) == 'm')
			&& ((ptr = strchr(extra,'=')) != nil)) {
				mult_fld = pi;
				strcpy(x_val, ptr+1);
			}
			pi++;
		}
	}
	number_fields = pi;
	free(info);
	return mult_fld;
}


/* FUNCTION: ev_elem()
 * RETURNS: returns the next token 
 * DESCRIPTION: get the next token in the proto file line
 */
char *
ev_elem(void)
{
	static char buf[64];
	char *p, *t;

	for (p = load; *p == ' ' || *p == '\t'; p++)
		;
	t = buf;
	while (t < buf+sizeof buf-1 && *p && *p != ' ' && *p != '\t' && *p != '\n')
		*t++ = *p++;
	*t = 0;
	p++;
	bcount += p - load;
	load = p;
	return buf;
}

/*
 * RETURNS: a pointer to a malloc'd flag structure
 * DESCRIPTION: allocate and assign values to the flag structure of
 * the proto file structure
 */
FLAGS *
ev_match(char *flags)
{
	static FLAGS *out;
	char *p;

	out = malloc(sizeof *out);
	if (out == nil) {
		pq_error(0, "ev: ev_match: No memory:");
		return nil;
	}
	memset(out, 0, sizeof *out);
	for (p = flags; *p; p++)
		switch (*p) {
		case 'e':	out->exact = 1;	break;
		case 'p':	out->prefix = 1;	break;
		case 's':	out->star = 1;	break;
		case 'n':	out->numeric = 1;	break;
		case 'w':	out->word = 1;	break;
		case 'a':	out->alpha = 1;	break;
		case 'i':	out->ignore = 1;	break;
		case 't':	out->token = 1;	break;
		case 'x':	out->xtoken = 1;	break;
		}
	return out;
}

/*
 * RETURNS: a modified val string 
 * DESCRIPTION:
 * Non alphanumeric characters are removed and characters are changed to
 * lower case as dictated by the Proto file.  Values with star matching
 * are checked for an * at the End of the string, in which case the
 * * is removed.
 */
char*
ev_modify(int key_type, char *val, PROTO_INFO **mod_struct)
{
	FLAGS *f = mod_struct[key_type]->p_match;

	if (f->star) {
		int j = strlen(val) - 1;
		if (val[j] == PREFIX)
			val[j] = 0;
	}
	if (f->ignore)
		ev_lower(val);
	if (f->alpha)
		strcpy(val, ev_alphanum(val));
	if (f->token || f->xtoken)
		strcpy(val, ev_token(val));
	if (f->numeric)
		sprint(val, "%d", atoi(val));
	return(val);
}

/*
 * RETURNS: a modified orig_key
 * DESCRIPTION: Used by ev_modify to convert characters into lower case.
 */
void
ev_lower(char *orig_key)
{
	char *mod_ptr;

	for (mod_ptr=orig_key; *mod_ptr; mod_ptr++) 
		*mod_ptr = tolower(*mod_ptr);
}

/* FUNCTION: ev_alphanum(orig_key)
 * RETURNS: a modified orig_key
 * DESCRIPTION: Used by ev_modify to delete non alphanumeric characters.
 */
char *
ev_alphanum(char *orig_key)
{
	static char mod_str[PQ_REC];
	char *mod_ptr;
	int mk_i = 0;
	
	for (mod_ptr=orig_key; *mod_ptr; mod_ptr++) 
		if (isalnum(*mod_ptr))
			mod_str[mk_i++] = *mod_ptr;
	mod_str[mk_i] = 0;
	return mod_str;
}

/*
 * RETURNS: a modified orig_key
 * DESCRIPTION: Used by ev_tmodify to delete non alphanumeric characters that
 *		are not token separators.
 */
char *
ev_token(char *orig_key)
{
	static char mod_str[PQ_REC];
	char *mod_ptr;
	int mk_i = 0;
	
	for (mod_ptr = orig_key; *mod_ptr; mod_ptr++) 
		if (isalnum(*mod_ptr) || strchr(TOKEN_SEPS, *mod_ptr))
			mod_str[mk_i++] = *mod_ptr;
	mod_str[mk_i] = 0;
	return mod_str;
}

/* FUNCTION: ev_word_match(s1, s2)
 * RETURNS: 1 - if the two strings are not equal
 * 	    0 - if the two strings match
 * DESCRIPTION: 
 * Matching algorthm used for certain attribute types such as titles.
 */
int
ev_word_match(char *s1, char *s2)
{
	while (*s1 || *s2) {
		if (*s1 != *s2)
			return 1;
		for (; isalnum(*s2) && *s1 == *s2; s1++, s2++);
		for (; isalnum(*s2); s2++);
		for (; *s1 && !isalnum(*s1); s1++);
		for (; *s2 && !isalnum(*s2); s2++);
	}
	return 0;
}

/* FUNCTION: ev_error_proc(error_str)
 * RETURNS:  nil always
 * DESCRIPTION: 
 * Creates error message and returns nil to calling routine.
 */
char *
ev_error_proc(char *error_str)
{
	char extra_error[256];

	sprint(extra_error , "%s%s", error_str, ev_err);
	strcpy(ev_err, extra_error);
	ev_err_msg[0] = 0;
	return nil;
}

/* FUNCTION: ev_paths(argv, db)
 * RETURNS:  -1 , on failure
 *	     0, on success
 * DESCRIPTION:
 * Determines name for data file, proto file, location for index files and
 * separator used to delimit database fields, and optionally locks database.
 */
int
ev_paths(char **argv, EV_INFO *db)
{
	int argc;
	char lockf[256];
	char root[256];
	char *tail;
	char err[ERRMAX];

	for (argc = 0; argv[argc] != nil; argc++)
		;

	db->sep = DB_SEP;
	if (argc >= 1 && strncmp(argv[0], "-d", 2) == 0) {
		if (strlen(argv[0]) == 2 && argc >= 2) {
			argc--, argv++;
			db->sep = argv[0][0];
		} else
			db->sep = argv[0][2];
		argc--, argv++;
	}

	sprint(db->path_name, "%s/", pq_path(argc == 1 ? argv[0] : "."));
	strcpy(db->db_nm, argc >= 2 ? pq_path(argv[0]) : ev_file(db->path_name, "Data", -1));
	strcpy(db->p_nm, argc >= 2 ? pq_path(argv[1]) : ev_file(db->path_name, "Proto", -1));
	strcpy(lockf, argc >= 3 ? pq_path(argv[2]) : ev_file(db->path_name, "Lock", -1));

	if (access(db->db_nm, 4) < 0 && argc == 1) {
		strcpy(root, db->db_nm);
		tail = strrchr(root, '/');
		*tail = 0;		/* strip off "Data" */
		tail = strrchr(root, '/');
		*tail++ = 0;		/* split off last component */
		sprint(db->path_name, "%s/%s.", root, tail);
		sprint(db->db_nm, "%s/D.%s", root, tail);
		sprint(db->p_nm, "%s/P.%s", root, tail);
		sprint(lockf, "%s/L.%s", root, tail);
	}

	/* arrange for lock file */
	db->lockfd = -1;
	if (access(lockf, 0) == 0) {
		while ((db->lockfd = open(lockf, OREAD)) < 0) {
			errstr(err, sizeof err);
			if (strstr(err, "file is locked") == nil) {
				pq_error(0, "ev: lock %s: %s", lockf, err);
				return -1;
			}
			sleep(500);
		}
	}

	return 0;
}
