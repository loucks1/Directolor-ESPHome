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

You must include the external_components: once
You must include the nrf24l01_base component once.  The ce_pin and cs_pin will match your physical hardware connections.  Use what I have provided unless you know what you're doing for pins.
You can include multiple "- platform: directolor_cover" sections, you will typically have one for each shade.
  Make sure the base value is whatever id you gave to nf24l01_base
  id is any valid ESPHome id
  name is the name you'd like it to have (set web_server: to version: 3 if it doesn't show the name in the UI)
  radio_code - see above or just put random values (hex) in here and join your blinds to it
  channel - the number of the blind on your remote
  movement_duration - a time value that tells how long it takes to fully open / close the blind.  If you leave this out, you will NOT get positional control of your blind, just open / close
  tilt_supported - whether or not the blind has tilt functions
  favorite_support - whether or not to create the buttons to set and go to the favorite position of the blind
  program_support - whether or not to create the buttons to allow join, remove and duplicate for this blind (radio_code / channel combination)

You MUST include a button in your configuration otherwise the compilation will fail with a bunch of:  "class esphome::Application' has no member named 'register_button" errors.  I've included a restart button example that you can use at the end of the example configuration below

```yaml
# example configuration:

external_components:
  source: github://loucks1/Directolor-ESPHome

nrf24l01_base:
  id: nrf24l01
  ce_pin: 22
  cs_pin: 21

cover:
  - platform: directolor_cover
    base: nrf24l01
    id: my_directolor_cover
    name: My Directolor Cover
    radio_code:   #05 06 13 04
      - 0x05
      - 0x06
      - 0x13
      - 0x04
    channel: 1
    movement_duration: 16s
    tilt_supported: true  
    favorite_support: true
    program_support: true

button:
  - platform: restart
    icon: mdi:power-cycle
    name: "ESP Reboot"
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
