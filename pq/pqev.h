/*
 * pq stuff for ev database
 */

#define	INDX_BLK		4096
#define	OFF_LEN 		4			/* # of bytes in binary rep of offset */
#define	OFF_MAX 	10
#define	NB		8
#define	ATTR_NAME_SZ	64			/* max len of attribute name */
#define	NUM_DUP	32			/* # of values that can be assigned to an attr */
#define	MULT_ATTR_SZ	12			/* size of mult attr value */
#define	NIX		64 			/* max num of attributes */
#define	SYSERR		1			/* just need a non-zero value */
#define	A_READ		4
#define	DB_SEP		'|'			/* default field separator (-d option) */
#define	LEVEL_SEP	'='			/* to create multi-level index filenames */
#define	BLANK		' '
#define	COMMENT	'#'
#define	PREFIX		'*'
#define	ATTRIBUTE	"attribute"
#define	TOKEN_SEPS	"@!.&-,_:/ \f\n\r\t\v"

#define	ev_cpy(f, s, l)	(strncpy((f), (s), (l)), (f)[l] = 0)

typedef struct {		
	unsigned int	exact: 1;	
	unsigned int	prefix: 1;
	unsigned int	star: 1;
	unsigned int	word: 1;
	unsigned int	numeric: 1;
	unsigned int	alpha: 1;
	unsigned int	ignore: 1;
	unsigned int	token: 1;
	unsigned int	xtoken: 1;
} FLAGS;

typedef struct {
	char		p_type[ATTR_NAME_SZ+1];	/* the name of this attribute */
	int		p_field;			/* position of field within record */
	int		i_size;
	int		i2_size;
	FLAGS*		p_match;
	char		multiple[MULT_ATTR_SZ+1];
	int		p_make;			/* set if this attr is indexed */
} PROTO_INFO;

typedef struct {
	int		proto_no;			/* where in the proto file this attr is located */
	char*		val[NUM_DUP];		/* the values assigned to this attribute */
	int		pre[NUM_DUP];		/* specify if the values of val have a prefix */
	int		num_val;			/* num of vals this attr has been assigned */
	int		equal;			/* does this attribute have a value ? */
	int		x_vector;
} IN_INFO;

typedef struct {
	char		path_name[256];		/* Directory containing index files */
	char		db_nm[256];		/* Full name of database file */
	char		p_nm[256];		/* Full name of proto file */
	char		sep;			/* Delimiter character for data file */
	int		db_ptr;			/* File descriptor of the database file */
	PROTO_INFO*	pptr[NIX];			/* Structure containing proto file */
	int		num_flds;			/* Number of proto file entries */
	char		x_vector[MULT_ATTR_SZ];	/* Holds value of multiple attribute */
	int		x_fld;			/* Proto entry location of mult. fld */
	int		valid_extra;		/* Set if x_vector exists */
	char		db_buf[PQ_REC+1];		/* Current line from data file */
	char		read_buf[INDX_BLK+1];	/* Stores INDX_BLK from index files */
	char*		buf_ptr;			/* Pointer into read_buf */
	IN_INFO*		in_info[PQ_VEC];		/* Contains modified input array */
	IN_INFO*		in_blk;			/* Pointer input array malloced space */
	int		in_num;			/* Number of qualifiers in input vector */
	IN_INFO*		maj_val;			/* Values of major key used to search */
	int		maj_loc;			/* Location in Proto file of major type */
	int		maj_no;			/* Number of major keys */
	char		key[PQ_REC];		/* Current key value used to search */
	FLAGS*		mf;			/* Current matching type */
	int		pre;			/* Set for prefix match of star type */
	int		depth;			/* Number of index files for key */
	int		db_offset;			/* Holds offset into lowest level index */
	int		index_end;		/* Number of characters in read_buf */
	int		add_vector;		/* Set if x_vector was added */
	char		free_blk[NIX*PQ_REC];	/* Extra core memory */
	char*		free_ptr;			/* Points to next empty free_blk place */
	int		no_index;			/* Set if no index files for key exists */
	int		attribute;			/* Set if "attribute" attribute is used */
	int		lockfd;			/* fd of lock file, if one was specified */
} EV_INFO;

typedef struct {
	int		key[NIX];			/* index into PROTO_INFO, indicating attrs w/indexes */
	Biobuf*		f_ptr[NIX];
	int		num_index;		/* number of attributes to build indexes on */
} INDEX_INFO;

char	ev_err_msg[256];
char	ev_err[256];
char	x_val[MULT_ATTR_SZ];
int	ev_err_no;
int	number_fields;

int	ev_len(char*, char);
int	ev_addr(char*, char, char**);
int	ev_proto(char*, PROTO_INFO**, int);
char*	ev_file(char*, char*, int);
int	ev_root(char*, int);
int	ev_ld(char*, PROTO_INFO**);
char*	ev_elem(void);
FLAGS*	ev_match(char*);
char*	ev_modify(int, char*, PROTO_INFO**);
char*	ev_tmodify(int, char*, PROTO_INFO**);
void	ev_lower(char*);
char*	ev_alphanum(char*);
char*	ev_token(char*);
int	ev_word_match(char*, char*);
char*	ev_error_proc(char*);
int	ev_paths(char**, EV_INFO*);

#pragma	lib	"/sys/src/cmd/pq/lib/libpq.a"
