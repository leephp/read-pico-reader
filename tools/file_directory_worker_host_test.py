"""Hold the actual file browser worker across navigation and release it in deterministic order."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

source = Path(__file__).resolve().parents[1] / "main/apps/app_files.c"
text = source.read_text()
unit = r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#define FILE_MAX 512
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define pdPASS 1
enum {FILE_VIEW_LIST,FILE_VIEW_MOVE,APP_REDRAW_NONE,APP_REDRAW_PAGE};
typedef int app_redraw_t;
typedef struct { char path[288]; off_t size; int64_t modified; bool is_dir, selected; } file_item_t;
static int s_view,s_folder=3,s_count,s_counts[3],s_file_sort;
static bool s_file_desc,oom;
static atomic_bool s_book_count_running,hold,entered,mounted;
static char s_dir[288],s_message[96];
static file_item_t *s_files;
static pthread_t thread;
static void (*worker)(void *);
static void *worker_arg;
static void *thread_main(void *arg){(void)arg;worker(worker_arg);return NULL;}
static void *heap_caps_calloc(size_t n,size_t size,int caps){(void)caps;return oom?NULL:calloc(n,size);}
static int xTaskCreate(void (*fn)(void *),const char *name,int stack,void *arg,int pr,void *out){
 (void)name;(void)stack;(void)pr;(void)out;worker=fn;worker_arg=arg;
 return pthread_create(&thread,NULL,thread_main,NULL)==0;
}
static bool read_pico_sd_begin_listing(void){
 atomic_store(&entered,true);while(atomic_load(&hold))usleep(100);return atomic_load(&mounted);
}
static void read_pico_sd_end_listing(void){}
static void vTaskDelete(void *arg){(void)arg;}
static void file_selection_restore(void){}
static const char *folder_root(int folder){(void)folder;return s_dir;}
'''
for name in ("file_item_name","supported","compare_file_order","compare_files","read_folder_items"):
    unit += function(name,source) + "\n"
unit += text[text.index("static atomic_bool dir_running"):text.index("\n#endif",text.index("static atomic_bool dir_running"))]
unit += r'''
int main(int argc,char **argv){
 assert(argc==3);mounted=true;hold=true;
 snprintf(s_dir,sizeof(s_dir),"%s",argv[1]);dir_requested=true;
 assert(directory_tick()==APP_REDRAW_NONE);
 while(!atomic_load(&entered))usleep(100);
 cancel_directory();strcpy(s_dir,argv[2]);dir_requested=true;
 assert(directory_tick()==APP_REDRAW_NONE&&dir_requested&&dir_running&&s_count==0);
 hold=false;pthread_join(thread,NULL);assert(!atomic_load(&dir_result));
 assert(directory_tick()==APP_REDRAW_NONE);pthread_join(thread,NULL);
 assert(directory_tick()==APP_REDRAW_PAGE&&s_count==2);
 assert(!strcmp(file_item_name(&s_files[0]),"a.txt")&&!strcmp(file_item_name(&s_files[1]),"z.txt"));
 cancel_directory();dir_requested=true;mounted=false;s_count=0;
 directory_tick();pthread_join(thread,NULL);
 assert(directory_tick()==APP_REDRAW_PAGE&&s_count==0&&s_message[0]);
 mounted=true;oom=true;dir_requested=true;assert(directory_tick()==APP_REDRAW_PAGE&&!dir_running);
 oom=false;dir_requested=true;directory_tick();pthread_join(thread,NULL);
 cancel_directory();assert(!dir_started&&!atomic_load(&dir_result));
 free(s_files);puts("PASS: delayed A, immediate cancel and B, stale result disposal, sorted B, unmounted and OOM");
}
'''
with tempfile.TemporaryDirectory() as d:
    root=Path(d)
    for folder,names in (("A",["old.txt"]),("B",["z.txt","a.txt"])):
        (root/folder).mkdir()
        for name in names:
            (root/folder/name).touch()
    (root/"test.c").write_text(unit)
    subprocess.run(["cc","-std=gnu11","-Wall","-Wextra","-Werror","-fsanitize=undefined","-pthread",
                    str(root/"test.c"),"-o",str(root/"test")],check=True)
    subprocess.run([str(root/"test"),str(root/"A"),str(root/"B")],check=True,timeout=10)
