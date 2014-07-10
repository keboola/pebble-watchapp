#include <pebble.h>

static Window *window;
static TextLayer *heading_layer;
static Layer  *daystats_layer;
static Layer *weekstats_layer;
static TextLayer *date_changed_layer;
static AppTimer *timer = NULL;
//MAX CHARS ALLOWED - 30

#define SYNC_TIME_MS 1000 * 60 * 0.5
#define STATS_BUFFER_SIZE PERSIST_STRING_MAX_LENGTH + 1
#define BUFFER_SIZE PERSIST_STRING_MAX_LENGTH + 1
  
char daystats_buffer[STATS_BUFFER_SIZE] = "123";
char weekstats_buffer[STATS_BUFFER_SIZE] = "  ";
char heading_buffer[STATS_BUFFER_SIZE] = "  ";
char datechange_buffer[STATS_BUFFER_SIZE] = "  ";

static AppSync sync;


  
const int inbound_size = BUFFER_SIZE;
//const int outbound_size = BUFFER_SIZE; set to maximum

static uint8_t sync_buffer[BUFFER_SIZE];
const bool animated = true;

static void logrect(const char * title, GRect rect)
{
  APP_LOG(APP_LOG_LEVEL_INFO,"%s: sirka:%d, vyska:%d, x:%d, y:%d", title,
          rect.size.w, rect.size.h,
          rect.origin.x, rect.origin.y);
  
}



#define  HEADING_KEY  0x0
#define  DATECHANGE_KEY  0x1
#define  DAYSTATS_KEY  0x2
#define  WEEKSTATS_KEY  0x3
#define  DATA_REQUEST_KEY 0x4
 
//******************RESIZE LAYERS*****************************************
//resize layers dynamically according to text set
static void resize_stats_layers()
{  
  //day stats resize
  GFont dayfont = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21 );
  GRect daybounds = layer_get_frame(daystats_layer);  
  GSize daymaxsize = graphics_text_layout_get_content_size(daystats_buffer, dayfont, daybounds, GTextOverflowModeWordWrap, GTextAlignmentLeft);
  GRect daynewbounds = GRect(daybounds.origin.x, daybounds.origin.y, 158, daymaxsize.h );
  layer_set_frame(daystats_layer, daynewbounds);
    
  
  //week stats resize
  GFont weekfont = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21 );
  GRect weekbounds = layer_get_frame(weekstats_layer);  
  GSize weekmaxsize = graphics_text_layout_get_content_size(weekstats_buffer, weekfont, weekbounds, GTextOverflowModeWordWrap, GTextAlignmentLeft);
  int16_t newx = 0;  
  int16_t minHeightDistance = 10;
  int16_t newy = daybounds.origin.y + daymaxsize.h +  minHeightDistance;
  
  GRect weeknewbounds = GRect(newx, newy, 158, weekmaxsize.h );
  layer_set_frame(weekstats_layer, weeknewbounds);
  //logrect(weekstats_buffer, weeknewbounds);
  
}
  
//******************UPDATE LAYER WITH TEXT*****************************************
static void update_text_layer(Layer *layer, GContext* ctx, const char* text
                              ,const char * pfont 
                              ,GTextOverflowMode overflowmode 
                              ,GTextAlignment alignment) {
  GRect bounds = layer_get_frame(layer);  
  GFont font = fonts_get_system_font(pfont ); 
   
  //Obtain the maximum size that text occupies within a given rectangular constraint
  GSize maxsize = graphics_text_layout_get_content_size(text, font, bounds, overflowmode, alignment);
  GRect textbox = GRect(0,0,maxsize.w,maxsize.h);
  //create GRect that aligns in the center of the frame
  grect_align(&textbox, &GRect(0, 0, bounds.size.w, bounds.size.h), GAlignTopLeft, false );
     
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx,
      text,
      font,    
      textbox,
      overflowmode,
      alignment,
      NULL);
 
 
}

//******************READ STRING DATA from storage**************************************
static const char * readStringFromStorage(uint32_t key, char* buffer, const char* default_value)
{ 
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "read buffer before %s", buffer);    
  int result = persist_read_string(key, buffer, PERSIST_STRING_MAX_LENGTH);
  //APP_LOG(APP_LOG_LEVEL_INFO, "read buffer AFTER exists:%s, key:%u, %s",(result != E_DOES_NOT_EXIST) ? "True" : "False", (unsigned int)key, buffer);    
  if (result == E_DOES_NOT_EXIST)
    return default_value;
  else
    return buffer;
}

static void writeStringToStorage(uint32_t key, const char * what)
{
  int result = persist_write_string(key, what);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Written %d bytes into storage of key %u: %s", result, (unsigned int)key, what);        
  
}

//******************TUPLE CHANGE CALLBACK*****************************************
static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    
    case HEADING_KEY:
      APP_LOG(APP_LOG_LEVEL_INFO, "tuple_changed: HEADING %s", new_tuple->value->cstring);        
      text_layer_set_text(heading_layer, new_tuple->value->cstring);   
      writeStringToStorage(HEADING_KEY, new_tuple->value->cstring);
      //text_layer_set_text(heading_layer, "Heading 9012345678901234567890");
      //layer_mark_dirty(text_layer_get_layer(heading_layer));
      break;
    
    case DATECHANGE_KEY:
      APP_LOG(APP_LOG_LEVEL_INFO, "tuple_changed: DATECHANGE_KEY %s", new_tuple->value->cstring);     
      text_layer_set_text(date_changed_layer, new_tuple->value->cstring);    
      writeStringToStorage(DATECHANGE_KEY, new_tuple->value->cstring);
              
      break;
    
    case DAYSTATS_KEY:      
      APP_LOG(APP_LOG_LEVEL_INFO, "tuple_changed: DAYSTATS_KEY %s", new_tuple->value->cstring);     
      snprintf(daystats_buffer, STATS_BUFFER_SIZE,"%s", new_tuple->value->cstring);
      layer_mark_dirty(daystats_layer);
      writeStringToStorage(DAYSTATS_KEY, new_tuple->value->cstring);
      resize_stats_layers();
      break;

    case WEEKSTATS_KEY:
      APP_LOG(APP_LOG_LEVEL_INFO, "tuple_changed: WEEKSTATS_KEY %s", new_tuple->value->cstring);      
      snprintf(weekstats_buffer, STATS_BUFFER_SIZE ,"%s", new_tuple->value->cstring);      
      layer_mark_dirty(weekstats_layer);
      writeStringToStorage(WEEKSTATS_KEY, new_tuple->value->cstring);   
      resize_stats_layers();
      break;
  }
  
  
}

//******************RENDER DAYSTATS*****************************************
static void update_daystats_layer_callback(Layer *layer, GContext* ctx) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "update daystats layer");
  update_text_layer(layer, ctx, daystats_buffer,
                    FONT_KEY_ROBOTO_CONDENSED_21    ,
                    GTextOverflowModeWordWrap  ,
                    GTextAlignmentLeft);  
}


//******************RENDER WEEKSTATS*****************************************
static void update_weekstats_layer_callback(Layer *layer, GContext* ctx) {
  // APP_LOG(APP_LOG_LEVEL_INFO, "update weekstats layer");
  update_text_layer(layer, ctx, weekstats_buffer,
                    FONT_KEY_ROBOTO_CONDENSED_21       ,
                    GTextOverflowModeWordWrap  ,
                    GTextAlignmentLeft);  
}

// http://stackoverflow.com/questions/21150193/logging-enums-on-the-pebble-watch/21172222#21172222
char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

//******************ERROR CALLBACK*****************************************
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %s", translate_error(app_message_error));
}


//*********************** INIT COMMUNICATION WITH PHONE BY SENDING DUMMY COMMAND *******************
static void send_cmd(void) {  
  
  Tuplet value = TupletInteger(DATA_REQUEST_KEY, 1);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    return;
  }

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();
  
  timer = app_timer_register(SYNC_TIME_MS, (AppTimerCallback) send_cmd, NULL);
  
}

//*********************** LOAD WINDOW *******************
static void window_load(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  //GRect bounds = layer_get_frame(window_layer);
  //APP_LOG(APP_LOG_LEVEL_INFO, "weight %d", bounds.size.w);
  //APP_LOG(APP_LOG_LEVEL_INFO, "height %d", bounds.size.h);
  int heading_vyska = 22;
  int date_vyska = heading_vyska;
  int total_vyska = 164;
  int stats_vyska = (total_vyska - heading_vyska - date_vyska) / 2;
  
  int current_y = 0;
  heading_layer = text_layer_create(GRect(0, current_y, 144, heading_vyska));
  text_layer_set_text_color(heading_layer, GColorWhite);
  text_layer_set_background_color(heading_layer, GColorBlack);
  text_layer_set_font(heading_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD ));
  text_layer_set_text_alignment(heading_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(heading_layer));
  text_layer_set_overflow_mode(heading_layer, GTextOverflowModeFill );  
  text_layer_set_text(heading_layer, "uuu");
  
  current_y += heading_vyska + 6;
  
  //DAY STATS LAYER  
  daystats_layer = layer_create(GRect(0, current_y, 144, stats_vyska));
  layer_set_update_proc(daystats_layer, &update_daystats_layer_callback);
  layer_add_child(window_layer, daystats_layer);
  layer_mark_dirty(daystats_layer);
  //--
  
  current_y += stats_vyska;
  
  //WEEK STATS LAYER
  weekstats_layer = layer_create(GRect(0, current_y, 144, stats_vyska));
  layer_set_update_proc(weekstats_layer, &update_weekstats_layer_callback);
  layer_add_child(window_layer, weekstats_layer);
  layer_mark_dirty(weekstats_layer);
  
  current_y += stats_vyska;
  
  //DATA CHANGED LAYER
  date_changed_layer = text_layer_create(GRect(0, current_y, 144, date_vyska));
  text_layer_set_text_color(date_changed_layer, GColorWhite);
  text_layer_set_background_color(date_changed_layer, GColorBlack);
  text_layer_set_font(date_changed_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(date_changed_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(date_changed_layer));
  text_layer_set_overflow_mode(date_changed_layer, GTextOverflowModeFill );  
  text_layer_set_text(date_changed_layer, "24.5.2014 21:00");
  
  
  
  const char * weekstatsinit= readStringFromStorage(WEEKSTATS_KEY, weekstats_buffer, "...");
  const char * headinginit = readStringFromStorage(HEADING_KEY, heading_buffer, " ");
  const char * datechangeinit =  readStringFromStorage(DATECHANGE_KEY, datechange_buffer, " ");
  const char * daystatsinit = readStringFromStorage(DAYSTATS_KEY, daystats_buffer, "Waiting for data");

 /* 
  APP_LOG(APP_LOG_LEVEL_INFO, "init HEADING into %s", headinginit);   
  APP_LOG(APP_LOG_LEVEL_INFO, "init DAY STATS into %s", daystatsinit);   
  APP_LOG(APP_LOG_LEVEL_INFO, "init WEEK STATYS into %s", weekstatsinit);   
  APP_LOG(APP_LOG_LEVEL_INFO, "init DATECHANGE into %s", datechangeinit);   
  */
  Tuplet initial_values[] = {
    TupletCString(WEEKSTATS_KEY,weekstatsinit),
    TupletCString(HEADING_KEY, headinginit),
    TupletCString(DATECHANGE_KEY,datechangeinit),
    TupletCString(DAYSTATS_KEY, daystatsinit),
    TupletInteger(DATA_REQUEST_KEY, 0),    
  };
  //ARRAY_LENGTH(initial_values)
   app_sync_init(&sync, sync_buffer, BUFFER_SIZE, initial_values,  ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);
  
 
  //first send_cmd triggered ASAP
  timer = app_timer_register(1000, (AppTimerCallback) send_cmd, NULL);
}


//***********************UNLOAD*************
static void window_unload(Window *window)
{ 
  
  //app_timer_cancel(timer);
  app_sync_deinit(&sync);
  
  text_layer_destroy(heading_layer);
  text_layer_destroy(date_changed_layer);
   
  
}

//*********** INIT **************************
static void init(void)
{
  window = window_create();
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload
  });  
  Layer *window_layer = window_get_root_layer(window);  
  int out_size = app_message_outbox_size_maximum();
  app_message_open(inbound_size, out_size);
  window_stack_push(window, animated);   
}

//***************DEINIT*******************
static void deinit(void) {
  layer_destroy(daystats_layer);
  layer_destroy(weekstats_layer);
  window_destroy(window);
  
}

//***************MAIN*******************
int main(void) {
  init();
  app_event_loop();
  deinit();
}