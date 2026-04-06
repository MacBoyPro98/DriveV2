module.exports = [
  {
    "type": "heading",
    "defaultValue": "Watchface Settings"
  },
  {
    "type": "text",
    "defaultValue": "Customize your watchface appearance and preferences."
  },
  {
    "type": "section",
    "capabilities": ["BW"],
    "items": [
      {
        "type": "heading",
        "defaultValue": "Display"
      },
      {
        "type": "toggle",
        "messageKey": "DarkMode",
        "label": "Dark Mode",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "capabilities": ["COLOR"],
    "items": [
      {
        "type": "heading",
        "defaultValue": "Background Colors"
      },
      {
        "type": "color",
        "messageKey": "BackgroundColor",
        "defaultValue": "0x000000",
        "label": "Background Color"
      },
      {
          "type": "color",
          "messageKey": "WeatherBackgroundColor",
          "defaultValue": "0xFFFFFF",
          "label": "Weather Background Color"
      }
    ]
  },
  {
    "type": "section",
    "capabilities": ["COLOR"],
    "items": [
      {
        "type": "heading",
        "defaultValue": "Text Colors"
      },
      {
        "type": "color",
        "messageKey": "TimeColor",
        "defaultValue": "#AA0000",
        "label": "Time Color"
      },
      {
        "type": "color",
        "messageKey": "DateColor",
        "defaultValue": "0xFFFFAA",
        "label": "Date Color"
      },
      {
        "type": "color",
        "messageKey": "WeatherColor",
        "defaultValue": "#55AAAA",
        "label": "Weather Color"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Preferences"
      },
      {
        "type": "toggle",
        "messageKey": "TemperatureUnit",
        "label": "Use Fahrenheit",
        "defaultValue": false
      },
      {
        "type": "slider",
        "messageKey": "WeatherCheckRate",
        "label": "Weather update rate (minutes)",
        "defaultValue": 30,
        "min": 5,
        "max": 120,
        "step": 5
      },
      {
        "type": "toggle",
        "messageKey": "ShowDate",
        "label": "Show Date",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "BatteryTextMode",
        "label": "Show Battery as Text",
        "defaultValue": false
      },
      {
        "type": "slider",
        "messageKey": "ChargingBlinkRate",
        "label": "Charging blink speed (ms)",
        "defaultValue": 1000,
        "min": 200,
        "max": 2000,
        "step": 100
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
