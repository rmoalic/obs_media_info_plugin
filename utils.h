#ifndef utils_include
#define utils_include 1

#define allocfail_return_null(ptr) if (ptr == NULL) { perror("alloc failled"); return NULL; }
#define allocfail_return(ptr) if (ptr == NULL) { perror("alloc failled"); return; }
#define allocfail_exit(ptr) if (ptr == NULL) { perror("alloc failled"); exit(EXIT_FAILURE); }
#define allocfail_print(ptr) if (ptr == NULL) { perror("alloc failled"); }

#define efree(ptr) if (ptr != NULL) { free(ptr); ptr = NULL; }
#define estrcmp(a, b) ((a == NULL || b == NULL) ? 0 : strcmp(a, b))

#endif
