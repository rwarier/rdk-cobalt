<!-- Generated automatically, DO NOT EDIT! -->
<a name="head.Cobalt_Plugin"></a>
# Cobalt Plugin

**Version: 1.0**

**Status: :black_circle::black_circle::white_circle:**

Cobalt plugin for Thunder framework.

### Table of Contents

- [Introduction](#head.Introduction)
- [Description](#head.Description)
- [Configuration](#head.Configuration)
- [Methods](#head.Methods)
- [Properties](#head.Properties)
- [Notifications](#head.Notifications)

<a name="head.Introduction"></a>
# Introduction

<a name="head.Scope"></a>
## Scope

This document describes purpose and functionality of the Cobalt plugin. It includes detailed specification about its configuration, methods and properties provided, as well as notifications sent.

<a name="head.Case_Sensitivity"></a>
## Case Sensitivity

All identifiers of the interfaces described in this document are case-sensitive. Thus, unless stated otherwise, all keywords, entities, properties, relations and actions should be treated as such.

<a name="head.Acronyms,_Abbreviations_and_Terms"></a>
## Acronyms, Abbreviations and Terms

The table below provides and overview of acronyms used in this document and their definitions.

| Acronym | Description |
| :-------- | :-------- |
| <a name="acronym.API">API</a> | Application Programming Interface |
| <a name="acronym.HTTP">HTTP</a> | Hypertext Transfer Protocol |
| <a name="acronym.JSON">JSON</a> | JavaScript Object Notation; a data interchange format |
| <a name="acronym.JSON-RPC">JSON-RPC</a> | A remote procedure call protocol encoded in JSON |

The table below provides and overview of terms and abbreviations used in this document and their definitions.

| Term | Description |
| :-------- | :-------- |
| <a name="term.callsign">callsign</a> | The name given to an instance of a plugin. One plugin can be instantiated multiple times, but each instance the instance name, callsign, must be unique. |

<a name="head.References"></a>
## References

| Ref ID | Description |
| :-------- | :-------- |
| <a name="ref.HTTP">[HTTP](http://www.w3.org/Protocols)</a> | HTTP specification |
| <a name="ref.JSON-RPC">[JSON-RPC](https://www.jsonrpc.org/specification)</a> | JSON-RPC 2.0 specification |
| <a name="ref.JSON">[JSON](http://www.json.org/)</a> | JSON specification |
| <a name="ref.Thunder">[Thunder](https://github.com/WebPlatformForEmbedded/Thunder/blob/master/doc/WPE%20-%20API%20-%20WPEFramework.docx)</a> | Thunder API Reference |

<a name="head.Description"></a>
# Description

The Cobalt plugin provides web browsing functionality based on the Cobalt engine.

The plugin is designed to be loaded and executed within the Thunder framework. For more information about the framework refer to [[Thunder](#ref.Thunder)].

<a name="head.Configuration"></a>
# Configuration

The table below lists configuration options of the plugin.

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| callsign | string | Plugin instance name (default: *Cobalt*) |
| classname | string | Class name: *Cobalt* |
| locator | string | Library name: *libWPEFrameworkCobalt.so* |
| autostart | boolean | Determines if the plugin shall be started automatically along with the framework |
| configuration | object | <sup>*(optional)*</sup>  |
| configuration?.url | string | <sup>*(optional)*</sup> The URL that is loaded upon starting the browser |
| configuration?.language | string | <sup>*(optional)*</sup> POSIX-style Language(Locale) ID. Example: 'en_US' |
| configuration?.preload | boolean | <sup>*(optional)*</sup> Enable pre-loading of application |
| configuration?.autosuspenddelay | number | <sup>*(optional)*</sup> Applicable when pre-loading. Number of seconds to wait before suspending the app |
| configuration?.gstdebug | string | <sup>*(optional)*</sup> Configure GST_DEBUG environment variable, default: 'gstplayer:4,2' |
| configuration?.closurepolicy | string | <sup>*(optional)*</sup> Configures how to handle window close request. Accepted values: [suspend, quit]. Default: 'quit' |
| configuration?.systemproperties | object | <sup>*(optional)*</sup> Configure some properties queried with Starboard System API |
| configuration?.systemproperties?.modelname | string | <sup>*(optional)*</sup> The production model number of the device |
| configuration?.systemproperties?.brandname | string | <sup>*(optional)*</sup> The name of the brand under which the device is being sold |
| configuration?.systemproperties?.modelyear | string | <sup>*(optional)*</sup> The year the device was launched |
| configuration?.systemproperties?.chipsetmodelnumber | string | <sup>*(optional)*</sup> The full model number of the main platform chipset |
| configuration?.systemproperties?.firmwareversion | string | <sup>*(optional)*</sup> The production firmware version number which the device is currently running |
| configuration?.systemproperties?.integratorname | string | <sup>*(optional)*</sup> Original manufcature of the device |
| configuration?.systemproperties?.friendlyname | string | <sup>*(optional)*</sup> A friendly name for this actual device |
| configuration?.systemproperties?.devicetype | string | <sup>*(optional)*</sup> Type of the device. Possible values [SetTopBox, OverTheTopBox, TV] |

<a name="head.Methods"></a>
# Methods

The following methods are provided by the Cobalt plugin:

Cobalt interface methods:

| Method | Description |
| :-------- | :-------- |
| [deeplink](#method.deeplink) | Send a deep link to the application |


<a name="method.deeplink"></a>
## *deeplink <sup>method</sup>*

Send a deep link to the application.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | string | An application-specific link |

### Result

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| result | null |  |

### Example

#### Request

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "method": "Cobalt.1.deeplink",
    "params": ""
}
```

#### Response

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "result": null
}
```

<a name="head.Properties"></a>
# Properties

The following properties are provided by the Cobalt plugin:

StateControl interface properties:

| Property | Description |
| :-------- | :-------- |
| [state](#property.state) | Running state of the service |

Accessibility interface properties:

| Property | Description |
| :-------- | :-------- |
| [accessibility](#property.accessibility) | Accessibility settings |


<a name="property.state"></a>
## *state <sup>property</sup>*

Provides access to the running state of the service.

Also see: [statechange](#event.statechange)

### Value

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| (property) | string | Running state of the service (must be one of the following: *resumed*, *suspended*) |

### Example

#### Get Request

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "method": "Cobalt.1.state"
}
```

#### Get Response

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "result": "resumed"
}
```

#### Set Request

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "method": "Cobalt.1.state",
    "params": "resumed"
}
```

#### Set Response

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "result": "null"
}
```

<a name="property.accessibility"></a>
## *accessibility <sup>property</sup>*

Provides access to the accessibility settings.

### Value

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| (property) | object | Accessibility settings |
| (property)?.closedcaptions | object | <sup>*(optional)*</sup> Platform's settings for closed captions |
| (property)?.closedcaptions.isenabled | boolean | Determines if the user has chosen to enable closed captions on their system |
| (property)?.closedcaptions?.backgroundcolor | string | <sup>*(optional)*</sup> Enum for possible closed captioning colors. (must be one of the following: *Blue*, *Black*, *Cyan*, *Green*, *Magenta*, *Red*, *White*, *Yellow*) |
| (property)?.closedcaptions?.backgroundopacity | string | <sup>*(optional)*</sup> Enum for possible closed captioning opacity percentages. (must be one of the following: *0*, *25*, *50*, *75*, *100*) |
| (property)?.closedcaptions?.characteredgestyle | string | <sup>*(optional)*</sup> Enum for possible closed captioning character edge styles. (must be one of the following: *None*, *Raised*, *Depressed*, *Uniform*, *DropShadow*) |
| (property)?.closedcaptions?.fontcolor | string | <sup>*(optional)*</sup> Enum for possible closed captioning colors. (must be one of the following: *Blue*, *Black*, *Cyan*, *Green*, *Magenta*, *Red*, *White*, *Yellow*) |
| (property)?.closedcaptions?.fontfamily | string | <sup>*(optional)*</sup> Enum for possible closed captioning font families. (must be one of the following: *Casual*, *Cursive*, *MonospaceSansSerif*, *MonospaceSerif*, *ProportionalSansSerif*, *ProportionalSerif*, *SmallCapitals*) |
| (property)?.closedcaptions?.fontopacity | string | <sup>*(optional)*</sup> Enum for possible closed captioning opacity percentages. (must be one of the following: *0*, *25*, *50*, *75*, *100*) |
| (property)?.closedcaptions?.fontsize | string | <sup>*(optional)*</sup> Enum for possible closed captioning font size percentages. (must be one of the following: *25*, *50*, *75*, *100*, *125*, *150*, *175*, *200*, *225*, *250*, *275*, *300*) |
| (property)?.closedcaptions?.windowcolor | string | <sup>*(optional)*</sup> Enum for possible closed captioning colors. (must be one of the following: *Blue*, *Black*, *Cyan*, *Green*, *Magenta*, *Red*, *White*, *Yellow*) |
| (property)?.closedcaptions?.windowopacity | string | <sup>*(optional)*</sup> Enum for possible closed captioning opacity percentages. (must be one of the following: *0*, *25*, *50*, *75*, *100*) |
| (property)?.textdisplay | object | <sup>*(optional)*</sup> Text display settings |
| (property)?.textdisplay.ishighcontrasttextenabled | boolean | Whether the high contrast text setting is enabled or not |

### Example

#### Get Request

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "method": "Cobalt.1.accessibility"
}
```

#### Get Response

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "result": {
        "closedcaptions": {
            "isenabled": false,
            "backgroundcolor": "Blue",
            "backgroundopacity": "0",
            "characteredgestyle": "None",
            "fontcolor": "Blue",
            "fontfamily": "Casual",
            "fontopacity": "0",
            "fontsize": "25",
            "windowcolor": "Blue",
            "windowopacity": "0"
        },
        "textdisplay": {
            "ishighcontrasttextenabled": false
        }
    }
}
```

#### Set Request

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "method": "Cobalt.1.accessibility",
    "params": {
        "closedcaptions": {
            "isenabled": false,
            "backgroundcolor": "Blue",
            "backgroundopacity": "0",
            "characteredgestyle": "None",
            "fontcolor": "Blue",
            "fontfamily": "Casual",
            "fontopacity": "0",
            "fontsize": "25",
            "windowcolor": "Blue",
            "windowopacity": "0"
        },
        "textdisplay": {
            "ishighcontrasttextenabled": false
        }
    }
}
```

#### Set Response

```json
{
    "jsonrpc": "2.0",
    "id": 1234567890,
    "result": "null"
}
```

<a name="head.Notifications"></a>
# Notifications

Notifications are autonomous events, triggered by the internals of the implementation, and broadcasted via JSON-RPC to all registered observers. Refer to [[Thunder](#ref.Thunder)] for information on how to register for a notification.

The following events are provided by the Cobalt plugin:

Cobalt interface events:

| Event | Description |
| :-------- | :-------- |
| [closure](#event.closure) | Triggered when app requests to close its window |

StateControl interface events:

| Event | Description |
| :-------- | :-------- |
| [statechange](#event.statechange) | Signals a state change of the service |


<a name="event.closure"></a>
## *closure <sup>event</sup>*

Triggered when app requests to close its window.

### Parameters

This event carries no parameters.

### Example

```json
{
    "jsonrpc": "2.0",
    "method": "client.events.1.closure"
}
```

<a name="event.statechange"></a>
## *statechange <sup>event</sup>*

Signals a state change of the service.

### Parameters

| Name | Type | Description |
| :-------- | :-------- | :-------- |
| params | object |  |
| params.suspended | boolean | Determines if the service has entered suspended state (true) or resumed state (false) |

### Example

```json
{
    "jsonrpc": "2.0",
    "method": "client.events.1.statechange",
    "params": {
        "suspended": false
    }
}
```

