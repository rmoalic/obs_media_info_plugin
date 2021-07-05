#ifndef utils_include
#define utils_include 1

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define allocfail_return_null(ptr) do {if (ptr == NULL) { perror(__FILE__ ":" STRINGIZE(__LINE__)" alloc failled"); return NULL; }} while(0)
#define allocfail_return(ptr) do {if (ptr == NULL) { perror(__FILE__ ":" STRINGIZE(__LINE__) " alloc failled"); return; }} while(0)
#define allocfail_exit(ptr) do {if (ptr == NULL) { perror(__FILE__ ":" STRINGIZE(__LINE__) " alloc failled"); exit(EXIT_FAILURE); }} while(0)
#define allocfail_print(ptr) do {if (ptr == NULL) { perror(__FILE__ ":" STRINGIZE(__LINE__) " alloc failled"); }} while(0)

#define efree(ptr) do {if (ptr != NULL) { free(ptr); ptr = NULL; }} while(0)
#define estrcmp(a, b) ((a == NULL || b == NULL) ? 0 : strcmp(a, b))

#endif
