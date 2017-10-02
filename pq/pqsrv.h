/*
 * pq protocol (port 411) commands and responses
 */
enum{
	/* server commands */
	Sread	= 'r',
	Swrite	= 'w',
	Sclose	= 'q',

	/* server responses */
	Sprompt	= '>',
	Svalue	= '|',
	Serror	= '?',
};
