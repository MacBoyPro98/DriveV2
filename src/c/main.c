#include <pebble.h>

// Persistent storage key
#define SETTINGS_KEY 1

// Define our settings struct
typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor ForegroundColor;
  GColor WeatherBackgroundColor;
  GColor WeatherForegroundColor;
  GColor TimeColor;
  GColor DateColor;
  GColor WeatherColor;
  int WeatherCheckRate;
  bool TemperatureUnit; // false = Celsius, true = Fahrenheit
  bool ShowDate;
} ClaySettings;

// An instance of the struct
static ClaySettings settings;

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
// static TextLayer *s_battery_layer;
static TextLayer *s_weather_layer;

// Battery
static Layer *s_battery_layer;
static int s_battery_level;

// Bluetooth
static BitmapLayer *s_bt_icon_layer;
static GBitmap *s_bt_icon_bitmap;

// Unobstructed area
static Layer *s_window_layer;

// Set default settings
static void prv_default_settings() {
  // use fahrenheit
  settings.TemperatureUnit = false;
  settings.WeatherCheckRate = 30;
  settings.ShowDate = true;
  settings.BackgroundColor = GColorBlack;
  settings.WeatherBackgroundColor = GColorWhite;
  settings.TimeColor = GColorWhite;
  settings.DateColor = GColorYellow;
  settings.WeatherColor = GColorCadetBlue;
}

// Save settings to persistent storage
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

// Read settings from persistent storage
static void prv_load_settings() {
  // Set defaults first
  prv_default_settings();
  // Then override with any saved values
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

// Apply settings to UI elements
static void prv_update_display() {
  // Set background color
//   window_set_background_color(s_main_window, settings.BackgroundColor);
  text_layer_set_background_color(s_time_layer, settings.BackgroundColor);
  text_layer_set_background_color(s_date_layer, settings.BackgroundColor);
  text_layer_set_background_color(s_weather_layer, settings.WeatherBackgroundColor);

  // Set text colors
  text_layer_set_text_color(s_time_layer, settings.TimeColor);
  text_layer_set_text_color(s_date_layer, settings.DateColor);
  text_layer_set_text_color(s_weather_layer, settings.WeatherColor);

  // Show/hide date based on setting
  layer_set_hidden(text_layer_get_layer(s_date_layer), !settings.ShowDate);

  // Mark battery layer for redraw (color may have changed)
  layer_mark_dirty(s_battery_layer);
}

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a, %b %d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();

  // Get weather update every 30 minutes
  if (tick_time->tm_min % 30 == 0) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

static void battery_callback(BatteryChargeState state) {
//   static char battery_text[15];
  
//   if (state.is_charging) {
//     snprintf(battery_text, sizeof(battery_text), "charging");
//   } else {
//     snprintf(battery_text, sizeof(battery_text), "Batt: %d%%", state.charge_percent);
//   }
//   text_layer_set_text(s_battery_layer, battery_text);
  
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Find the width of the bar (inside the border)
  int bar_width = ((s_battery_level * (bounds.size.w - 4)) / 100);

  // Draw the border using the text color
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, bounds, 2);

  // Choose color based on battery level
  GColor bar_color;
  if (s_battery_level <= 20) {
    bar_color = PBL_IF_COLOR_ELSE(GColorRed, GColorWhite);
  } else if (s_battery_level <= 40) {
    bar_color = PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite);
  } else {
    bar_color = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
  }

  // Draw the filled bar inside the border
  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(ctx, GRect(2, 2, bar_width, bounds.size.h - 4), 1, GCornerNone);
}

static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);

  if (!connected) {
    vibes_double_pulse();
  }
}

// AppMessage received handler
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Check for weather data
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);

  if (temp_tuple && conditions_tuple) {
    static char temperature_buffer[8];
    static char weather_layer_buffer[42];

    int temp_value = (int)temp_tuple->value->int32;

    // Convert to Fahrenheit if setting is enabled
    if (settings.TemperatureUnit) {
      temp_value = (temp_value * 9 / 5) + 32;
      snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", temp_value);
    } else {
      snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", temp_value);
    }

    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", temperature_buffer);
    text_layer_set_text(s_weather_layer, weather_layer_buffer);
  }

  // Check for Clay settings data
  Tuple *bg_color_t = dict_find(iterator, MESSAGE_KEY_BackgroundColor);
  if (bg_color_t) {
    settings.BackgroundColor = GColorFromHEX(bg_color_t->value->int32);
  }
  
  Tuple *w_bg_color_t = dict_find(iterator, MESSAGE_KEY_WeatherBackgroundColor);
  if (w_bg_color_t) {
    settings.WeatherBackgroundColor = GColorFromHEX(w_bg_color_t->value->int32);
  }

  Tuple *time_color_t = dict_find(iterator, MESSAGE_KEY_TimeColor);
  if (time_color_t) {
    settings.TimeColor = GColorFromHEX(time_color_t->value->int32);
  }

  Tuple *date_color_t = dict_find(iterator, MESSAGE_KEY_DateColor);
  if (date_color_t) {
    settings.DateColor = GColorFromHEX(date_color_t->value->int32);
  }

  Tuple *temp_color_t = dict_find(iterator, MESSAGE_KEY_WeatherColor);
  if (temp_color_t) {
    settings.WeatherColor = GColorFromHEX(temp_color_t->value->int32);
  }

  Tuple *temp_unit_t = dict_find(iterator, MESSAGE_KEY_TemperatureUnit);
  if (temp_unit_t) {
    settings.TemperatureUnit = temp_unit_t->value->int32 == 1;
  }

  Tuple *show_date_t = dict_find(iterator, MESSAGE_KEY_ShowDate);
  if (show_date_t) {
    settings.ShowDate = show_date_t->value->int32 == 1;
  }

  // Save and apply if any settings were changed
  if (bg_color_t || w_bg_color_t || time_color_t || date_color_t || temp_color_t || temp_unit_t || show_date_t) {
    prv_save_settings();
    prv_update_display();

    // Refetch weather if the temperature unit changed so the display updates
    if (temp_unit_t) {
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
      app_message_outbox_send();
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

// Unobstructed area handlers
static void prv_unobstructed_will_change(GRect final_unobstructed_screen_area, void *context) {
  // Hide BT icon during the transition to reduce clutter
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), true);
  // Hide Weather during the transition to reduce clutter 
  // layer_set_hidden(text_layer_get_layer(s_weather_layer), true);
}

static void prv_unobstructed_change(AnimationProgress progress, void *context) {
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);

  // Reposition time, date, and battery to fit in the available space
  int time_height = 60;
  int date_height = 30;
  int bar_height = 12;
  int block_height = time_height + date_height + bar_height;
  int time_y = ((bounds.size.h / 3) * 2) - block_height;
  int date_y = time_y + 42;

  GRect time_frame = layer_get_frame(text_layer_get_layer(s_time_layer));
  time_frame.origin.y = time_y;
  layer_set_frame(text_layer_get_layer(s_time_layer), time_frame);

  GRect date_frame = layer_get_frame(text_layer_get_layer(s_date_layer));
  date_frame.origin.y = date_y;
  layer_set_frame(text_layer_get_layer(s_date_layer), date_frame);
}

static void prv_unobstructed_did_change(void *context) {
  GRect full_bounds = layer_get_bounds(s_window_layer);
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  bool bt_obstructed = !grect_equal(&full_bounds, &bounds);

  // Keep BT icon hidden when obstructed, otherwise restore based on connection
  if (bt_obstructed) {
    layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), true);
  } else {
    layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer),
      connection_service_peek_pebble_app_connection());
  }
  
//   int weather_height = 54;
//   int weather_y = bounds.size.h - weather_height;
//   GRect weather_frame = layer_get_frame(text_layer_get_layer(s_weather_layer));
//   weather_frame.origin.y = weather_y;
//   layer_set_frame(text_layer_get_layer(s_weather_layer), weather_frame);
  
//   bool w_obstructed = !grect_equal(&full_bounds, &bounds);
//   if (w_obstructed) {
//     layer_set_hidden(text_layer_get_layer(s_weather_layer), true);
//   } else {
//     layer_set_hidden(text_layer_get_layer(s_weather_layer), false);
//   }
}

static void main_window_load(Window *window) {
  s_window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(s_window_layer);

  // Date + Time + Battery block = 2/3 screen size
  int time_height = 60;
  int date_height = 30;
  int bar_height = 12;
  int block_height = time_height + date_height + bar_height;
  int time_y = ((bounds.size.h / 3) * 2) - block_height;
//   int date_y = time_y + time_height;
  int date_y = time_y + 42;

  // Time
  s_time_layer = text_layer_create(GRect(0, time_y, bounds.size.w, time_height));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Date
  s_date_layer = text_layer_create(GRect(0, date_y, bounds.size.w, date_height));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  // Create battery meter Layer
  int bar_width = bounds.size.w / 2;
  int bar_x = (bounds.size.w - bar_width) / 2;
  int bar_y = (date_y + date_height) + 6;
  s_battery_layer = layer_create(GRect(bar_x, bar_y, bar_width, bar_height));
  layer_set_update_proc(s_battery_layer, battery_update_proc);

  // Weather
  int weather_height = bounds.size.h / 3;
  int weather_y = (bounds.size.h - weather_height);
  s_weather_layer = text_layer_create(GRect(0, weather_y, bounds.size.w, weather_height));
  text_layer_set_font(s_weather_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "...");

  // Create the Bluetooth icon GBitmap
  s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ICON);
  int bt_y = bar_y + 12;
  s_bt_icon_layer = bitmap_layer_create(GRect((bounds.size.w - 30) / 2, bt_y, 30, 30));
  bitmap_layer_set_bitmap(s_bt_icon_layer, s_bt_icon_bitmap);
  bitmap_layer_set_compositing_mode(s_bt_icon_layer, GCompOpSet);

  // Add layers to the Window
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_weather_layer));
  layer_add_child(s_window_layer, s_battery_layer);
  layer_add_child(s_window_layer, bitmap_layer_get_layer(s_bt_icon_layer));

  // Apply saved settings
  prv_update_display();

  // Apply correct layout in case Quick View is already active
  prv_unobstructed_change(0, NULL);
  prv_unobstructed_did_change(NULL);

  // Subscribe to unobstructed area events
  UnobstructedAreaHandlers handlers = {
    .will_change = prv_unobstructed_will_change,
    .change = prv_unobstructed_change,
    .did_change = prv_unobstructed_did_change
  };
  unobstructed_area_service_subscribe(handlers, NULL);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
  layer_destroy(s_battery_layer);
  gbitmap_destroy(s_bt_icon_bitmap);
  bitmap_layer_destroy(s_bt_icon_layer);
}

static void init() {
  // Load settings before creating UI
  prv_load_settings();

  s_main_window = window_create();
  window_set_background_color(s_main_window, settings.BackgroundColor);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time();

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });

  // Register AppMessage callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  const int inbox_size = 256;
  const int outbox_size = 256;
  app_message_open(inbox_size, outbox_size);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}