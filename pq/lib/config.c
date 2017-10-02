#include "all.h"
Module modtab[] = {
"alias", alias_open, alias_close, alias_read, alias_write,
"call", call_open, call_close, call_read, call_write,
"copy", copy_open, copy_close, copy_read, copy_write,
"ev", ev_open, ev_close, ev_read, ev_write,
"forge", forge_open, forge_close, forge_read, forge_write,
"format", format_open, format_close, format_read, format_write,
"join", join_open, join_close, join_read, join_write,
"log", log_open, log_close, log_read, log_write,
"opt", opt_open, opt_close, opt_read, opt_write,
"path", path_open, path_close, path_read, path_write,
"pq", pq_open, pq_close, pq_read, pq_write,
"rewrite", rewrite_open, rewrite_close, rewrite_read, rewrite_write,
"suffix", suffix_open, suffix_close, suffix_read, suffix_write,
"virt", virt_open, virt_close, virt_read, virt_write,
"zap", zap_open, zap_close, zap_read, zap_write,
nil};
