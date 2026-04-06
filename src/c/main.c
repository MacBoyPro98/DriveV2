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
  int ChargingBlinkRate; // milliseconds between blinks (default 1000)
  bool BatteryTextMode; // false = bar (default), true = text "Batt: XX%"
  bool DarkMode; // B&W only: true = black bg/white text, false = inverted
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
static TextLayer *s_battery_text_layer;
static int s_battery_level;
static bool s_battery_charging;
static bool s_charge_blink_on;
static int s_charge_dot_count; // 0-2 for sequential dot animation in text mode
static AppTimer *s_charge_timer;

// Weather background
static Layer *s_weather_bg_layer;

// Bluetooth
static BitmapLayer *s_bt_icon_layer;
static GBitmap *s_bt_icon_bitmap;

// Unobstructed area
static Layer *s_window_layer;

// Forward declarations
static void prv_update_bt_display(bool connected);
static void prv_update_battery_text(void);

// Set default settings
static void prv_default_settings() {
  // false = fahrenheit, true = celcius
  settings.TemperatureUnit = false;
  settings.WeatherCheckRate = 30;
  settings.ShowDate = true;
  settings.BackgroundColor = GColorBlack;
  settings.WeatherBackgroundColor = GColorWhite;
  settings.TimeColor = GColorDarkCandyAppleRed;
  settings.DateColor = GColorYellow;
  settings.WeatherColor = GColorCadetBlue;
  settings.ChargingBlinkRate = 1000;
  settings.BatteryTextMode = false;
  settings.DarkMode = true;
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
  // B&W colors derived from DarkMode setting
  GColor bw_bg = settings.DarkMode ? GColorBlack : GColorWhite;
  GColor bw_fg = settings.DarkMode ? GColorWhite : GColorBlack;

  window_set_background_color(s_main_window, PBL_IF_COLOR_ELSE(settings.BackgroundColor, bw_bg));
  // Set background color
  text_layer_set_background_color(s_time_layer, PBL_IF_COLOR_ELSE(settings.BackgroundColor, bw_bg));
  text_layer_set_background_color(s_date_layer, PBL_IF_COLOR_ELSE(settings.BackgroundColor, bw_bg));
  text_layer_set_background_color(s_weather_layer, GColorClear);
  layer_mark_dirty(s_weather_bg_layer);

  // Set text colors
  text_layer_set_text_color(s_time_layer, PBL_IF_COLOR_ELSE(settings.TimeColor, bw_fg));
  text_layer_set_text_color(s_date_layer, PBL_IF_COLOR_ELSE(settings.DateColor, bw_fg));
  text_layer_set_text_color(s_weather_layer, PBL_IF_COLOR_ELSE(settings.WeatherColor, bw_bg));

  // Show/hide date based on setting
  layer_set_hidden(text_layer_get_layer(s_date_layer), !settings.ShowDate);

  // Show/hide battery layers based on mode
  if (settings.BatteryTextMode) {
    layer_set_hidden(s_battery_layer, true);
    layer_set_hidden(text_layer_get_layer(s_battery_text_layer), false);
    prv_update_battery_text();
    text_layer_set_background_color(s_battery_text_layer, GColorClear);
  } else {
    layer_set_hidden(s_battery_layer, false);
    layer_set_hidden(text_layer_get_layer(s_battery_text_layer), true);
    layer_mark_dirty(s_battery_layer);
  }
}

static void prv_update_battery_text() {
  static char s_batt_text_buf[16]; // "Batt: 100%..." = 13 chars + null
  if (s_battery_charging) {
    const char *dots[] = {".", "..", "..."};
    snprintf(s_batt_text_buf, sizeof(s_batt_text_buf), "Batt: %d%%%s",
             s_battery_level, dots[s_charge_dot_count]);
  } else {
    snprintf(s_batt_text_buf, sizeof(s_batt_text_buf), "Batt: %d%%", s_battery_level);
  }
  text_layer_set_text(s_battery_text_layer, s_batt_text_buf);

  // Match text color to battery bar colors
  GColor bw_fg = settings.DarkMode ? GColorWhite : GColorBlack;
  GColor text_color;
  if (s_battery_level <= 20) {
    text_color = PBL_IF_COLOR_ELSE(GColorRed, bw_fg);
  } else if (s_battery_level <= 40) {
    text_color = PBL_IF_COLOR_ELSE(GColorChromeYellow, bw_fg);
  } else {
    text_color = PBL_IF_COLOR_ELSE(GColorGreen, bw_fg);
  }
  text_layer_set_text_color(s_battery_text_layer, text_color);
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
  if (tick_time->tm_min % settings.WeatherCheckRate == 0) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

static void charge_blink_timer_callback(void *data) {
  if (settings.BatteryTextMode) {
    s_charge_dot_count = (s_charge_dot_count + 1) % 3;
    prv_update_battery_text();
  } else {
    s_charge_blink_on = !s_charge_blink_on;
    layer_mark_dirty(s_battery_layer);
  }
  s_charge_timer = app_timer_register(settings.ChargingBlinkRate, charge_blink_timer_callback, NULL);
}

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;

  if (state.is_charging && !s_battery_charging) {
    s_battery_charging = true;
    s_charge_blink_on = true;
    s_charge_dot_count = 0;
    s_charge_timer = app_timer_register(settings.ChargingBlinkRate, charge_blink_timer_callback, NULL);
  } else if (!state.is_charging && s_battery_charging) {
    s_battery_charging = false;
    if (s_charge_timer) {
      app_timer_cancel(s_charge_timer);
      s_charge_timer = NULL;
    }
  }

  if (settings.BatteryTextMode) {
    prv_update_battery_text();
  } else {
    layer_mark_dirty(s_battery_layer);
  }
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Find the width of the bar (inside the border)
  int bar_width = ((s_battery_level * (bounds.size.w - 4)) / 100);

  // B&W foreground color based on DarkMode
  GColor bw_fg = settings.DarkMode ? GColorWhite : GColorBlack;

  // Draw the border using the foreground color
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, bw_fg));
  graphics_draw_round_rect(ctx, bounds, 2);

  // Choose color based on battery level
  GColor bar_color;
  if (s_battery_level <= 20) {
    bar_color = PBL_IF_COLOR_ELSE(GColorRed, bw_fg);
  } else if (s_battery_level <= 40) {
    bar_color = PBL_IF_COLOR_ELSE(GColorChromeYellow, bw_fg);
  } else {
    bar_color = PBL_IF_COLOR_ELSE(GColorGreen, bw_fg);
  }

  // Draw the filled bar inside the border
  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(ctx, GRect(2, 2, bar_width, bounds.size.h - 4), 1, GCornerNone);

  // Draw blinking "remaining" portion when charging
  if (s_battery_charging && s_charge_blink_on) {
    int remaining_x = 2 + bar_width;
    int remaining_w = (bounds.size.w - 4) - bar_width;
    if (remaining_w > 0) {
      graphics_fill_rect(ctx, GRect(remaining_x, 2, remaining_w, bounds.size.h - 4), 1, GCornerNone);
    }
  }
}

static void weather_bg_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GColor bw_fg = settings.DarkMode ? GColorWhite : GColorBlack;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(settings.WeatherBackgroundColor, bw_fg));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void bluetooth_callback(bool connected) {
  prv_update_bt_display(connected);

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
    settings.TimeColor = PBL_IF_COLOR_ELSE(GColorFromHEX(time_color_t->value->int32), GColorWhite);
  }

  Tuple *date_color_t = dict_find(iterator, MESSAGE_KEY_DateColor);
  if (date_color_t) {
    settings.DateColor = PBL_IF_COLOR_ELSE(GColorFromHEX(date_color_t->value->int32), GColorWhite);
  }

  Tuple *temp_color_t = dict_find(iterator, MESSAGE_KEY_WeatherColor);
  if (temp_color_t) {
    settings.WeatherColor = PBL_IF_COLOR_ELSE(GColorFromHEX(temp_color_t->value->int32), GColorBlack);
  }

  Tuple *temp_unit_t = dict_find(iterator, MESSAGE_KEY_TemperatureUnit);
  if (temp_unit_t) {
    settings.TemperatureUnit = temp_unit_t->value->int32 == 1;
  }

  Tuple *show_date_t = dict_find(iterator, MESSAGE_KEY_ShowDate);
  if (show_date_t) {
    settings.ShowDate = show_date_t->value->int32 == 1;
  }

  Tuple *blink_rate_t = dict_find(iterator, MESSAGE_KEY_ChargingBlinkRate);
  if (blink_rate_t) {
    settings.ChargingBlinkRate = (int)blink_rate_t->value->int32;
  }

  Tuple *dark_mode_t = dict_find(iterator, MESSAGE_KEY_DarkMode);
  if (dark_mode_t) {
    settings.DarkMode = dark_mode_t->value->int32 == 1;
  }

  Tuple *batt_text_t = dict_find(iterator, MESSAGE_KEY_BatteryTextMode);
  if (batt_text_t) {
    settings.BatteryTextMode = batt_text_t->value->int32 == 1;
  }

  // Save and apply if any settings were changed
  if (bg_color_t || w_bg_color_t || time_color_t || date_color_t || temp_color_t || temp_unit_t || show_date_t || blink_rate_t || dark_mode_t || batt_text_t) {
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

// Shift battery bar left and show BT icon when disconnected,
// or center battery bar and hide BT icon when connected.
static void prv_update_bt_display(bool connected) {
  GRect bat_frame = layer_get_frame(s_battery_layer);
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  int w = bounds.size.w;
  int bar_width = bat_frame.size.w;
  int bar_h = bat_frame.size.h;
  int bar_y = bat_frame.origin.y;

  if (connected) {
    layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), true);
  } else {
    GSize icon_size = gbitmap_get_bounds(s_bt_icon_bitmap).size;
    int icon_w = icon_size.w;
    int icon_h = icon_size.h;

    if (settings.BatteryTextMode) {
      // Text layer is full-width center-aligned; just position BT icon to the right
      GRect text_frame = layer_get_frame(text_layer_get_layer(s_battery_text_layer));
      int text_h = text_frame.size.h;
      int icon_x = w - icon_w - 4;
      int icon_y = bar_y + (text_h - icon_h) / 2;
      layer_set_frame(bitmap_layer_get_layer(s_bt_icon_layer),
                      GRect(icon_x, icon_y, icon_w, icon_h));
    } else {
      // Keep battery bar centered; place BT icon just to its right
      int bar_x = (w - bar_width) / 2;
      int gap = bar_h / 3;
      int icon_x = bar_x + bar_width + gap;
      int icon_y = bar_y + (bar_h - icon_h) / 2;
      layer_set_frame(bitmap_layer_get_layer(s_bt_icon_layer),
                      GRect(icon_x, icon_y, icon_w, icon_h));
    }
    layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), false);
  }
}

// Position all layers based on available screen bounds.
// All dimensions scale proportionally from the 144x168 reference design.
static void prv_position_layers(GRect bounds) {
  int h = bounds.size.h;
  int w = bounds.size.w;
  int upper_height = (h * 2) / 3;

  // Divide upper 2/3 into 3 equal zones, one per element
  int zone_h = upper_height / 3;

  // Scale element heights relative to 168px reference height
  int time_layer_h = h * 5 / 14;    // 60 @ 168, 81 @ 228
  int date_layer_h = h * 5 / 28;    // 30 @ 168, 40 @ 228
  int bar_h        = h / 14;        // 12 @ 168, 16 @ 228

  // Center each element vertically within its zone
  int time_y = (zone_h - time_layer_h) / 2 + 10;
  int date_y = zone_h + (zone_h - date_layer_h) / 2 + 5;
  int bar_y  = 2 * zone_h + (zone_h - bar_h) / 2;

  int bar_width = w / 2;
  int bar_x = (w - bar_width) / 2;

  layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, time_y, w, time_layer_h));
  layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, date_y, w, date_layer_h));
  layer_set_frame(s_battery_layer, GRect(bar_x, bar_y, bar_width, bar_h));
  // Battery text layer: centered on bar, shifted up 5px
  int text_h = 28;
  int text_y = bar_y + (bar_h - text_h) / 2 - 5;
  layer_set_frame(text_layer_get_layer(s_battery_text_layer), GRect(0, text_y, w, text_h));

  // Weather background fills entire bottom 1/3
  int lower_height = h - upper_height;
  layer_set_frame(s_weather_bg_layer, GRect(0, upper_height, w, lower_height));

  // Center the 42px weather text vertically within the bottom 1/3
  int font_h = 42;
  int front_top_pad = 5;
  int weather_y = upper_height + (lower_height - font_h) / 2 - front_top_pad;
  layer_set_frame(text_layer_get_layer(s_weather_layer), GRect(0, weather_y, w, font_h));

  // Adjust battery/BT layout based on current connection state
  prv_update_bt_display(connection_service_peek_pebble_app_connection());
}

// Unobstructed area handlers
static void prv_unobstructed_will_change(GRect final_unobstructed_screen_area, void *context) {
  // Hide BT icon during the transition to reduce clutter
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), true);
  // Hide weather layers during the transition to reduce clutter
  layer_set_hidden(text_layer_get_layer(s_weather_layer), true);
  layer_set_hidden(s_weather_bg_layer, true);
}

static void prv_unobstructed_change(AnimationProgress progress, void *context) {
  // Use full bounds so time/date/battery stay in place during quick-view
  prv_position_layers(layer_get_bounds(s_window_layer));
}

static void prv_unobstructed_did_change(void *context) {
  GRect full_bounds = layer_get_bounds(s_window_layer);
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  bool bt_obstructed = !grect_equal(&full_bounds, &bounds);

  // Keep BT icon and weather hidden when obstructed, otherwise restore
  if (bt_obstructed) {
    prv_update_bt_display(true);
    layer_set_hidden(text_layer_get_layer(s_weather_layer), true);
    layer_set_hidden(s_weather_bg_layer, true);
  } else {
    prv_update_bt_display(connection_service_peek_pebble_app_connection());
    layer_set_hidden(text_layer_get_layer(s_weather_layer), false);
    layer_set_hidden(s_weather_bg_layer, false);
  }
}

static void main_window_load(Window *window) {
  s_window_layer = window_get_root_layer(window);

  // Create layers with temporary rects (prv_position_layers sets final positions)
  s_time_layer = text_layer_create(GRect(0, 0, 0, 0));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  s_date_layer = text_layer_create(GRect(0, 0, 0, 0));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  s_battery_layer = layer_create(GRect(0, 0, 0, 0));
  layer_set_update_proc(s_battery_layer, battery_update_proc);

  s_battery_text_layer = text_layer_create(GRect(0, 0, 0, 0));
  text_layer_set_font(s_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_battery_text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_battery_text_layer, GColorClear);

  s_weather_bg_layer = layer_create(GRect(0, 0, 0, 0));
  layer_set_update_proc(s_weather_bg_layer, weather_bg_update_proc);

  s_weather_layer = text_layer_create(GRect(0, 0, 0, 0));
  text_layer_set_font(s_weather_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text(s_weather_layer, "...");

  // Create Bluetooth icon
  s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ICON);
  s_bt_icon_layer = bitmap_layer_create(GRect(0, 0, 0, 0));
  bitmap_layer_set_bitmap(s_bt_icon_layer, s_bt_icon_bitmap);
  bitmap_layer_set_compositing_mode(s_bt_icon_layer, GCompOpSet);

  // Position all layers
  prv_position_layers(layer_get_bounds(s_window_layer));

  // Add layers to the Window (weather bg first so it draws behind text)
  layer_add_child(s_window_layer, s_weather_bg_layer);
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_weather_layer));
  layer_add_child(s_window_layer, s_battery_layer);
  layer_add_child(s_window_layer, text_layer_get_layer(s_battery_text_layer));
  layer_add_child(s_window_layer, bitmap_layer_get_layer(s_bt_icon_layer));

  // Apply saved settings
  prv_update_display();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
  layer_destroy(s_weather_bg_layer);
  layer_destroy(s_battery_layer);
  text_layer_destroy(s_battery_text_layer);
  bitmap_layer_destroy(s_bt_icon_layer);
  gbitmap_destroy(s_bt_icon_bitmap);
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

  // Subscribe to unobstructed area events (for round watches with quick view)
  UnobstructedAreaHandlers ua_handlers = {
    .will_change = prv_unobstructed_will_change,
    .change = prv_unobstructed_change,
    .did_change = prv_unobstructed_did_change
  };
  unobstructed_area_service_subscribe(ua_handlers, NULL);

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
  if (s_charge_timer) {
    app_timer_cancel(s_charge_timer);
  }
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}