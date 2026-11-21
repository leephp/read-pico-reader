#include "host.h"
#include "read_pico_sd.h"
#include "read_pico_pmu.h"
#include "read_pico_transfer.h"
#include "read_pico_search.h"
#include "display.h"
#include "epd_highlevel.h"
#include "esp_app_desc.h"
#include "boot_state.h"
#include "esp_vfs_fat.h"
#include "sc7a20h.h"
#include "sc7a20h_lab.h"
#include "esp_timer.h"
#include "ff.h"
#include "ble_page_turner.h"
#include "book_cover.h"
#include "book_weread_cover.h"
#include "app_transfer_mode.h"
#include "continuous_du.h"
#include "kiiko_device_sync.h"
#include "ota_online.h"
#include "ota_update.h"
#include "app.h"
#include <string.h>
#include <time.h>
#include <sys/statvfs.h>

const EpdWaveform E0470_WAVEFORM={0},E0470_FOLLOW_WAVEFORM={0},E0470_TEXTTURN_WAVEFORM={0};
static bool wifi;
void sim_wifi_toggle(void){wifi=!wifi;}
bool sim_wifi_on(void){return wifi;}
int read_pico_sd_get_info(read_pico_sd_info_t*i){memset(i,0,sizeof(*i));i->present=i->mounted=true;strcpy(i->name,"Mac");i->capacity_bytes=16ULL*1024*1024*1024;i->free_bytes=8ULL*1024*1024*1024;return 0;}
int read_pico_sd_start_probe(void){return 0;}int read_pico_sd_remount(void){return 0;}int read_pico_sd_sync(void){return 0;}
int read_pico_sd_format(void){return ESP_ERR_NOT_SUPPORTED;}
const pmu_snapshot_t *read_pico_pmu_get(void){static pmu_snapshot_t p;p.present=p.status_ok=p.identity_ok=true;p.flags=PMU_STATUS_BATTERY_VALID;p.soc_permille=780;p.time_synced=true;p.unix_sec=time(NULL);return &p;}
void read_pico_transfer_get_status(read_pico_transfer_status_t*s){memset(s,0,sizeof(*s));s->wifi_enabled=s->network_ready=wifi;s->mode=READ_PICO_TRANSFER_MODE_STA;}
int read_pico_transfer_get_saved_wifi(char*out,bool*configured){if(out)strcpy(out,"Mac 模拟网络");if(configured)*configured=wifi;return 0;}
void epd_hl_set_all_white(EpdiyHighlevelState*h){if(h->front_fb)memset(h->front_fb,255,1216*684/2);}
uint8_t *epd_hl_get_framebuffer(EpdiyHighlevelState*h){return h->front_fb;}
void guard_draw_result(EpdiyHighlevelState*h,enum EpdDrawError e){(void)h;(void)e;}
void rails_keepalive(void){}void display_set_bulk_io(bool active){(void)active;}
void display_set_reader_rails(bool reader){(void)reader;}
enum EpdDrawError update_display_mode(EpdiyHighlevelState*h,enum EpdDrawMode m){return 0;}
enum EpdDrawError update_display_mode_diff(EpdiyHighlevelState*h,enum EpdDrawMode m){return 0;}
enum EpdDrawError update_display_full(EpdiyHighlevelState*h){return 0;}
enum EpdDrawError update_display_from_white(EpdiyHighlevelState*h){return 0;}
enum EpdDrawError update_display_fast_page(EpdiyHighlevelState*h){return 0;}
enum EpdDrawError update_display_white(EpdiyHighlevelState*h){return 0;}
bool display_take_white_exit(void){return false;}
void display_main_transition_cancel(void){}
void display_main_transition_arm(const void*p,bool a,bool b){}
void display_main_transition_disarm(void){}
enum EpdDrawError update_display_area_with(EpdiyHighlevelState*h,const EpdWaveform*w,enum EpdDrawMode m,EpdRect a){return 0;}
enum EpdDrawError update_display_area_diff_with(EpdiyHighlevelState*h,const EpdWaveform*w,enum EpdDrawMode m,EpdRect a){return 0;}
enum EpdDrawError update_display_area_full_with(EpdiyHighlevelState*h,const EpdWaveform*w,enum EpdDrawMode m,EpdRect a){return 0;}
const esp_app_desc_t *esp_app_get_description(void){static esp_app_desc_t a={.version=SIM_FIRMWARE_VERSION,.project_name="Pico",.date="2026-10-12",.idf_ver="Mac simulator"};return &a;}
bool pico_boot_asset_allowed(const char*p){return true;}
bool pico_boot_recovery(void){return false;}
void pico_boot_book_end(bool ok){}
void pico_boot_clear_resume(void){}
void pico_boot_hold_resume(void){}
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EM_JS(void,sim_random,(void *p,size_t n),{crypto.getRandomValues(HEAPU8.subarray(p,p+n));});
void esp_fill_random(void*p,size_t n){sim_random(p,n);}
#else
void esp_fill_random(void*p,size_t n){arc4random_buf(p,n);}
#endif
int esp_vfs_fat_spiflash_mount_rw_wl(const char*p,const char*label,const esp_vfs_fat_mount_config_t*c,wl_handle_t*w){*w=1;sim_mkdir(p,0700);return 0;}
int esp_vfs_fat_info(const char*p,uint64_t*total,uint64_t*free){*total=16ULL*1024*1024*1024;*free=8ULL*1024*1024*1024;return 0;}
int f_getfree(const char*p,DWORD*n,FATFS**fs){static FATFS f={.csize=8,.n_fatent=2000000};*fs=&f;*n=1000000;return 0;}

const EpdWaveform E0470_FULL_WAVEFORM={0};
const EpdWaveform *display_reader_text_waveform(void){return &E0470_TEXTTURN_WAVEFORM;}
void display_main_transition_shelf_page(void){}
enum EpdDrawError update_display_with(EpdiyHighlevelState*h,const EpdWaveform*w,enum EpdDrawMode m){return 0;}
enum EpdDrawError update_display_water_turn(EpdiyHighlevelState*h,EpdRect a,e0470_turn_dir_t d){return 0;}
static int turn_tick=14000;
void e0470_page_turn_set_tick_us(int n){turn_tick=n;}int e0470_page_turn_tick_us(void){return turn_tick;}void e0470_page_turn_release(void){}
bool app_poll_main_navigation(app_ctx_t*c){return false;}
int ui_key_hit_test(uint16_t x,uint16_t y){return y>=1216?(x<228?0:x<456?1:2):-1;}
int pico_boot_book_begin(const char*p){return 0;}
void pmu_selftest_prepare_powerdown(void){}
int read_pico_pmu_cmd(uint16_t c,const uint8_t*p,uint8_t n){return ESP_ERR_NOT_SUPPORTED;}
void esp_restart(void){}
int read_pico_transfer_sync_time(uint32_t*s){return ESP_ERR_NOT_SUPPORTED;}
int read_pico_transfer_import_wifi_backup(const read_pico_transfer_wifi_backup_t*b){return ESP_ERR_NOT_SUPPORTED;}
int read_pico_transfer_export_wifi_backup(read_pico_transfer_wifi_backup_t*b){memset(b,0,sizeof(*b));return ESP_ERR_NOT_SUPPORTED;}
bool read_pico_transfer_wifi_backup_valid(const read_pico_transfer_wifi_backup_t*b){return false;}
static bool checked;
int pico_online_check(void){checked=true;return ESP_ERR_NOT_SUPPORTED;}int pico_online_download(void){return ESP_ERR_NOT_SUPPORTED;}int pico_online_commit(void){return ESP_ERR_NOT_SUPPORTED;}
bool pico_online_busy(void){return false;}void pico_online_cancel_join(void){}
void pico_online_get_status(pico_update_status_t*s){memset(s,0,sizeof(*s));s->state=checked?PICO_UPDATE_FAILED:PICO_UPDATE_IDLE;snprintf(s->message,sizeof(s->message),"模拟器尚未接入联网升级");}
int pico_ota_inspect(const char*p,pico_ota_info_t*i){memset(i,0,sizeof(*i));snprintf(i->current_version,sizeof(i->current_version),"%s",SIM_FIRMWARE_VERSION);snprintf(i->message,sizeof(i->message),"模拟器暂不运行设备BIN");return ESP_ERR_NOT_SUPPORTED;}
int pico_ota_install(const char*p,char*m,size_t n){snprintf(m,n,"请在真实Pico设备上升级");return ESP_ERR_NOT_SUPPORTED;}
void ble_pt_reset_failure(void){}int ble_pt_stop(uint32_t n){return 0;}bool ble_pt_running(void){return false;}
bool ble_pt_take_start_failure(char*out,size_t n){return false;}
void ble_pt_scan_start(uint32_t n){}void ble_pt_scan_stop(void){}bool ble_pt_scanning(void){return false;}uint8_t ble_pt_device_count(void){return 0;}const ble_pt_device_t *ble_pt_device(uint8_t n){return NULL;}
int ble_pt_connect(const char*p){return ESP_ERR_NOT_SUPPORTED;}void ble_pt_disconnect(void){}bool ble_pt_connected(void){return false;}bool ble_pt_connecting(void){return false;}const char *ble_pt_connected_name(void){return "";}
uint8_t ble_pt_bond_count(void){return 0;}const ble_pt_bond_t*ble_pt_bond(uint8_t n){return NULL;}void ble_pt_forget(const char*p){}
bool ble_pt_pop_key(ble_pt_event_t*e){return false;}bool ble_pt_pop_raw(ble_pt_raw_t*r){return false;}
uint32_t ble_pt_raw_code(const ble_pt_raw_t*r){return 0;}
int ble_pt_bind(ble_pt_action_t a,const ble_pt_frame_t *frame){return ESP_ERR_NOT_SUPPORTED;}bool ble_pt_binding(ble_pt_action_t a,ble_pt_frame_t *out){return false;}
ble_pt_action_t ble_pt_action_for_raw(uint32_t code){return BLE_PT_ACTION_NONE;}
ble_pt_action_t ble_pt_action_for_usage(uint8_t usage,uint8_t mods){return BLE_PT_ACTION_NONE;}
#include "ui_nav.h"
#include "ui_kit.h"
static void transfer_render(app_ctx_t*c,uint8_t*f){ui_clear_page(f);ui_nav_status(f);ui_draw_header(f,"文件传输","Mac 模拟器");ui_text(f,342,430,30,"通过模拟器侧栏导入书籍和字体",EPD_DRAW_ALIGN_CENTER,false);ui_text(f,342,510,24,"WiFi / USB 硬件传输尚未接入",EPD_DRAW_ALIGN_CENTER,false);ui_nav_draw(f,2);}
static app_redraw_t transfer_key(app_ctx_t*c,int k){c->request_return=true;return APP_REDRAW_PAGE;}
const app_desc_t app_transfer={.title="文件传输",.render=transfer_render,.on_key=transfer_key};
void app_transfer_request_hotspot_start(void){}void app_transfer_request_usb_start(void){}void app_transfer_request_wifi_setup(void){}void app_transfer_request_wifi_upload(void){}
void app_transfer_request_cover_wifi(void){}
static void sync_render(app_ctx_t*c,uint8_t*f){ui_clear_page(f);ui_nav_status(f);ui_draw_header(f,"云同步","设备间同步");ui_text(f,342,430,30,"Mac 模拟器不执行设备间同步",EPD_DRAW_ALIGN_CENTER,false);ui_nav_draw(f,2);}
const app_desc_t app_cloud_sync={.title="云同步",.render=sync_render};
esp_err_t app_factory_reset(void){return ESP_ERR_NOT_SUPPORTED;}

/* 设备间同步需要双机射频；宿主给空状态。/ Device sync needs two radios; the host reports idle. */
esp_err_t kiiko_sync_start(kiiko_sync_role_t role,const kiiko_sync_io_t*io){(void)role;(void)io;return ESP_ERR_NOT_SUPPORTED;}
void kiiko_sync_confirm(void){}
void kiiko_sync_cancel(void){}
void kiiko_sync_stop(void){}
bool kiiko_sync_active(void){return false;}
void kiiko_sync_get_status(kiiko_sync_status_t*out){if(out)memset(out,0,sizeof(*out));}

void ble_pt_cancel_recent(void){}
void ble_pt_reconnect_recent(void){}

/* 波形派生表在宿主没有物理面板；返回保守的常量表。 */
/* Derived panel tables have no physical panel on the host; return the conservative table. */
const EpdWaveform *display_main_fast_white_waveform(const EpdWaveform*source,enum EpdDrawMode mode){(void)mode;return source?source:&E0470_WAVEFORM;}
const EpdWaveform *display_reader_soft_turn_waveform(void){return &E0470_WAVEFORM;}
const EpdWaveform *display_reader_white_push_waveform(void){return &E0470_WAVEFORM;}
const EpdWaveform *display_settings_motion_waveform(void){return &E0470_WAVEFORM;}
const EpdWaveform *display_settings_scroll_waveform(void){return &E0470_WAVEFORM;}
enum EpdDrawError update_display_click_feedback(EpdiyHighlevelState*h,EpdRect a){(void)h;(void)a;return 0;}
enum EpdDrawError update_display_reader_area(EpdiyHighlevelState*h,const EpdWaveform*w,enum EpdDrawMode m,EpdRect a,bool full){(void)h;(void)w;(void)m;(void)a;(void)full;return 0;}
enum EpdDrawError update_display_text_turn(EpdiyHighlevelState*h,EpdRect a,e0470_turn_dir_t d){(void)h;(void)a;(void)d;return 0;}

bool read_pico_sd_io_ready(void){return true;}
esp_err_t read_pico_sd_start_space_refresh(void){return ESP_OK;}
esp_err_t read_pico_transfer_start(const read_pico_transfer_cfg_t*cfg){(void)cfg;return ESP_ERR_NOT_SUPPORTED;}
void read_pico_transfer_stop(void){}
bool read_pico_transfer_try_stop_if_idle(void){return true;}

void continuous_du_reset(void){}
void cover_ram_cache_clear(void){}
void cover_ram_cache_trim(size_t budget){(void)budget;}

/* 缩放/图层封面走宿主 ImageIO 的整图路径；这些流式接口在模拟器里一律未命中。 */
/* Scaled/layered covers use whole-image ImageIO on the host; the streaming hooks always miss. */
int book_cover_load_sized_io(const char*path,const char*title,const char*author,uint8_t*out,unsigned width,unsigned height,bool*auto_cache){(void)path;(void)title;(void)author;(void)out;(void)width;(void)height;if(auto_cache)*auto_cache=false;return -1;}
bool book_cover_shelf_ram_read(const char*path,const char*title,const char*author,uint64_t size,int64_t modified,bool list,bool fast,book_fast_cover_key_t raster,void*out,size_t capacity,cover_ram_meta_t*meta){(void)path;(void)title;(void)author;(void)size;(void)modified;(void)list;(void)fast;(void)raster;(void)out;(void)capacity;if(meta)memset(meta,0,sizeof(*meta));return false;}
void book_cover_shelf_ram_store(const char*path,const char*title,const char*author,uint64_t size,int64_t modified,uint64_t font_key,bool list,bool fast,book_fast_cover_key_t raster,const uint8_t*pixels,bool white_edge){(void)path;(void)title;(void)author;(void)size;(void)modified;(void)font_key;(void)list;(void)fast;(void)raster;(void)pixels;(void)white_edge;}
bool book_cover_shelf_no_artwork(const char*path,const char*title,const char*author,uint64_t size,int64_t modified,bool remember){(void)path;(void)title;(void)author;(void)size;(void)modified;(void)remember;return false;}
bool book_image_file_xth_grayscale(const char*path,unsigned width,unsigned height,uint8_t*out){(void)path;(void)width;(void)height;(void)out;return false;}
bool book_image_xth_dimensions(const char*path,unsigned*width,unsigned*height){(void)path;(void)width;(void)height;return false;}
bool book_png_file_layer_rows(const char*path,unsigned width,unsigned height,bool(*sink)(void*,unsigned,unsigned,const uint8_t*,const uint8_t*),void*context){(void)path;(void)width;(void)height;(void)sink;(void)context;return false;}
enum EpdDrawError update_display_image_gray(EpdiyHighlevelState*h){return 0;}

void ble_pt_learn_begin(void){}
ble_pt_action_t ble_pt_action_for_frame(const ble_pt_frame_t*f){return BLE_PT_ACTION_NONE;}
void display_main_transition_gray_bands(const EpdRect*b,unsigned count){}
enum EpdDrawError display_panel_repair(EpdiyHighlevelState*h){return 0;}
int read_pico_sd_refresh_space(void){return 0;}
void rp_boot_mark(const char*n){}
const EpdWaveform *display_main_bw_waveform(void){return &E0470_WAVEFORM;}
const EpdWaveform *display_main_gray_waveform(void){return &E0470_WAVEFORM;}
const EpdWaveform *display_main_normal_waveform(void){return &E0470_WAVEFORM;}

/* 微信读书封面下载需要 cJSON 与真实网络；模拟器按“没有下载封面”处理。 */
/* WeRead cover download needs cJSON and a real network; the simulator always reports "no cover". */
bool book_weread_cover_is_txt(const char*p){(void)p;return false;}
bool book_weread_cover_metadata(const char*p,char*t,size_t tc,char*a,size_t ac){(void)p;(void)t;(void)tc;(void)a;(void)ac;return false;}
uint64_t book_weread_cover_stamp(const char*p){(void)p;return 0;}
bool book_weread_cover_read(const char*p,uint8_t*out,unsigned w,unsigned h){(void)p;(void)out;(void)w;(void)h;return false;}
esp_err_t book_weread_cover_start(book_weread_path_t*paths,unsigned count){(void)paths;(void)count;return ESP_ERR_NOT_SUPPORTED;}
void book_weread_cover_cancel(void){}
book_weread_cover_status_t book_weread_cover_status(void){return (book_weread_cover_status_t){0};}
unsigned book_weread_cover_revision(void){return 0;}
book_weread_cover_row_t book_weread_cover_row(unsigned row){(void)row;return (book_weread_cover_row_t){.state=BOOK_WEREAD_NOT_FOUND};}
void book_weread_cover_focus(unsigned row){(void)row;}
bool book_weread_cover_option(unsigned row,unsigned option,book_weread_cover_option_t*out,uint8_t*preview){(void)row;(void)option;(void)preview;if(out)memset(out,0,sizeof(*out));return false;}
bool book_weread_cover_choose(unsigned row,unsigned option){(void)row;(void)option;return false;}
