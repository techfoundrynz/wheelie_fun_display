var e={source:`docs/wheelie-vesc-comms-api.md`,route:`reference/vesc-protocol`,title:`WFB protocol notes`,group:`Reference`,description:`VESC transport, telemetry and configuration reference for WFB.`,body:`<p>VESC communications reference for Wheelie app version 1.0.0.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th></th>
<th></th>
</tr>
</thead>
<tbody>
<tr>
<td>Package</td>
<td><code>com.thefloatlife.Wheelie</code></td>
</tr>
<tr>
<td>Version</td>
<td>1.0.0 (<code>versionCode</code> 6, iOS <code>buildNumber</code> 9)</td>
</tr>
</tbody>
</table></div>
<p>Entries marked <strong>inferred</strong> describe unconfirmed encodings or behavior and
require device verification before enabling writes. Later app or controller
versions may change the protocol behavior documented here.</p>
<hr>
<h2 id="1-package-compatibility">1. Package compatibility</h2>
<p>The app expects a compatible custom package to be installed on the controller;
it does not provide a <code>.vescpkg</code> or controller firmware installation path.</p>
<p>The app carries the custom-config schema described in §6. It can read and write
that configuration without fetching the package, but refuses unsupported
signatures with <code>"Package Version Unsupported or failed to get config"</code>.</p>
<p>There is no supported Lisp source upload path. <code>COMM_LISP_PRINT</code> replies are
explicitly discarded.</p>
<hr>
<h2 id="2-transport-ble">2. Transport — BLE</h2>
<p>The GATT profile is the standard VESC one:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Constant</th>
<th>UUID</th>
<th>Role</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>VESC_SERVICE_UUID</code></td>
<td><code>6e400001-b5a3-f393-e0a9-e50e24dcca9e</code></td>
<td>Nordic UART Service</td>
</tr>
<tr>
<td><code>VESC_CHARACTERISTICS_RX_UUID</code></td>
<td><code>6e400002-b5a3-f393-e0a9-e50e24dcca9e</code></td>
<td>write, app -&gt; board</td>
</tr>
<tr>
<td><code>VESC_CHARACTERISTICS_TX_UUID</code></td>
<td><code>6e400003-b5a3-f393-e0a9-e50e24dcca9e</code></td>
<td>notify, board -&gt; app</td>
</tr>
</tbody>
</table></div>
<p>This is byte-for-byte the NUS profile that <code>nrf51_vesc</code> and VESC Express
expose - no custom service, no renamed characteristics, no vendor base UUID.
Any tool that speaks stock VESC BLE will connect. All of this app's
customisation lives above the transport, inside <code>COMM_CUSTOM_APP_DATA</code>
payloads.</p>
<p>After <code>discoverAllServicesAndCharacteristicsForDevice</code> the app walks
<code>servicesForDevice</code> and pushes <code>'vesc'</code> into a capability array if the NUS
service is present; that tag gates binding the <code>vescMessageHandler</code> listener.</p>
<p>Device discovery filters on advertised name — helper <code>bleDefaultNameIncludesWfb</code>.</p>
<h3 id="write-path-writebufferdeviceid-packet">Write path — <code>writeBuffer(deviceId, packet)</code></h3>
<pre><code>chunkSize = manager.mtu - 3
for each chunk:
    writeCharacteristicWithoutResponseForDevice(
        deviceId, VESC_SERVICE_UUID, VESC_CHARACTERISTICS_RX_UUID,
        chunk.toString('base64'))
await Promise.all(all chunk promises)
</code></pre>
<p>Note the chunks are issued <strong>concurrently</strong> and awaited together, not
sequentially. Ordering depends on the platform BLE queue preserving submission
order; on a stack that reorders, a multi-chunk packet would arrive scrambled.</p>
<p>Android permissions requested: <code>BLUETOOTH</code>, <code>BLUETOOTH_ADMIN</code>, <code>BLUETOOTH_SCAN</code>,
<code>BLUETOOTH_CONNECT</code>, <code>ACCESS_FINE_LOCATION</code>, <code>ACCESS_COARSE_LOCATION</code>,
<code>FOREGROUND_SERVICE</code>, <code>FOREGROUND_SERVICE_CONNECTED_DEVICE</code>, <code>WAKE_LOCK</code>.
iOS declares the <code>bluetooth-central</code> background mode. There is a
<code>VescBackgroundService</code> that keeps the connection alive under a foreground
service.</p>
<hr>
<h2 id="3-framing-generatepacketpayload-canid">3. Framing — <code>generatePacket(payload, canId?)</code></h2>
<p>Standard VESC serial framing:</p>
<pre><code>short (len &lt;= 256):   0x02  len:u8       payload  crc16:u16be  0x03
long  (len &gt;  256):   0x03  len:u16be    payload  crc16:u16be  0x03
</code></pre>
<p>CAN forwarding is applied at frame-build time: if <code>canId</code> is supplied and
<code>payload[0] !== COMM_FORWARD_CAN</code>, the two bytes <code>[34, canId]</code> are prepended.
The send path (<code>writeData</code>) does the same automatically from
<code>_primaryCANDeviceId</code>, so a caller normally doesn't pass <code>canId</code> explicitly.</p>
<h3 id="crc">CRC</h3>
<p>CRC-16/CCITT (XMODEM), init <code>0x0000</code>, table-driven, byte-at-a-time:</p>
<pre><code class="language-js">crc = table[((crc &gt;&gt; 8) ^ byte) &amp; 0xFF] ^ (crc &lt;&lt; 8)   // truncated to u16
</code></pre>
<p>Matches <code>crc.c</code> in <code>vedderb/bldc</code>.</p>
<h3 id="two-framing-bugs-worth-knowing-about">Two framing bugs worth knowing about</h3>
<ol>
<li><strong>Off-by-one on the length-form boundary.</strong> The branch is <code>len &lt;= 256</code> for
the short form; upstream uses <code>len &lt;= 255</code>. A payload of exactly 256 bytes is
emitted with a single length byte of <code>256 &amp; 0xFF = 0</code>, which the firmware
will read as a zero-length packet. Payloads of exactly 256 bytes are
therefore unusable.</li>
<li><strong>No 3-byte length form.</strong> Upstream supports start byte <code>0x04</code> with a
24-bit length for payloads &gt; 65535. Not implemented here — irrelevant for
the commands this app sends, but a limit if you're reusing the code.</li>
</ol>
<h3 id="vescbuffer"><code>VescBuffer</code></h3>
<p>A reimplementation of VESC's <code>buffer.c</code>. Readers:
<code>readUInt8</code>, <code>readInt8</code>, <code>readUInt16</code>, <code>readInt16</code>, <code>readUInt32</code>, <code>readInt32</code>,
<code>readDouble16(scale)</code>, <code>readDouble32(scale)</code>, <code>readFloat32</code>,
<code>readDouble32Auto</code>, <code>readString</code>, <code>slice</code>, <code>size</code>, <code>raw</code>.
Writers mirror these, plus <code>writeDouble32_Old</code> retained for compatibility.</p>
<p><code>readDouble16(scale)</code> / <code>readDouble32(scale)</code> are the scaled-integer forms
(read a signed int, divide by scale). <code>readDouble32Auto</code> is the
float32-packed-in-uint32 form.</p>
<hr>
<h2 id="4-packet-id-enum">4. Packet ID enum</h2>
<p>127 <code>COMM_*</code> IDs are defined. <strong>IDs 0–90 and 92–101 match upstream
<code>bldc/datatypes.h</code> exactly.</strong> Beyond that this app targets a fork and the
numbering diverges. Full table:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>ID</th>
<th>Name</th>
<th></th>
<th>ID</th>
<th>Name</th>
</tr>
</thead>
<tbody>
<tr>
<td>0</td>
<td><code>COMM_FW_VERSION</code></td>
<td></td>
<td>51</td>
<td><code>COMM_GET_VALUES_SETUP_SELECTIVE</code></td>
</tr>
<tr>
<td>1</td>
<td><code>COMM_JUMP_TO_BOOTLOADER</code></td>
<td></td>
<td>52</td>
<td><code>COMM_EXT_NRF_PRESENT</code></td>
</tr>
<tr>
<td>2</td>
<td><code>COMM_ERASE_NEW_APP</code></td>
<td></td>
<td>53</td>
<td><code>COMM_EXT_NRF_ESB_SET_CH_ADDR</code></td>
</tr>
<tr>
<td>3</td>
<td><code>COMM_WRITE_NEW_APP_DATA</code></td>
<td></td>
<td>54</td>
<td><code>COMM_EXT_NRF_ESB_SEND_DATA</code></td>
</tr>
<tr>
<td>4</td>
<td><code>COMM_GET_VALUES</code></td>
<td></td>
<td>55</td>
<td><code>COMM_EXT_NRF_ESB_RX_DATA</code></td>
</tr>
<tr>
<td>5</td>
<td><code>COMM_SET_DUTY</code></td>
<td></td>
<td>56</td>
<td><code>COMM_EXT_NRF_SET_ENABLED</code></td>
</tr>
<tr>
<td>6</td>
<td><code>COMM_SET_CURRENT</code></td>
<td></td>
<td>57</td>
<td><code>COMM_DETECT_MOTOR_FLUX_LINKAGE_OPENLOOP</code></td>
</tr>
<tr>
<td>7</td>
<td><code>COMM_SET_CURRENT_BRAKE</code></td>
<td></td>
<td>58</td>
<td><code>COMM_DETECT_APPLY_ALL_FOC</code></td>
</tr>
<tr>
<td>8</td>
<td><code>COMM_SET_RPM</code></td>
<td></td>
<td>59</td>
<td><code>COMM_JUMP_TO_BOOTLOADER_ALL_CAN</code></td>
</tr>
<tr>
<td>9</td>
<td><code>COMM_SET_POS</code></td>
<td></td>
<td>60</td>
<td><code>COMM_ERASE_NEW_APP_ALL_CAN</code></td>
</tr>
<tr>
<td>10</td>
<td><code>COMM_SET_HANDBRAKE</code></td>
<td></td>
<td>61</td>
<td><code>COMM_WRITE_NEW_APP_DATA_ALL_CAN</code></td>
</tr>
<tr>
<td>11</td>
<td><code>COMM_SET_DETECT</code></td>
<td></td>
<td>62</td>
<td><code>COMM_PING_CAN</code></td>
</tr>
<tr>
<td>12</td>
<td><code>COMM_SET_SERVO_POS</code></td>
<td></td>
<td>63</td>
<td><code>COMM_APP_DISABLE_OUTPUT</code></td>
</tr>
<tr>
<td>13</td>
<td><code>COMM_SET_MCCONF</code></td>
<td></td>
<td>64</td>
<td><code>COMM_TERMINAL_CMD_SYNC</code></td>
</tr>
<tr>
<td>14</td>
<td><code>COMM_GET_MCCONF</code></td>
<td></td>
<td>65</td>
<td><code>COMM_GET_IMU_DATA</code></td>
</tr>
<tr>
<td>15</td>
<td><code>COMM_GET_MCCONF_DEFAULT</code></td>
<td></td>
<td>66</td>
<td><code>COMM_BM_CONNECT</code></td>
</tr>
<tr>
<td>16</td>
<td><code>COMM_SET_APPCONF</code></td>
<td></td>
<td>67</td>
<td><code>COMM_BM_ERASE_FLASH_ALL</code></td>
</tr>
<tr>
<td>17</td>
<td><code>COMM_GET_APPCONF</code></td>
<td></td>
<td>68</td>
<td><code>COMM_BM_WRITE_FLASH</code></td>
</tr>
<tr>
<td>18</td>
<td><code>COMM_GET_APPCONF_DEFAULT</code></td>
<td></td>
<td>69</td>
<td><code>COMM_BM_REBOOT</code></td>
</tr>
<tr>
<td>19</td>
<td><code>COMM_SAMPLE_PRINT</code></td>
<td></td>
<td>70</td>
<td><code>COMM_BM_DISCONNECT</code></td>
</tr>
<tr>
<td>20</td>
<td><code>COMM_TERMINAL_CMD</code></td>
<td></td>
<td>71</td>
<td><code>COMM_BM_MAP_PINS_DEFAULT</code></td>
</tr>
<tr>
<td>21</td>
<td><code>COMM_PRINT</code></td>
<td></td>
<td>72</td>
<td><code>COMM_BM_MAP_PINS_NRF5X</code></td>
</tr>
<tr>
<td>22</td>
<td><code>COMM_ROTOR_POSITION</code></td>
<td></td>
<td>73</td>
<td><code>COMM_ERASE_BOOTLOADER</code></td>
</tr>
<tr>
<td>23</td>
<td><code>COMM_EXPERIMENT_SAMPLE</code></td>
<td></td>
<td>74</td>
<td><code>COMM_ERASE_BOOTLOADER_ALL_CAN</code></td>
</tr>
<tr>
<td>24</td>
<td><code>COMM_DETECT_MOTOR_PARAM</code></td>
<td></td>
<td>75</td>
<td><code>COMM_PLOT_INIT</code></td>
</tr>
<tr>
<td>25</td>
<td><code>COMM_DETECT_MOTOR_R_L</code></td>
<td></td>
<td>76</td>
<td><code>COMM_PLOT_DATA</code></td>
</tr>
<tr>
<td>26</td>
<td><code>COMM_DETECT_MOTOR_FLUX_LINKAGE</code></td>
<td></td>
<td>77</td>
<td><code>COMM_PLOT_ADD_GRAPH</code></td>
</tr>
<tr>
<td>27</td>
<td><code>COMM_DETECT_ENCODER</code></td>
<td></td>
<td>78</td>
<td><code>COMM_PLOT_SET_GRAPH</code></td>
</tr>
<tr>
<td>28</td>
<td><code>COMM_DETECT_HALL_FOC</code></td>
<td></td>
<td>79</td>
<td><code>COMM_GET_DECODED_BALANCE</code></td>
</tr>
<tr>
<td>29</td>
<td><code>COMM_REBOOT</code></td>
<td></td>
<td>80</td>
<td><code>COMM_BM_MEM_READ</code></td>
</tr>
<tr>
<td>30</td>
<td><code>COMM_ALIVE</code></td>
<td></td>
<td>81</td>
<td><code>COMM_WRITE_NEW_APP_DATA_LZO</code></td>
</tr>
<tr>
<td>31</td>
<td><code>COMM_GET_DECODED_PPM</code></td>
<td></td>
<td>82</td>
<td><code>COMM_WRITE_NEW_APP_DATA_ALL_CAN_LZO</code></td>
</tr>
<tr>
<td>32</td>
<td><code>COMM_GET_DECODED_ADC</code></td>
<td></td>
<td>83</td>
<td><code>COMM_BM_WRITE_FLASH_LZO</code></td>
</tr>
<tr>
<td>33</td>
<td><code>COMM_GET_DECODED_CHUK</code></td>
<td></td>
<td>84</td>
<td><code>COMM_SET_CURRENT_REL</code></td>
</tr>
<tr>
<td>34</td>
<td><code>COMM_FORWARD_CAN</code></td>
<td></td>
<td>85</td>
<td><code>COMM_CAN_FWD_FRAME</code></td>
</tr>
<tr>
<td>35</td>
<td><code>COMM_SET_CHUCK_DATA</code></td>
<td></td>
<td>86</td>
<td><code>COMM_SET_BATTERY_CUT</code></td>
</tr>
<tr>
<td>36</td>
<td><code>COMM_CUSTOM_APP_DATA</code></td>
<td></td>
<td>87</td>
<td><code>COMM_SET_BLE_NAME</code></td>
</tr>
<tr>
<td>37</td>
<td><code>COMM_NRF_START_PAIRING</code></td>
<td></td>
<td>88</td>
<td><code>COMM_SET_BLE_PIN</code></td>
</tr>
<tr>
<td>38</td>
<td><code>COMM_GPD_SET_FSW</code></td>
<td></td>
<td>89</td>
<td><code>COMM_SET_CAN_MODE</code></td>
</tr>
<tr>
<td>39</td>
<td><code>COMM_GPD_BUFFER_NOTIFY</code></td>
<td></td>
<td>90</td>
<td><code>COMM_GET_IMU_CALIBRATION</code></td>
</tr>
<tr>
<td>40</td>
<td><code>COMM_GPD_BUFFER_SIZE_LEFT</code></td>
<td></td>
<td>92</td>
<td><code>COMM_GET_CUSTOM_CONFIG_XML</code></td>
</tr>
<tr>
<td>41</td>
<td><code>COMM_GPD_FILL_BUFFER</code></td>
<td></td>
<td>93</td>
<td><code>COMM_GET_CUSTOM_CONFIG</code></td>
</tr>
<tr>
<td>42</td>
<td><code>COMM_GPD_OUTPUT_SAMPLE</code></td>
<td></td>
<td>94</td>
<td><code>COMM_GET_CUSTOM_CONFIG_DEFAULT</code></td>
</tr>
<tr>
<td>43</td>
<td><code>COMM_GPD_SET_MODE</code></td>
<td></td>
<td>95</td>
<td><code>COMM_SET_CUSTOM_CONFIG</code></td>
</tr>
<tr>
<td>44</td>
<td><code>COMM_GPD_FILL_BUFFER_INT8</code></td>
<td></td>
<td>96</td>
<td><code>COMM_BMS_GET_VALUES</code></td>
</tr>
<tr>
<td>45</td>
<td><code>COMM_GPD_FILL_BUFFER_INT16</code></td>
<td></td>
<td>97</td>
<td><code>COMM_BMS_SET_CHARGE_ALLOWED</code></td>
</tr>
<tr>
<td>46</td>
<td><code>COMM_GPD_SET_BUFFER_INT_SCALE</code></td>
<td></td>
<td>98</td>
<td><code>COMM_BMS_SET_BALANCE_OVERRIDE</code></td>
</tr>
<tr>
<td>47</td>
<td><code>COMM_GET_VALUES_SETUP</code></td>
<td></td>
<td>99</td>
<td><code>COMM_BMS_RESET_COUNTERS</code></td>
</tr>
<tr>
<td>48</td>
<td><code>COMM_SET_MCCONF_TEMP</code></td>
<td></td>
<td>100</td>
<td><code>COMM_BMS_FORCE_BALANCE</code> ⚠</td>
</tr>
<tr>
<td>49</td>
<td><code>COMM_SET_MCCONF_TEMP_SETUP</code></td>
<td></td>
<td>100</td>
<td><code>COMM_GET_MCCONF_TEMP</code> ⚠</td>
</tr>
<tr>
<td>50</td>
<td><code>COMM_GET_VALUES_SELECTIVE</code></td>
<td></td>
<td>101</td>
<td><code>COMM_BMS_ZERO_CURRENT_OFFSET</code></td>
</tr>
</tbody>
</table></div>
<p>Continuing:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>ID</th>
<th>Name</th>
<th>Upstream</th>
<th>Note</th>
</tr>
</thead>
<tbody>
<tr>
<td>111</td>
<td><code>COMM_JUMP_TO_BOOTLOADER_HW</code></td>
<td>102</td>
<td>shifted</td>
</tr>
<tr>
<td>112</td>
<td><code>COMM_ERASE_NEW_APP_HW</code></td>
<td>103</td>
<td>shifted</td>
</tr>
<tr>
<td>113</td>
<td><code>COMM_WRITE_NEW_APP_DATA_HW</code></td>
<td>104</td>
<td>shifted</td>
</tr>
<tr>
<td>114</td>
<td><code>COMM_ERASE_BOOTLOADER_HW</code></td>
<td>105</td>
<td>shifted</td>
</tr>
<tr>
<td>115</td>
<td><code>COMM_JUMP_TO_BOOTLOADER_ALL_CAN_HW</code></td>
<td>106</td>
<td>shifted</td>
</tr>
<tr>
<td>116</td>
<td><code>COMM_ERASE_NEW_APP_ALL_CAN_HW</code></td>
<td>107</td>
<td>shifted</td>
</tr>
<tr>
<td>117</td>
<td><code>COMM_WRITE_NEW_APP_DATA_ALL_CAN_HW</code></td>
<td>108</td>
<td>shifted</td>
</tr>
<tr>
<td>118</td>
<td><code>COMM_ERASE_BOOTLOADER_ALL_CAN_HW</code></td>
<td>109</td>
<td>shifted</td>
</tr>
<tr>
<td>119</td>
<td><code>COMM_SET_ODOMETER</code></td>
<td>110</td>
<td>shifted</td>
</tr>
<tr>
<td>135</td>
<td><code>COMM_LISP_PRINT</code></td>
<td>135</td>
<td>matches</td>
</tr>
<tr>
<td>138</td>
<td><code>COMM_LISP_REPL_CMD</code></td>
<td>138</td>
<td>matches</td>
</tr>
<tr>
<td>139</td>
<td><code>COMM_LISP_STREAM_CODE</code></td>
<td>139</td>
<td>matches</td>
</tr>
<tr>
<td>149</td>
<td><code>COMM_SET_APPCONF_NO_STORE</code></td>
<td>149</td>
<td>matches</td>
</tr>
<tr>
<td>150</td>
<td><code>COMM_EBMS_STORE_CONF</code></td>
<td>—</td>
<td>vendor ext (upstream 150 is <code>COMM_GET_GNSS</code>)</td>
</tr>
<tr>
<td>151</td>
<td><code>COMM_EBMS_GET_CELLS</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>152</td>
<td><code>COMM_EBMS_GET_AUX</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>153</td>
<td><code>COMM_EBMS_GET_EXP_TEMP</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>154</td>
<td><code>COMM_EBMS_SET_MCCONF</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>155</td>
<td><code>COMM_EBMS_GET_MCCONF</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>156</td>
<td><code>COMM_EBMS_GET_MCCONF_DEFAULT</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>157</td>
<td><code>COMM_EBMS_GET_VALUES</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>161</td>
<td><code>COMM_LOCK_SETPIN</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>162</td>
<td><code>COMM_WRITE_LOCK</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
<tr>
<td>163</td>
<td><code>COMM_LOCK_STATUS</code></td>
<td>—</td>
<td>vendor ext</td>
</tr>
</tbody>
</table></div>
<h3 id="duplicate-id-100">⚠ Duplicate ID 100</h3>
<p>The enum assigns <code>COMM_GET_MCCONF_TEMP = 100</code> (upstream value is <strong>91</strong>) while
also assigning <code>COMM_BMS_FORCE_BALANCE = 100</code>. In a TypeScript numeric enum
this compiles fine but the reverse map <code>enum[100]</code> resolves to whichever was
declared last. Since IDs 92–99 and 101 all match upstream exactly, <code>91</code> is
almost certainly what was intended and <code>100</code> is a typo. If you're
interoperating, treat <code>COMM_GET_MCCONF_TEMP</code> as 91 on the wire and expect this
app to send 100.</p>
<p>Two other observations:</p>
<ul>
<li><code>COMM_GET_CUSTOM_CONFIG_XML</code> (92) is defined but never sent — the app carries
the config schema locally instead (§6).</li>
<li>A 14-element array of "movement-capable" commands is built from
<code>COMM_JUMP_TO_BOOTLOADER</code>, <code>COMM_SET_DUTY</code>, <code>COMM_SET_CURRENT</code>,
<code>COMM_SET_CURRENT_BRAKE</code>, <code>COMM_SET_RPM</code>, <code>COMM_SET_POS</code>,
<code>COMM_SET_HANDBRAKE</code>, <code>COMM_SET_DETECT</code>, <code>COMM_SET_SERVO_POS</code>, <code>COMM_REBOOT</code>,
<code>COMM_ALIVE</code>, <code>COMM_FORWARD_CAN</code>, <code>COMM_SET_CHUCK_DATA</code>,
<code>COMM_CUSTOM_APP_DATA</code> — purpose not confirmed (<em>inferred</em>: a gating or
keep-alive list).</li>
</ul>
<p>Also defined: <code>HW_TYPE_VESC = 0</code>, <code>HW_TYPE_VESC_BMS = 1</code>,
<code>HW_TYPE_CUSTOM_MODULE = 2</code>, and the full upstream <code>FAULT_CODE_*</code> set
(including <code>FAULT_CODE_FLASH_CORRUPTION</code>, <code>..._APP_CFG</code>, <code>..._MC_CFG</code>).</p>
<hr>
<h2 id="5-inbound-dispatch-processvescmessagepacketid-buffer">5. Inbound dispatch — <code>processVESCMessage(packetId, buffer)</code></h2>
<p>Only these IDs are decoded; anything else logs <code>Unhandled packet type "N"</code>.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Packet</th>
<th>Decoder</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>COMM_FW_VERSION</code></td>
<td><code>getVescFWVersion</code></td>
</tr>
<tr>
<td><code>COMM_GET_VALUES</code></td>
<td><code>getVescValues</code></td>
</tr>
<tr>
<td><code>COMM_GET_VALUES_SETUP</code></td>
<td><code>getVescValuesSetup</code></td>
</tr>
<tr>
<td><code>COMM_GET_MCCONF</code></td>
<td><code>getVescMotorConf</code></td>
</tr>
<tr>
<td><code>COMM_GET_MCCONF_DEFAULT</code></td>
<td><code>getVescMotorConfDefault</code></td>
</tr>
<tr>
<td><code>COMM_GET_APPCONF</code></td>
<td><code>getVescAppConf</code></td>
</tr>
<tr>
<td><code>COMM_GET_APPCONF_DEFAULT</code></td>
<td><code>getVescAppConfDefault</code></td>
</tr>
<tr>
<td><code>COMM_CUSTOM_APP_DATA</code></td>
<td><code>getVescAppData</code> → <code>getFloatData</code></td>
</tr>
<tr>
<td><code>COMM_GET_CUSTOM_CONFIG</code></td>
<td><code>getFloatConf</code></td>
</tr>
<tr>
<td><code>COMM_BMS_GET_VALUES</code></td>
<td><code>getVescBMSData</code></td>
</tr>
<tr>
<td><code>COMM_PING_CAN</code></td>
<td><code>getVescPingCAN</code></td>
</tr>
<tr>
<td><code>COMM_GET_IMU_DATA</code></td>
<td><code>getVescIMUData</code></td>
</tr>
<tr>
<td><code>COMM_DETECT_APPLY_ALL_FOC</code></td>
<td><code>getVescDetectAllFOCReceived</code></td>
</tr>
<tr>
<td><code>COMM_SET_MCCONF</code></td>
<td><code>getVescSetMcconf</code> (ack)</td>
</tr>
<tr>
<td><code>COMM_SET_APPCONF</code> / <code>..._NO_STORE</code></td>
<td><code>getVescSetAppconf</code> (ack)</td>
</tr>
<tr>
<td><code>COMM_PRINT</code></td>
<td>logged as <code>Terminal: …</code> <strong>and shown in a native <code>Alert</code></strong></td>
</tr>
<tr>
<td><code>COMM_LISP_PRINT</code></td>
<td>silently dropped</td>
</tr>
</tbody>
</table></div>
<p>The <code>COMM_PRINT</code> handler decodes the payload as ASCII and pops a modal alert
titled "Terminal Response:". Board-controlled text reaching a UI dialog
unfiltered is worth flagging if you care about that class of issue.</p>
<p><code>COMM_GET_VALUES_SETUP</code> and <code>COMM_BMS_GET_VALUES</code> each appear <strong>twice</strong> in the
comparison chain with identical handlers — the second branch is dead code.</p>
<h3 id="comm_fw_version-0-response"><code>COMM_FW_VERSION</code> (0) response</h3>
<pre><code>u8   major
u8   minor
str  hardware name (NUL-terminated)
u8[] uuid (remaining bytes)
</code></pre>
<h3 id="comm_get_values-4-response"><code>COMM_GET_VALUES</code> (4) response</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Field</th>
<th>Read</th>
<th>Scale</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>tempMosfet</code></td>
<td><code>readDouble16</code></td>
<td>10</td>
</tr>
<tr>
<td><code>tempMotor</code></td>
<td><code>readDouble16</code></td>
<td>10</td>
</tr>
<tr>
<td><code>currentMotor</code></td>
<td><code>readDouble32</code></td>
<td>100</td>
</tr>
<tr>
<td><code>currentBattery</code></td>
<td><code>readDouble32</code></td>
<td>100</td>
</tr>
<tr>
<td><code>id</code></td>
<td><code>readDouble32</code></td>
<td>100</td>
</tr>
<tr>
<td><code>iq</code></td>
<td><code>readDouble32</code></td>
<td>100</td>
</tr>
<tr>
<td><code>dutyCycle</code></td>
<td><code>readDouble16</code></td>
<td>1000</td>
</tr>
<tr>
<td><code>erpm</code></td>
<td><code>readDouble32</code></td>
<td>1</td>
</tr>
<tr>
<td><code>voltage</code></td>
<td><code>readDouble16</code></td>
<td>10</td>
</tr>
<tr>
<td><code>ampHoursConsumed</code></td>
<td><code>readDouble32</code></td>
<td>10000</td>
</tr>
<tr>
<td><code>ampHoursCharged</code></td>
<td><code>readDouble32</code></td>
<td>10000</td>
</tr>
<tr>
<td><code>wattHoursConsumed</code></td>
<td><code>readDouble32</code></td>
<td>10000</td>
</tr>
<tr>
<td><code>wattHoursCharged</code></td>
<td><code>readDouble32</code></td>
<td>10000</td>
</tr>
<tr>
<td><code>tachValue</code></td>
<td><code>readInt32</code></td>
<td>—</td>
</tr>
<tr>
<td><code>tachAbs</code></td>
<td><code>readInt32</code></td>
<td>—</td>
</tr>
<tr>
<td><code>faultCode</code></td>
<td><code>readInt8</code></td>
<td>—</td>
</tr>
</tbody>
</table></div>
<h3 id="comm_get_values_setup-47-response"><code>COMM_GET_VALUES_SETUP</code> (47) response</h3>
<p>Same prefix through <code>currentBattery</code>, then:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Field</th>
<th>Read</th>
<th>Scale / transform</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>dutyCycle</code></td>
<td><code>readDouble16</code></td>
<td>1000</td>
</tr>
<tr>
<td><code>erpm</code></td>
<td><code>readDouble32</code></td>
<td>1</td>
</tr>
<tr>
<td><code>speed</code></td>
<td><code>readDouble32</code></td>
<td>×3.5897435 (m/s → mph)</td>
</tr>
<tr>
<td><code>voltage</code></td>
<td><code>readDouble16</code></td>
<td>10</td>
</tr>
<tr>
<td><code>batteryLevel</code></td>
<td><code>readDouble16</code></td>
<td>1000</td>
</tr>
<tr>
<td><code>ampHoursConsumed</code> … <code>wattHoursCharged</code></td>
<td><code>readDouble32</code></td>
<td>10000</td>
</tr>
<tr>
<td><code>distance</code>, <code>distanceAbs</code> (legacy labels <code>tachValue</code>, <code>tachAbs</code>)</td>
<td><code>readDouble32</code></td>
<td>1000; metres</td>
</tr>
<tr>
<td><code>pidPos</code></td>
<td><code>readDouble32</code></td>
<td>1000000</td>
</tr>
<tr>
<td><code>faultCode</code></td>
<td><code>readInt8</code></td>
<td>—</td>
</tr>
<tr>
<td><code>controllerId</code></td>
<td><code>readInt8</code></td>
<td>—</td>
</tr>
<tr>
<td><code>numVescs</code></td>
<td><code>readInt8</code></td>
<td>—</td>
</tr>
<tr>
<td><code>wattHoursRemaining</code></td>
<td><code>readDouble32</code></td>
<td>10000</td>
</tr>
<tr>
<td><code>odometer</code></td>
<td><code>readUInt32</code></td>
<td>—</td>
</tr>
<tr>
<td><code>loopTime</code></td>
<td><code>readUInt32</code></td>
<td>—</td>
</tr>
</tbody>
</table></div>
<p>For distance reporting, the display follows the <a href="https://github.com/vedderb/bldc/blob/master/comm/commands.c">upstream VESC serializer</a>: the two setup-distance fields are scaled metres rather than raw tachometer counts. <code>distanceAbs</code> supplies the trip since controller boot; the optional unsigned <code>odometer</code> supplies lifetime metres. These common fields are used for both supported vehicle packages and generic VESC, independently of the inferred custom-package tail below.</p>
<h3 id="comm_get_imu_data-65-response"><code>COMM_GET_IMU_DATA</code> (65) response</h3>
<pre><code>u16 mask
</code></pre>
<p>Then, for each set bit in ascending order, one <code>float32</code>:
<code>0x001 roll</code>, <code>0x002 pitch</code>, <code>0x004 yaw</code>, <code>0x008 accX</code>, <code>0x010 accY</code>,
<code>0x020 accZ</code>, <code>0x040 gyroX</code>, <code>0x080 gyroY</code>, <code>0x100 gyroZ</code>, <code>0x200 magX</code>, …</p>
<h3 id="comm_ping_can-62-response"><code>COMM_PING_CAN</code> (62) response</h3>
<p><code>u8[]</code> — one byte per CAN device ID found, length = buffer size.</p>
<hr>
<h2 id="6-custom-app-data-the-wfb-package-layer">6. Custom app data — the WFB package layer</h2>
<p>This is the app-specific layer, carried inside <code>COMM_CUSTOM_APP_DATA</code> (36).</p>
<pre><code>COMM_CUSTOM_APP_DATA  appId:u8  subCommand:u8  [payload…]
</code></pre>
<p><code>AppPacketTypes.WFB = 102</code>. <code>getVescAppData</code> reads the first two bytes and
<strong>returns <code>null</code> unless <code>appId === 102</code></strong> — so this app will not talk to a stock
Float package board (which uses app ID 101) or a Refloat board.</p>
<h3 id="sub-commands-floatpackettypes">Sub-commands (<code>FloatPacketTypes</code>)</h3>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>ID</th>
<th>Name</th>
<th>Notes</th>
</tr>
</thead>
<tbody>
<tr>
<td>0</td>
<td><code>GET_INFO</code></td>
<td></td>
</tr>
<tr>
<td>1</td>
<td><code>GET_RTDATA</code></td>
<td></td>
</tr>
<tr>
<td>2</td>
<td><code>SET_TUNE</code></td>
<td></td>
</tr>
<tr>
<td>3</td>
<td><code>SET_DEFAULT_TUNE</code></td>
<td></td>
</tr>
<tr>
<td>4</td>
<td><code>SAVE_TUNE</code></td>
<td></td>
</tr>
<tr>
<td>5</td>
<td><code>RESTORE_TUNE</code></td>
<td></td>
</tr>
<tr>
<td>6</td>
<td><code>TUNE_OTHER</code></td>
<td></td>
</tr>
<tr>
<td>7</td>
<td><code>MOVE</code></td>
<td></td>
</tr>
<tr>
<td>8</td>
<td><code>BOOSTER</code></td>
<td></td>
</tr>
<tr>
<td>9</td>
<td><code>PRINT_INFO</code></td>
<td></td>
</tr>
<tr>
<td>10</td>
<td><code>GET_ALL_DATA</code></td>
<td>primary telemetry poll</td>
</tr>
<tr>
<td>11</td>
<td><code>EXPERIMENT</code></td>
<td></td>
</tr>
<tr>
<td>12</td>
<td><code>LOCK</code></td>
<td></td>
</tr>
<tr>
<td>22</td>
<td><code>FLYWHEEL</code></td>
<td></td>
</tr>
<tr>
<td>25</td>
<td><code>LIGHT_INFO</code></td>
<td></td>
</tr>
<tr>
<td>26</td>
<td><code>LIGHT_CTRL</code></td>
<td></td>
</tr>
<tr>
<td>27</td>
<td><code>LCM_INFO</code></td>
<td></td>
</tr>
<tr>
<td>100</td>
<td><code>BIKE_RIDE_CONFIGURE</code></td>
<td>vendor</td>
</tr>
<tr>
<td>101</td>
<td><code>BIKE_WHEELIE_CONFIGURE</code></td>
<td>vendor</td>
</tr>
<tr>
<td>102</td>
<td><code>BIKE_FEATURE_CONFIGURE</code></td>
<td>vendor</td>
</tr>
<tr>
<td>103</td>
<td><code>BIKE_SPEED_LIMIT_CONFIGURE</code></td>
<td>vendor</td>
</tr>
</tbody>
</table></div>
<p>IDs 0–27 track the Float package's <code>FLOAT_COMMAND_*</code> layout; 100–103 are
specific to this fork.</p>
<p>The app's telemetry loop is <code>getRTFloatData(ctx, canId)</code> →
<code>getData(COMM_CUSTOM_APP_DATA, WFB, GET_ALL_DATA, canId)</code>, and
<code>getAllFloatData</code> additionally issues a plain <code>COMM_GET_VALUES_SETUP</code>.</p>
<h3 id="get_all_data-10-response"><code>GET_ALL_DATA</code> (10) response</h3>
<p>Requires <code>buffer.size() &gt;= 35</code>, else the parse is abandoned.</p>
<pre><code>u8  mode
</code></pre>
<p>Then, always:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Field</th>
<th>Encoding</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>pidOutput</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>pitchAngle</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>rollAngle</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>state</code></td>
<td><code>u8 &amp; 0x0F</code></td>
</tr>
<tr>
<td><code>setpointAdjustmentType</code></td>
<td>same byte <code>&gt;&gt; 4</code></td>
</tr>
<tr>
<td><code>switchState</code></td>
<td><code>u8 &amp; 0x0F</code></td>
</tr>
<tr>
<td><code>adc1</code></td>
<td><code>u8 / 50</code></td>
</tr>
<tr>
<td><code>adc2</code></td>
<td><code>u8 / 50</code></td>
</tr>
<tr>
<td><code>setPoint</code></td>
<td><code>(u8 - 128) / 5</code></td>
</tr>
<tr>
<td><code>atr</code></td>
<td><code>(u8 - 128) / 5</code></td>
</tr>
<tr>
<td><code>brakeTilt</code></td>
<td><code>(u8 - 128) / 5</code></td>
</tr>
<tr>
<td><code>torqueTilt</code></td>
<td><code>(u8 - 128) / 5</code></td>
</tr>
<tr>
<td><code>turnTilt</code></td>
<td><code>(u8 - 128) / 5</code></td>
</tr>
<tr>
<td><code>inputTilt</code></td>
<td><code>(u8 - 128) / 5</code></td>
</tr>
<tr>
<td><code>truePitchAngle</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>appliedBoosterCurrent</code></td>
<td><code>u8 - 128</code></td>
</tr>
<tr>
<td><code>voltage</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>erpm</code></td>
<td><code>readInt16</code></td>
</tr>
<tr>
<td><code>speed</code></td>
<td><code>readDouble16(10)</code> × 3.5897435</td>
</tr>
<tr>
<td><code>currentMotor</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>currentBattery</code></td>
<td><code>readDouble16(10)</code></td>
</tr>
<tr>
<td><code>dutyCycle</code></td>
<td><code>(u8 - 128) / 100</code></td>
</tr>
<tr>
<td><code>id</code></td>
<td><code>u8 / 3</code></td>
</tr>
</tbody>
</table></div>
<p>After reading <code>switchState</code> the app normalises it: raw <code>2</code> → <code>FULL</code>,
raw <code>1</code> → <code>ADC1</code> or <code>ADC2</code> depending on which of <code>adc1</code>/<code>adc2</code> is larger,
anything else → <code>OFF</code>.</p>
<p>If <code>mode &gt;= 2</code>, four more fields:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Field</th>
<th>Encoding</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>tachAbs</code></td>
<td><code>readFloat32</code></td>
</tr>
<tr>
<td><code>tempMosfet</code></td>
<td><code>u8 / 2</code></td>
</tr>
<tr>
<td><code>tempMotor</code></td>
<td><code>u8 / 2</code></td>
</tr>
<tr>
<td><code>tempBattery</code></td>
<td><code>u8</code></td>
</tr>
</tbody>
</table></div>
<p>Then, unconditionally:</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Field</th>
<th>Encoding</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>odometer</code></td>
<td>(<em>inferred</em> — <code>readUInt32</code>)</td>
</tr>
<tr>
<td><code>ampHoursConsumed</code></td>
<td><code>readDouble16</code></td>
</tr>
<tr>
<td><code>ampHoursCharged</code></td>
<td><code>readDouble16</code></td>
</tr>
<tr>
<td><code>wattHoursConsumed</code></td>
<td><code>readDouble16</code></td>
</tr>
<tr>
<td><code>wattHoursCharged</code></td>
<td><code>readDouble16</code></td>
</tr>
<tr>
<td><code>batteryLevel</code></td>
<td><code>u8 / 2</code></td>
</tr>
</tbody>
</table></div>
<h3 id="enums-used-by-the-telemetry">Enums used by the telemetry</h3>
<p><strong><code>state</code></strong> (low nibble):
<code>0 BOOT</code>, <code>1 RUNNING</code>, <code>2 RUNNING_TILTBACK</code>, <code>3 RUNNING_WHEELSLIP</code>,
<code>4 RUNNING_UPSIDEDOWN</code>, <code>5 RUNNING_FLYWHEEL</code>, <code>6 STOP_ANGLE_PITCH</code>,
<code>7 STOP_ANGLE_ROLL</code>, <code>8 STOP_SWITCH_HALF</code>, <code>9 STOP_SWITCH_FULL</code>,
<code>11 STARTUP</code>, <code>12 STOP_REVERSE</code>, <code>13 STOP_QUICKSTOP</code>, <code>14 CHARGING</code>,
<code>15 DISABLED</code>. (No <code>10</code>.)</p>
<p><strong><code>setpointAdjustmentType</code></strong> (high nibble):
<code>0 CENTERING</code>, <code>1 REVERSESTOP</code>, <code>2 TILTBACK_NONE</code>, <code>3 TILTBACK_DUTY</code>,
<code>4 TILTBACK_HV</code>, <code>5 TILTBACK_LV</code>, <code>6 TILTBACK_TEMP</code>.</p>
<p><strong><code>switchState</code></strong>: <code>0 OFF</code>, <code>1 ADC1</code>, <code>2 ADC2</code>, <code>3 FULL</code>.</p>
<p><strong>LED type</strong>: <code>0 NONE</code>, <code>1 RGB</code>, <code>2 RGBW</code>, <code>3 EXTERNAL</code>.</p>
<p><strong>Ride mode</strong>: <code>0 DutyCycle</code>, <code>1 Standard</code>.</p>
<hr>
<h2 id="7-custom-config-comm_get_custom_config-comm_set_custom_config">7. Custom config — <code>COMM_GET_CUSTOM_CONFIG</code> / <code>COMM_SET_CUSTOM_CONFIG</code></h2>
<p>The custom-config interface uses the local parameter schema.</p>
<p><code>getFloatConf(buffer)</code> reads:</p>
<pre><code>u8   (discarded)
u32  signature
</code></pre>
<p>then looks the signature up in a version map. The map has exactly <strong>one entry,
version <code>1</code></strong> — so this build supports a single WFB config revision, and an
unrecognised signature yields <code>null</code>, surfacing to the user as
<code>"Package Version Unsupported or failed to get config"</code>. The signature value
itself is computed at runtime from the parameter table (VESC Tool-style hash).
Its numeric value is not specified here and must be verified before writing configuration.</p>
<p>Per version the app holds three generated artefacts:
<code>configPropertyMetadata</code> (the schema), a parser built by <code>getParser(metadata)</code>,
a serialiser built by <code>getEncoder(metadata)</code>, and <code>configPageLayout</code> for the UI.
This is exactly the code-gen output that VESC Tool produces from a package's
config XML — embedded rather than fetched.</p>
<h3 id="the-bike_config-parameter-table-27-entries">The <code>bike_config</code> parameter table (27 entries)</h3>
<p><code>vTx</code> is the wire type; <code>scale</code> is <code>vTxDoubleScale</code> (value is transmitted as an
integer of <code>value × scale</code>). Negative bounds below use their signed values.</p>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>Name</th>
<th>cDefine</th>
<th>Type</th>
<th>vTx</th>
<th>Scale</th>
<th>Range</th>
<th>Default</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>config_name</code></td>
<td>—</td>
<td>string</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td><code>bike_config</code></td>
</tr>
<tr>
<td><code>hw_name</code></td>
<td>—</td>
<td>label</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>"WFB Cfg"</td>
</tr>
<tr>
<td><code>package_disable</code></td>
<td><code>PACKAGE_DISABLE</code></td>
<td>bool</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>1</td>
</tr>
<tr>
<td><code>kp</code></td>
<td><code>WHEELIE_KP</code></td>
<td>double</td>
<td>7</td>
<td>10</td>
<td>0 – 40</td>
<td>30</td>
</tr>
<tr>
<td><code>kp2</code></td>
<td><code>WHEELIE_KP2</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>0 – 3</td>
<td>1</td>
</tr>
<tr>
<td><code>mahony_kp</code></td>
<td><code>WHEELIE_MAHONY_KP</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>0 – 3</td>
<td>2</td>
</tr>
<tr>
<td><code>kp_brake</code></td>
<td><code>WHEELIE_KP_BRAKE</code></td>
<td>double</td>
<td>7</td>
<td>10</td>
<td>0.2 – 2</td>
<td>1.5</td>
</tr>
<tr>
<td><code>kp2_brake</code></td>
<td><code>WHEELIE_KP2_BRAKE</code></td>
<td>double</td>
<td>7</td>
<td>10</td>
<td>0 – 2</td>
<td>1.5</td>
</tr>
<tr>
<td><code>max_current</code></td>
<td><code>BIKE_MAX_CURRENT</code></td>
<td>int</td>
<td>4</td>
<td>—</td>
<td>0 – 150</td>
<td>150</td>
</tr>
<tr>
<td><code>max_current_brake</code></td>
<td><code>BIKE_MAX_CURRENT_BRAKE</code></td>
<td>int</td>
<td>4</td>
<td>—</td>
<td>−150 – 0</td>
<td>−90</td>
</tr>
<tr>
<td><code>ramp_alpha_pos</code></td>
<td><code>BIKE_RAMP_ALPHA_POS</code></td>
<td>double</td>
<td>7</td>
<td>10000</td>
<td>0.0001 – 1</td>
<td>0.003</td>
</tr>
<tr>
<td><code>ramp_alpha_neg</code></td>
<td><code>BIKE_RAMP_ALPHA_NEG</code></td>
<td>double</td>
<td>7</td>
<td>10000</td>
<td>0.0001 – 1</td>
<td>0.008</td>
</tr>
<tr>
<td><code>ramp_alpha_return</code></td>
<td><code>BIKE_RAMP_ALPHA_RETURN</code></td>
<td>double</td>
<td>7</td>
<td>10000</td>
<td>0.0001 – 1</td>
<td>0.008</td>
</tr>
<tr>
<td><code>balance_pitch</code></td>
<td><code>BIKE_BALANCE_PITCH</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>5 – 55</td>
<td>30</td>
</tr>
<tr>
<td><code>button_function</code></td>
<td><code>BIKE_BUTTON_FUNCTION</code></td>
<td>enum</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>2</td>
</tr>
<tr>
<td><code>throttle_tilt_enabled</code></td>
<td>—</td>
<td>bool</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>—</td>
</tr>
<tr>
<td><code>throttle_tilt_angle_limit</code></td>
<td><code>BIKE_THROTTLE_TILT_ANGLE_LIMIT</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>−20 – 20</td>
<td>5</td>
</tr>
<tr>
<td><code>brake_tilt_angle_limit</code></td>
<td><code>BIKE_BRAKE_TILT_ANGLE_LIMIT</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>−20 – 20</td>
<td>−5</td>
</tr>
<tr>
<td><code>anti_loop_enabled</code></td>
<td><code>BIKE_ANTI_LOOP_ENABLED</code></td>
<td>bool</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>1</td>
</tr>
<tr>
<td><code>anti_loop_use_button_angle</code></td>
<td>—</td>
<td>bool</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>—</td>
</tr>
<tr>
<td><code>anti_loop_angle</code></td>
<td><code>BIKE_ANTI_LOOP_ANGLE</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>5 – 55</td>
<td>30</td>
</tr>
<tr>
<td><code>horn_strength</code></td>
<td><code>BIKE_HORN_STRENGTH</code></td>
<td>double</td>
<td>7</td>
<td>100</td>
<td>0 – 100</td>
<td>4</td>
</tr>
<tr>
<td><code>horn_frequency</code></td>
<td><code>BIKE_HORN_FREQUENCY</code></td>
<td>int</td>
<td>3</td>
<td>—</td>
<td>200 – 1000</td>
<td>494</td>
</tr>
<tr>
<td><code>lift_resistance_kp2</code></td>
<td>—</td>
<td>double</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>—</td>
</tr>
<tr>
<td><code>speed_limit</code></td>
<td><code>BIKE_SPEED_LIMIT</code></td>
<td>int</td>
<td>1</td>
<td>—</td>
<td>0 – 35</td>
<td>19</td>
</tr>
<tr>
<td><code>persist_wheelie_throttle</code></td>
<td><code>BIKE_PERSIST_WHEELIE_THROTTLE</code></td>
<td>bool</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>0</td>
</tr>
<tr>
<td><code>persist_wheelie_brake</code></td>
<td><code>BIKE_PERSIST_WHEELIE_BRAKE</code></td>
<td>bool</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>0</td>
</tr>
</tbody>
</table></div>
<p>The declared JS types for the whole set are:
<code>config_name: string</code>, <code>hw_name: undefined</code>, <code>package_disable: boolean</code>,
and the remainder <code>number</code> except the five booleans
(<code>throttle_tilt_enabled</code>, <code>anti_loop_enabled</code>, <code>anti_loop_use_button_angle</code>,
<code>persist_wheelie_throttle</code>, <code>persist_wheelie_brake</code>).</p>
<p><code>hw_name</code> carries the description "This is the VESC Logging and Communication
Module."</p>
<hr>
<h2 id="8-client-command-scheduling">8. Client command scheduling</h2>
<p>Outbound commands are matched to responses through command callbacks. While
callbacks are outstanding in serial command mode, the sender backs off in
10 ms increments before writing the next command.</p>
<hr>
<h2 id="9-summary-of-defects-found">9. Summary of defects found</h2>
<div class="table-scroll" tabindex="0"><table>
<thead>
<tr>
<th>#</th>
<th>Issue</th>
<th>Impact</th>
</tr>
</thead>
<tbody>
<tr>
<td>1</td>
<td><code>COMM_GET_MCCONF_TEMP = 100</code> (should be 91), colliding with <code>COMM_BMS_FORCE_BALANCE = 100</code></td>
<td>Wrong ID on the wire; enum reverse-map ambiguity</td>
</tr>
<tr>
<td>2</td>
<td>Short-frame boundary is <code>len &lt;= 256</code>, should be <code>&lt;= 255</code></td>
<td>256-byte payloads emit a length byte of 0</td>
</tr>
<tr>
<td>3</td>
<td><code>_HW</code> bootloader IDs shifted +9 vs upstream (111–118 vs 102–109)</td>
<td>Incompatible with stock firmware — intentional if the fork matches</td>
</tr>
<tr>
<td>4</td>
<td>BLE chunks written concurrently via <code>Promise.all</code></td>
<td>Multi-chunk packets rely on stack-preserved ordering</td>
</tr>
<tr>
<td>5</td>
<td><code>COMM_PRINT</code> payload rendered in a native <code>Alert</code> unfiltered</td>
<td>Board-controlled text reaches a UI dialog</td>
</tr>
<tr>
<td>6</td>
<td>Duplicate dead branches for <code>COMM_GET_VALUES_SETUP</code> / <code>COMM_BMS_GET_VALUES</code></td>
<td>Cosmetic</td>
</tr>
<tr>
<td>7</td>
<td>No 3-byte length frame form</td>
<td>Payloads &gt; 65535 unsupported</td>
</tr>
</tbody>
</table></div>
`,headings:[{id:`1-package-compatibility`,title:`1. Package compatibility`,level:2},{id:`2-transport-ble`,title:`2. Transport — BLE`,level:2},{id:`write-path-writebufferdeviceid-packet`,title:`Write path — writeBuffer(deviceId, packet)`,level:3},{id:`3-framing-generatepacketpayload-canid`,title:`3. Framing — generatePacket(payload, canId?)`,level:2},{id:`crc`,title:`CRC`,level:3},{id:`two-framing-bugs-worth-knowing-about`,title:`Two framing bugs worth knowing about`,level:3},{id:`vescbuffer`,title:`VescBuffer`,level:3},{id:`4-packet-id-enum`,title:`4. Packet ID enum`,level:2},{id:`duplicate-id-100`,title:`⚠ Duplicate ID 100`,level:3},{id:`5-inbound-dispatch-processvescmessagepacketid-buffer`,title:`5. Inbound dispatch — processVESCMessage(packetId, buffer)`,level:2},{id:`comm_fw_version-0-response`,title:`COMM_FW_VERSION (0) response`,level:3},{id:`comm_get_values-4-response`,title:`COMM_GET_VALUES (4) response`,level:3},{id:`comm_get_values_setup-47-response`,title:`COMM_GET_VALUES_SETUP (47) response`,level:3},{id:`comm_get_imu_data-65-response`,title:`COMM_GET_IMU_DATA (65) response`,level:3},{id:`comm_ping_can-62-response`,title:`COMM_PING_CAN (62) response`,level:3},{id:`6-custom-app-data-the-wfb-package-layer`,title:`6. Custom app data — the WFB package layer`,level:2},{id:`sub-commands-floatpackettypes`,title:`Sub-commands (FloatPacketTypes)`,level:3},{id:`get_all_data-10-response`,title:`GET_ALL_DATA (10) response`,level:3},{id:`enums-used-by-the-telemetry`,title:`Enums used by the telemetry`,level:3},{id:`7-custom-config-comm_get_custom_config-comm_set_custom_config`,title:`7. Custom config — COMM_GET_CUSTOM_CONFIG / COMM_SET_CUSTOM_CONFIG`,level:2},{id:`the-bike_config-parameter-table-27-entries`,title:`The bike_config parameter table (27 entries)`,level:3},{id:`8-client-command-scheduling`,title:`8. Client command scheduling`,level:2},{id:`9-summary-of-defects-found`,title:`9. Summary of defects found`,level:2}],search:`VESC communications reference for Wheelie app version 1.0.0. Package com.thefloatlife.Wheelie Version 1.0.0 (versionCode 6, iOS buildNumber 9) Entries marked inferred describe unconfirmed encodings or behavior and require device verification before enabling writes. Later app or controller versions may change the protocol behavior documented here. 1. Package compatibility The app expects a compatible custom package to be installed on the controller; it does not provide a .vescpkg or controller firmware installation path. The app carries the custom-config schema described in §6. It can read and write that configuration without fetching the package, but refuses unsupported signatures with "Package Version Unsupported or failed to get config". There is no supported Lisp source upload path. COMM_LISP_PRINT replies are explicitly discarded. 2. Transport — BLE The GATT profile is the standard VESC one: Constant UUID Role VESC_SERVICE_UUID 6e400001-b5a3-f393-e0a9-e50e24dcca9e Nordic UART Service VESC_CHARACTERISTICS_RX_UUID 6e400002-b5a3-f393-e0a9-e50e24dcca9e write, app -> board VESC_CHARACTERISTICS_TX_UUID 6e400003-b5a3-f393-e0a9-e50e24dcca9e notify, board -> app This is byte-for-byte the NUS profile that nrf51_vesc and VESC Express expose - no custom service, no renamed characteristics, no vendor base UUID. Any tool that speaks stock VESC BLE will connect. All of this app's customisation lives above the transport, inside COMM_CUSTOM_APP_DATA payloads. After discoverAllServicesAndCharacteristicsForDevice the app walks servicesForDevice and pushes 'vesc' into a capability array if the NUS service is present; that tag gates binding the vescMessageHandler listener. Device discovery filters on advertised name — helper bleDefaultNameIncludesWfb. Write path — writeBuffer(deviceId, packet) chunkSize = manager.mtu - 3 for each chunk: writeCharacteristicWithoutResponseForDevice( deviceId, VESC_SERVICE_UUID, VESC_CHARACTERISTICS_RX_UUID, chunk.toString('base64')) await Promise.all(all chunk promises) Note the chunks are issued concurrently and awaited together, not sequentially. Ordering depends on the platform BLE queue preserving submission order; on a stack that reorders, a multi-chunk packet would arrive scrambled. Android permissions requested: BLUETOOTH, BLUETOOTH_ADMIN, BLUETOOTH_SCAN, BLUETOOTH_CONNECT, ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION, FOREGROUND_SERVICE, FOREGROUND_SERVICE_CONNECTED_DEVICE, WAKE_LOCK. iOS declares the bluetooth-central background mode. There is a VescBackgroundService that keeps the connection alive under a foreground service. 3. Framing — generatePacket(payload, canId?) Standard VESC serial framing: short (len <= 256): 0x02 len:u8 payload crc16:u16be 0x03 long (len > 256): 0x03 len:u16be payload crc16:u16be 0x03 CAN forwarding is applied at frame-build time: if canId is supplied and payload[0] !== COMM_FORWARD_CAN, the two bytes [34, canId] are prepended. The send path (writeData) does the same automatically from _primaryCANDeviceId, so a caller normally doesn't pass canId explicitly. CRC CRC-16/CCITT (XMODEM), init 0x0000, table-driven, byte-at-a-time: crc = table[((crc >> 8) ^ byte) & 0xFF] ^ (crc << 8) // truncated to u16 Matches crc.c in vedderb/bldc. Two framing bugs worth knowing about Off-by-one on the length-form boundary. The branch is len <= 256 for the short form; upstream uses len <= 255. A payload of exactly 256 bytes is emitted with a single length byte of 256 & 0xFF = 0, which the firmware will read as a zero-length packet. Payloads of exactly 256 bytes are therefore unusable. No 3-byte length form. Upstream supports start byte 0x04 with a 24-bit length for payloads > 65535. Not implemented here — irrelevant for the commands this app sends, but a limit if you're reusing the code. VescBuffer A reimplementation of VESC's buffer.c. Readers: readUInt8, readInt8, readUInt16, readInt16, readUInt32, readInt32, readDouble16(scale), readDouble32(scale), readFloat32, readDouble32Auto, readString, slice, size, raw. Writers mirror these, plus writeDouble32_Old retained for compatibility. readDouble16(scale) / readDouble32(scale) are the scaled-integer forms (read a signed int, divide by scale). readDouble32Auto is the float32-packed-in-uint32 form. 4. Packet ID enum 127 COMM_* IDs are defined. IDs 0–90 and 92–101 match upstream bldc/datatypes.h exactly. Beyond that this app targets a fork and the numbering diverges. Full table: ID Name ID Name 0 COMM_FW_VERSION 51 COMM_GET_VALUES_SETUP_SELECTIVE 1 COMM_JUMP_TO_BOOTLOADER 52 COMM_EXT_NRF_PRESENT 2 COMM_ERASE_NEW_APP 53 COMM_EXT_NRF_ESB_SET_CH_ADDR 3 COMM_WRITE_NEW_APP_DATA 54 COMM_EXT_NRF_ESB_SEND_DATA 4 COMM_GET_VALUES 55 COMM_EXT_NRF_ESB_RX_DATA 5 COMM_SET_DUTY 56 COMM_EXT_NRF_SET_ENABLED 6 COMM_SET_CURRENT 57 COMM_DETECT_MOTOR_FLUX_LINKAGE_OPENLOOP 7 COMM_SET_CURRENT_BRAKE 58 COMM_DETECT_APPLY_ALL_FOC 8 COMM_SET_RPM 59 COMM_JUMP_TO_BOOTLOADER_ALL_CAN 9 COMM_SET_POS 60 COMM_ERASE_NEW_APP_ALL_CAN 10 COMM_SET_HANDBRAKE 61 COMM_WRITE_NEW_APP_DATA_ALL_CAN 11 COMM_SET_DETECT 62 COMM_PING_CAN 12 COMM_SET_SERVO_POS 63 COMM_APP_DISABLE_OUTPUT 13 COMM_SET_MCCONF 64 COMM_TERMINAL_CMD_SYNC 14 COMM_GET_MCCONF 65 COMM_GET_IMU_DATA 15 COMM_GET_MCCONF_DEFAULT 66 COMM_BM_CONNECT 16 COMM_SET_APPCONF 67 COMM_BM_ERASE_FLASH_ALL 17 COMM_GET_APPCONF 68 COMM_BM_WRITE_FLASH 18 COMM_GET_APPCONF_DEFAULT 69 COMM_BM_REBOOT 19 COMM_SAMPLE_PRINT 70 COMM_BM_DISCONNECT 20 COMM_TERMINAL_CMD 71 COMM_BM_MAP_PINS_DEFAULT 21 COMM_PRINT 72 COMM_BM_MAP_PINS_NRF5X 22 COMM_ROTOR_POSITION 73 COMM_ERASE_BOOTLOADER 23 COMM_EXPERIMENT_SAMPLE 74 COMM_ERASE_BOOTLOADER_ALL_CAN 24 COMM_DETECT_MOTOR_PARAM 75 COMM_PLOT_INIT 25 COMM_DETECT_MOTOR_R_L 76 COMM_PLOT_DATA 26 COMM_DETECT_MOTOR_FLUX_LINKAGE 77 COMM_PLOT_ADD_GRAPH 27 COMM_DETECT_ENCODER 78 COMM_PLOT_SET_GRAPH 28 COMM_DETECT_HALL_FOC 79 COMM_GET_DECODED_BALANCE 29 COMM_REBOOT 80 COMM_BM_MEM_READ 30 COMM_ALIVE 81 COMM_WRITE_NEW_APP_DATA_LZO 31 COMM_GET_DECODED_PPM 82 COMM_WRITE_NEW_APP_DATA_ALL_CAN_LZO 32 COMM_GET_DECODED_ADC 83 COMM_BM_WRITE_FLASH_LZO 33 COMM_GET_DECODED_CHUK 84 COMM_SET_CURRENT_REL 34 COMM_FORWARD_CAN 85 COMM_CAN_FWD_FRAME 35 COMM_SET_CHUCK_DATA 86 COMM_SET_BATTERY_CUT 36 COMM_CUSTOM_APP_DATA 87 COMM_SET_BLE_NAME 37 COMM_NRF_START_PAIRING 88 COMM_SET_BLE_PIN 38 COMM_GPD_SET_FSW 89 COMM_SET_CAN_MODE 39 COMM_GPD_BUFFER_NOTIFY 90 COMM_GET_IMU_CALIBRATION 40 COMM_GPD_BUFFER_SIZE_LEFT 92 COMM_GET_CUSTOM_CONFIG_XML 41 COMM_GPD_FILL_BUFFER 93 COMM_GET_CUSTOM_CONFIG 42 COMM_GPD_OUTPUT_SAMPLE 94 COMM_GET_CUSTOM_CONFIG_DEFAULT 43 COMM_GPD_SET_MODE 95 COMM_SET_CUSTOM_CONFIG 44 COMM_GPD_FILL_BUFFER_INT8 96 COMM_BMS_GET_VALUES 45 COMM_GPD_FILL_BUFFER_INT16 97 COMM_BMS_SET_CHARGE_ALLOWED 46 COMM_GPD_SET_BUFFER_INT_SCALE 98 COMM_BMS_SET_BALANCE_OVERRIDE 47 COMM_GET_VALUES_SETUP 99 COMM_BMS_RESET_COUNTERS 48 COMM_SET_MCCONF_TEMP 100 COMM_BMS_FORCE_BALANCE ⚠ 49 COMM_SET_MCCONF_TEMP_SETUP 100 COMM_GET_MCCONF_TEMP ⚠ 50 COMM_GET_VALUES_SELECTIVE 101 COMM_BMS_ZERO_CURRENT_OFFSET Contin`};export{e as default};