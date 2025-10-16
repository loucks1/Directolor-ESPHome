ESP32 + NRF24L01+ ESPHOME library to control Levolor blinds

This is a library known to work with ESP32 and NRF24L01+. It allows direct control of Levolor blinds by replaying stored remote codes. The easiest way to use this is to install the library and open the Directolor example. You can then clone one of the Directolor remotes to a remote you own. Then you can program your shades to the new cloned remote or using Directolor and control the blinds with either Directolor or your own remote.

This differs from Wevolor (https://wevolor.com/) in that you DON'T need a Levolor 6 button remote.

Typical usage:
  Install the solution to your ESP32 by including the example below and modifying to fit your needs.  Don't worry about the radio_code just yet.

  Press the stop button on your existing remote a few times

  View the ESPHome device logs and look for something like: 
```14:26:47 [D] [nrf24l01_base:178] bytes: 20 pipe: 1: 11 00 05 CA FF FF B0 51 86 06 0F 01 00 B0 51 52 53 00 68 24 AA
14:26:47 [I] [nrf24l01_base:228] Received Stop from: 0B 65 B0 51
14:26:49 [D] [nrf24l01_base:178] bytes: 20 pipe: 1: 11 00 05 CB FF FF B0 51 86 06 10 01 00 B0 51 52 53 00 F7 BD AA
14:26:49 [I] [nrf24l01_base:228] Received Stop from: 0B 65 B0 51
```
Copy the bytes from your remote into the radio_code below, keeping the same position.  For the example above, you'd use:
```radio_code:   #0B 65 B0 51
  - 0x0B
  - 0x65
  - 0xB0
  - 0x51
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

The ce_pin and cs_pin will match your physical hardware connections.  Use what I have provided unless you know what you're doing for pins.  Specify them for each  <br>
&emsp;<b>nrf24_ce_pin</b>defaults to 22 if not provided<br>
&emsp;<b>nrf24_cs_pin</b>defaults to 21 if not provided<br>

```yaml
# example configuration:

packages:
  directolor:
    url:  https://github.com/loucks1/Directolor-ESPHome
    files:
      - path: directolor_cover.yaml
        vars:
          id: blind1
          name: Directolor Blind 1
          radio_code: 
            - 0x06
            - 0x03
            - 0x05
            - 0x12
          tilt_supported: true
          channel: 1
          disable_favorite_support: true
          disable_program_support: true
      - path: directolor_cover.yaml
        vars:
          id: blind2
          name: Directolor Blind 2
          radio_code: 
            - 0x06
            - 0x03
            - 0x05
            - 0x12
          tilt_supported: false
          channel: 2
          movement_duration: 17s
          disable_favorite_support: false
          disable_program_support: false
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
