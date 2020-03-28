# ESP8266 Lights
This code manages an ESP8266 driving a WS2812B Addressable LED Strip. The number of lights and their color can be updated using AWS IoT.

## Features
* Offline updates with AWS IoT Device Shadow

## Communication

Update the lights indirectly by communicating with the AWS IoT Device Shadow.

Update topic: `$aws/things/led-lightstrip-1/shadow/update`

```
{
  "state": {
    "desired": {
      "color": "FFFFFF",
      "number": 10
    }
  }
} 
```