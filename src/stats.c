#include <pebble.h>

#define SYNC_TIME_MS 1000 * 60 * 0.5
#define STATS_BUFFER_SIZE PERSIST_STRING_MAX_LENGTH + 1
#define BUFFER_SIZE PERSIST_STRING_MAX_LENGTH + 1  

  
#define  DATA_REQUEST_KEY 0x400
  
static Window *window;
static TextLayer *heading_layer;

struct TextRow 
{
  TextLayer * text_layer;
  char text[STATS_BUFFER_SIZE];
  enum {HEADER,ROW,CHANGEDATE} format;
  
};

static struct TextRow matrix[] = {
  {NULL, "FIRST ROW: 123456 KC", HEADER}, //row 1 - HEADER
  {NULL, "SECOND: 12345678 KC", ROW}, //row 2
  {NULL, "THIRD: 123124920 KC", ROW}, //row 3
  {NULL, "FOURTH: 123333 KC", ROW}, //row 4
  {NULL, "FIFTH: 22 %", ROW}, //row 5
  {NULL, "SIXTH: 44%", ROW},  //row 6  
  {NULL, "SEVENTH: date changed", CHANGEDATE}  //row 7  
};

static const int rows_count = sizeof(matrix)/sizeof(struct TextRow);


static AppTimer *timer = NULL;
//MAX CHARS ALLOWED - 30
static AppSync sync;

  
const int inbound_size = BUFFER_SIZE;
//const int outbound_size = BUFFER_SIZE; set to maximum

static uint8_t sync_buffer[BUFFER_SIZE];
const bool animated = true;

//********************************TRANSLATE ERROR************************************************
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

//**************************LOGRECT***********************************************
static void logrect(const char * title, GRect rect)
{
  APP_LOG(APP_LOG_LEVEL_INFO,"%s: sirka:%d, vyska:%d, x:%d, y:%d", title,
          rect.size.w, rect.size.h,
          rect.origin.x, rect.origin.y);
  
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

//*************************************WRITE string to storage************************************
static void writeStringToStorage(uint32_t key, const char * what)
{
  int result = persist_write_string(key, what);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Written %d bytes into storage of key %u: %s", result, (unsigned int)key, what);        
  
}


static int current_page = 0;
//****************************CURRENT PAGE ROW************************************
//returns index of the row within the current page
//returns -1 if the rowid does not belong to the current page
static int currentPageRow(int rowid)
{
  int rownumbers = rows_count -1; //minus the bottom date change row
  int page = rownumbers / rowid;
  if(page != current_page)
    return -1;
  return current_page % page;
}


//******************TUPLE CHANGE CALLBACK*****************************************
static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
    return;
  
      APP_LOG(APP_LOG_LEVEL_INFO, "tuple_changed: %d: %s", (int)key, new_tuple->value->cstring);     
      //just a paraniod safety check
      if(key > 100)
        return;
      
      writeStringToStorage(key, new_tuple->value->cstring);
      int rowid = currentPageRow(key);
      if(rowid > -1)
      {        
        text_layer_set_text(heading_layer, new_tuple->value->cstring);
      }
      //layer_mark_dirty(text_layer_get_layer(heading_layer));
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

//**********************GET*TEXT*ROW*BY*INDEX*******************************
struct TextRow* getTextRowById(int idx)
{
  if (idx >= rows_count)
  {
      APP_LOG(APP_LOG_LEVEL_ERROR,"ERROR text row index out of bounds:%d, rows_count=%d", idx, rows_count);
      return NULL;
  }
  
  return &matrix[idx];
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
  int total_vyska = 168; 
  int current_y = 0;
  int vyskaRiadku = total_vyska / rows_count;
  
  for(int i = 0; i < rows_count; i++, current_y += vyskaRiadku)
    {
    struct TextRow *row = getTextRowById(i);
    //read cached value and store it to row->text, otherwise return default value
    const char * inittext = row->text; //readStringFromStorage(i, row->text,row->text);
    TextLayer * tlayer = text_layer_create(GRect(0, current_y, 144, vyskaRiadku));     
    if(row->format == HEADER || row->format == CHANGEDATE)
    {
      text_layer_set_text_color(tlayer, GColorWhite);
      text_layer_set_background_color(tlayer, GColorBlack);
    }   
    text_layer_set_font(tlayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD ));
    text_layer_set_text_alignment(tlayer, GTextAlignmentCenter);
    text_layer_set_overflow_mode(tlayer, GTextOverflowModeFill ); 
    text_layer_set_text(tlayer, inittext);
    layer_add_child(window_layer, text_layer_get_layer(tlayer));
    row->text_layer = tlayer;
  }
  
  return;
  //**********************************************************************
  /*
  const char * weekstatsinit= readStringFromStorage(WEEKSTATS_KEY, weekstats_buffer, "...");
  const char * headinginit = readStringFromStorage(HEADING_KEY, heading_buffer, " ");
  const char * datechangeinit =  readStringFromStorage(DATECHANGE_KEY, datechange_buffer, " ");
  const char * daystatsinit = readStringFromStorage(DAYSTATS_KEY, daystats_buffer, "Waiting for data");

  
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
  */
}


//***********************UNLOAD*************
static void window_unload(Window *window)
{   
  //app_timer_cancel(timer);  
  //app_sync_deinit(&sync);
  for(int i =0; i < rows_count; i++)
  {
    struct TextRow* row = getTextRowById(i);
    text_layer_destroy(row->text_layer);
  } 
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
  window_destroy(window);  
}

//***************MAIN*******************
int main(void) {
  init();
  app_event_loop();
  deinit();
}