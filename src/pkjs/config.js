module.exports = [
  {
    "type": "heading",
    "defaultValue": "Watchface Settings"
  },
  {
    "type": "text",
    "defaultValue": "Customize your watchface appearance and preferences."
  },
//   {
//     "type": "section",
//     "layout": ["BLACK_WHITE"],
//     "items": [
//       {
//         "type": "heading",
//         "defaultValue": "Colors"
//       },
//       {
//         "type": "toggle",
//         "messageKey": "InvertColors",
//         "label": "Invert Colors",
//         "defaultValue": false
//       }
//     ]
//   },
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
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
