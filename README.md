ESP32 + NRF24L01+ ESPHOME library to control Levolor blinds

This is a library known to work with ESP32 and NRF24L01+. It allows direct control of Levolor blinds by replaying stored remote codes. The easiest way to use this is to install the library and open the Directolor example. You can then clone one of the Directolor remotes to a remote you own. Then you can program your shades to the new cloned remote or using Directolor and control the blinds with either Directolor or your own remote.

This differs from Wevolor (https://wevolor.com/) in that you DON'T need a Levolor 6 button remote.

Typical usage:
  Install the solution to your ESP32 by including the example below and modifying to fit your needs.  Don't worry about the radio_code just yet.

  Press the stop button on your existing remote a few times

  View the ESPHome device logs and look for something like: 
```
15:51:57	[I]	[directolor_radio:063]	Found Remote with address: [0x11, 0x11, 0x38, 0x28]
15:51:57	[I]	[directolor_radio:205]	Now listening for 3-byte payloads on address: [C0, 11, 11]
15:51:57	[I]	[directolor_radio:157]	Received Stop from: 11:11:38:28
15:52:00	[I]	[directolor_radio:157]	Received Close from: 11:11:38:28
15:52:01	[I]	[directolor_radio:157]	Received Open from: 11:11:38:28
15:52:02	[I]	[directolor_radio:157]	Received Tilt Close from: 11:11:38:28
```
Copy the bytes from your remote into the radio_code below, keeping the same position.  For the example above, you'd use:
```
radio_code: [0x11, 0x11, 0x38, 0x28]
```

You've now cloned your remote into directolor.  Test that you can control your shades via the web interface buttons. 

You can include multiple "- path: directolor_cover.yaml" sections, you will typically have one for each shade.<br>
For the vars: <br>
&emsp;<b>id</b> is any valid ESPHome id<br>
&emsp;<b>name</b> is the name you'd like it to have<br>
&emsp;<b>radio_code</b> - see above or just put random values (hex) in here and join your blinds to it<br>
&emsp;<b>channel</b> - the number of the blind on your remote<br>
&emsp;<b>movement_duration</b> - a time value that tells how long it takes to fully open / close the blind.  If you leave this out, you will NOT get positional control of your blind, just open / close<br>
&emsp;<b>tilt_supported</b> - whether or not the blind has tilt functions<br>
&emsp;<b>disable_favorite_support</b> - setting this to true will mark the "to favorite" and "set favorite" buttons as disabled in Home Assistant (you can enable them manually via the HA UI) and exclude them from the ESPHome webserver if you are using version 3<br>
&emsp;<b>disable_program_support</b> - setting this to true will mark the "duplicate", "join" and "remove" buttons as disabled in Home Assistant (you can enable them manually via the HA UI) and exclude them from the ESPHome webserver if you are using version 3<br>

The ce_pin and cs_pin will match your physical hardware connections.  Use what I have provided unless you know what you're doing for pins.  If you change them, make sure to specify them for each vars section<br>
&emsp;<b>nrf24_ce_pin</b> - defaults to 22 if not provided<br>
&emsp;<b>nrf24_cs_pin</b> - defaults to 21 if not provided<br>

```yaml
# example configuration:

  directolor:
    url:  https://github.com/loucks1/Directolor-ESPHome
    ref: feature/nrf24-espidf-integration
    refresh: 1s
    files:
      - path: plumbing.yaml
      - path: directolor_cover.yaml
        vars:
          id: test_blind
          name: Test Blind
          radio_code:   [0x11, 0x11, 0x38, 0x28]
          tilt_supported: true
          channel: 1
          disable_favorite_support: true
          disable_program_support: true
          radio_hub_id: my_directolor_radio 
      - path: directolor_cover.yaml
        vars:
          id: example_blind
          name: Example Blind
          radio_code:   [0x11, 0x11, 0x38, 0x28]
          tilt_supported: false
          channel: 2
          movement_duration: 17s
          disable_favorite_support: true
          disable_program_support: true
          radio_hub_id: my_directolor_radio 
```



To connect the ESP32 to the NRF24L01+ connect:
<br>(Some have recommended a 10uF cap across ground and 3.3V - I haven't needed the cap.)
<table>
  <tr>
    <th>ESP32 pin</th>
    <th>rf24 pin</th>
  </tr>
  <tr>
    <td>ground</td>
    <td>1</td>
  </tr>
  <tr>
    <td>3.3v</td>
    <td>2</td>
  </tr>
  <tr>
    <td>22</td>
    <td>3 (CE)</td>
  </tr>
  <tr>
    <td>21</td>
    <td>4 (CSN)</td>
  </tr>
  <tr>
    <td>18</td>
    <td>5 (SCK)</td>
  </tr>
  <tr>
    <td>23</td>
    <td>6 (MOSI)</td>
  </tr>
    <tr>
    <td>19</td>
    <td>7 (MISO)</td>
  </tr>
</table>
