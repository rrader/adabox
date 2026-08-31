# The detailed development datasheet download：

# 1 ．Product overview

DY-SV5W is a smart voice module developed independently by our company，
Integrated IO trigger, UART protocol control, One _line single-bus protocol control,
standard MP3 and other 7 operating modes setting by DIP switch; Drive 4~8Ω, 3~5W
speaker by a 5W class D audio amplifier on board **；** Support MP3、WAV decoding; Support
TF card for maximum 32G downloading music files directly by micro USB connector.

# 2 ．Product characteristics

1 、Support MP3、WAV decoding。

2 、Support sampling rate (KHz):8/11.025/12/16/22.05/24/32/44.1/48。

3 、24bit DAC output，support dynamic range 90dB，SNR 85 dB。

4 、Support FAT16/FAT32 file system，support TF card for maximum 32G。

5 、Drive 4~8Ω speaker by 5W class D amplifier chip on board。

6 、Play 65535 pieces of music by UART control protocol; Play, Pause, Selecting music,
VOL+/- and so on can be controlled，the communication baud rate is 9 600 bps。

7 、Play 255 pieces of music by IO triggering;

8 、Play music by One_line control protocol，Play, Pause, Selecting music, VOL+/- and so on
can be controlled。

9 、Switch 7 operating modes using DIP switch easily。

# 3 ．Definition of interfaces and functions


### Definition of pins

```
PINS Description
```
```
GND(POWER) POWER GND
```
```
+5V VCC (+5V)
```
```
TXD/IO0 IO1 in IO triggering mode；TX in UART control mode，
connect to RX of MCU
```
```
RXD/IO1 IO1 in IO triggering mode; RX in UART control mode，
connect to TX of MCU
```

```
IO2 IO2 in IO triggering mode
```
```
IO3 IO3 in IO triggering mode
```
```
IO4/ONE_LINE IO4 in IO triggering mode；RX in One_line mode
```
```
IO5 IO5 in IO triggering mode
```
```
IO6 IO6 in IO triggering mode
```
```
IO7 IO7 in IO triggering mode
```
```
BUSY Out put low level while playing music（ 0 V），output high
level when end playing（ 3 .3V）
```
```
GND Reference GND
```
### IO electric characteristics

```
IO input characteristics
```
### Symbol Parameters MIN

#### MAX UNIT

VIL (^)
LOW LEVEL INPUT (^0) 0.8 V

### VIH HIGH LEVEL INPUT^ 2.7 3.3 V

```
IO output characteristics
```
### VoL LOW LEVEL OUTPUT^0 0.33 V

### VoH HIGH LEVEL OUTPUT 2.7 3.3 V

### Configuration of DIP Switch

```
Corresponds
to the
number on
DIP switch
```
```
1 CON1 “Pull-down” close to the “1” “Pull-up” close to the
“ON”
2 CON2 “Pull-down” close to the “2” “Pull-up” close to the
“ON”
3 CON3 “Pull-down” close to the “3” “Pull-up” close to the
“ON”
```

**NOTES** ： **”I/O Combination mode 0”IO0~IO7 restore to high level after outputting
corresponding level, It's like triggering a button once.
“I/O Combination mode 1” IO0~IO7 always keep the corresponding level
after Outputting corresponding level.
The difference between “I/O Combination mode 0”and “I/O
Combination mode 1”is that it still plays music when IOs restore to original
high level in “I/O Combination mode 0”, and it stops playing music when
IOs restore to original high level in “I/O Combination mode 1”
The difference between “I/O stand-alone mode 0” and “I/O stand-alone
mode 1”is the same as above**

# 4 ．Module size

```
Control modes Configure PINS I/O Function
CON3 CON2 CON1 IO7 IO6 IO5 IO4 IO3 IO2 IO1 IO
I/O
Combination
mode 0
```
```
0 0 0 I/O Combination trigger at falling edge and restoring to high level ，play
2^8- 1 （ 255 ）pieces music.
```
```
I/O
Combination
mode 1
```
```
0 0 1 I/O Combination trigger while IOs keeping low level， play 2^8-1（ 255 ）
pieces music.
```
#### I/O

```
stand-alone
mode 0
```
```
0 1 0 8th
music
```
```
7th
music
```
```
6th
music
```
```
5th
music
```
```
4th
music
```
```
3rd
music
```
```
2nd
music
```
```
1st
music
```
```
I/O
stand-alone
mode 1
```
```
0 1 1 8th
music
```
```
7th
music
```
```
6th
music
```
```
5th
music
```
```
4th
music
```
```
3rd
music
```
```
2nd
music
```
```
1st
music
```
```
UART control
mode
1
0
0
```
```
RXD TXD
```
```
One_Line
mode
```
```
RXD
```
```
Standard MP
mode
```
```
1 0 1 RPT EQ P/P/
MOD
E
```
```
PREV/
V-
```
```
NEXT/
V+
```

# 5. Modes operation instructions

## 5.1 I/O Combination mode 0

MCU I/O output corresponding level to trigger specified music and release the IO
level to high, it stops playing music after completing playing current music; It will play the
new music, if retrigger the music while playing. If keep the triggering state, it will keep
playing circularly. The busy pin is always effective while playing.

**NOTE：The music files must be named by five numbers such as
00001.mp3~00255.mp**

Music Control

```
IO7 IO6 IO5 IO4 IO3 IO2 IO1 IO0 Music playing
1 1 1 1 1 1 1 0 00001.mp
```
```
1 1 1 1 1 1 0 1 00002.mp
```
```
1 1 1 1 1 1 0 0 00003.mp
```
```
1 1 1 1 1 0 1 1 00004.mp
```
```
1 1 1 1 1 0 1 0 00005.mp
```
```
1 1 1 1 1 0 0 1 00006.mp
```
```
1 1 1 1 1 0 0 0 00007.mp
```

#### ...... ...... ...... ...... ...... ...... ...... ...... ......

```
0 0 0 0 0 0 0 0 00255.mp
```
## 5.2 I/O Combination mode 1

MCU I/O output corresponding level to trigger specified music and keep the
triggering state, it will keep playing circularly. It will stop playing immediately when
release to original high level anytime. The busy pin is always effective while playing.

**NOTE：The music files must be named by five numbers such as
00001.mp3~00255.mp**

Music Control

```
IO7 IO6 IO5 IO4 IO3 IO2 IO1 IO0 Music playing
1 1 1 1 1 1 1 0 00001.mp
```
```
1 1 1 1 1 1 0 1 00002.mp
```
```
1 1 1 1 1 1 0 0 00003.mp
```
```
1 1 1 1 1 0 1 1 00004.mp
```
```
1 1 1 1 1 0 1 0 00005.mp
```
```
1 1 1 1 1 0 0 1 00006.mp
```
```
1 1 1 1 1 0 0 0 00007.mp
```
```
...... ...... ...... ...... ...... ...... ...... ...... ......
```
```
0 0 0 0 0 0 0 0 00255.mp
```
## 5.3 I/O stand-alone mode 0

IO0~IO7 control 8 pieces of music，one IO only control one piece of music; IO trigger
specified music by falling edge level and restore to original high level, it stops playing after
completing playing the triggered music. It will play new music, if retrigger the music while


playing；If keep the triggering state, it will keep playing circularly. The busy pin is always
effective while playing.

**NOTE：The music files must be named by five numbers such as
00001.mp3~00008.mp**

Music Control

```
IO7 IO6 IO5 IO4 IO3 IO2 IO1 IO0 Music playing
1 1 1 1 1 1 1 0 00001.mp
1 1 1 1 1 1 0 1 00002.mp
1 1 1 1 1 0 1 1 00003.mp
1 1 1 1 0 1 1 1 00004.mp
1 1 1 0 1 1 1 1 00005.mp
1 1 0 1 1 1 1 1 00006.mp
1 0 1 1 1 1 1 1 00007.mp
0 1 1 1 1 1 1 1 00008.mp
```
## 5.4 I/O stand-alone mode 1

IO0~IO7 control 8 pieces of music，one IO only control one piece of music; IO trigger
specified music by low level, and it will keep playing circularly if keep the triggering state.
It will stop playing immediately when release to original high level anytime. he busy pin is
always effective while playing.

**NOTE：The music files must be named by five numbers such as
00001.mp3~00008.mp**

Music Control

```
IO7 IO6 IO5 IO4 IO3 IO2 IO1 IO0 Music playing
1 1 1 1 1 1 1 0 00001.mp
1 1 1 1 1 1 0 1 00002.mp
1 1 1 1 1 0 1 1 00003.mp
```

```
1 1 1 1 0 1 1 1 00004.mp
1 1 1 0 1 1 1 1 00005.mp
1 1 0 1 1 1 1 1 00006.mp
1 0 1 1 1 1 1 1 00007.mp
0 1 1 1 1 1 1 1 00008.mp
```
## 5.5 UART Control Mode

### 5.5.1 Communication format

```
Full duplex serial communication；
```
```
Baud: 9600 bps，Data：8 bits ，stop:1bit，Check: N.
```
```
Start-CMD Type-data length（n）-data 1 －data n－CRC(SM)
```
 ※ CMD CODE ：fixed AA.

 ※ CMD TYPE : Used to distinguish CMD types.

 ※ DATA LENGTH：The number of bytes of data in CMD

 ※ DATA ：Data in CMD，there is no data when the length of data is one, only CMD.

 ※ CRC ：The sum of all data from the start code to the end data before the CRC, and

```
select the low 8 bits.
```
```
※ DATA FORMAT：Send the high 8 bits first, and the low 8 bits follow.
```
### 5.5.2 Communication Protocol

```
The definition of data as follows.
```
1. Definition of playing state ：It is in stop state when power on.

```
※ 00(Stop) 01(Play) 02(Pause)
```
2. The definition of devices: It is in stop state when switch device.

```
※ USB:00 SD:01 FLASH:02 NO_DEVICE：FF
```
3. Volume：The volume all is 31 steps which is 0-30，it is 20 steps when power on.


4. The definition of playing modes：It is in single stop mode when power on.

```
※ Full cycle(00)：Play all music in sequence.
```
```
※ Single cycle(01)：Play the current music always.
```
```
※ Single stop(02)：Stop playing after playing the current music once.
```
```
※ Random broadcast(03)：Play music randomly in device.
```
```
※ Repeat folder(04)：Play all music in current folder in sequence.
```
```
※ Random broadcast in folder(05): Play music randomly in current folder.
```
```
※ Order play in folder (06)：Play music in current folder in sequence,
```
```
and stop when complete playing the last music.
```
```
※ Order play (07)：Play music in device in sequence, and stop when
```
```
complete playing the last music.
```
5. Definition of EQ：It is NORMAL (00) when power on.

```
※ NORMAL(00) POP(01) ROCK(02) JAZZ(03) CLASSIC(04)
```
6. Definition of combination play：It is combined by file name, the file is stored in DY
    folder，the file is named as two bytes , the number name is recommended such as
    01.mp3, 02.mp3, and it is also named by two letters.

### 5.5.3 Definition of CMD

### Control CMD

#### CMD CODE RESPONSE

```
Play AA 02 00 AC No return
```
```
Pause AA 03 00 AD No return
```
```
Stop AA 04 00 AE No return
```
```
Previous
music
```
```
AA 05 00 AF No return
```
```
Next music AA 06 00 B0 No return
```

```
Volume+ AA 14 00 BE No return
```
```
Volume- AA 15 00 BF No return
```
```
Previous
folder
directory
```
```
AA 0E 00 B8 No return
```
```
Next folder
directory
```
```
AA 0F 00 B9 No return
```
```
End playing AA 10 00 BA No return
```
### Setting CMD

#### CMD CODE RESPONSE

```
Volume setting AA 13 01 VOL SM No return
```
```
Cycle mode setting AA 18 01 Mode SM No return
```
```
Cycle times setting
AA 19 02 High Byte Low Byte
Times
```
#### SM

```
No return
```
```
EQ setting AA 1A 01 EQ SM No return
```
```
Select specified music
AA 07 02 High Byte Low Byte
Music NO.
```
#### SM

```
No return
```
```
Select device and path AA 08 Length Device Path SM No return
```
```
Switch to selected
device
```
```
AA 0B 01Device SM No return
```
```
Select specified file to
interlude
```
```
AA 16 03 Device High Byte Low Byte
Music NO.
```
SM (^) No return
Select specified path to
interlude
AA 17 Length Device Path SM No return


```
Select file but do not
play
AA 1F 0 2 High Byte Low Byte
Music NO.
```
#### SM

```
No return
```
### Check CMD

#### CMD CODE RESPONSE

```
Check Play State AA 01 00 AB AA 01 01 State SM
```
```
Check Device Online AA 09 00 B3 AA 09 01Device SM
```
```
Check Current Playing
Device
```
```
AA 0A 00 B4 AA 0A 01 Device SM
```
```
Check Number Of all
Music
```
#### AA 0C 00 B^

```
AA 0C 02 High Byte Low Byte
Music NO.
```
#### SM

```
Check Current Music AA 0D 00 B^
AA 0D 02 High Byte Low Byte
Music NO.
```
#### SM

```
Check the first Music in
Folder
```
#### AA 11 00 BB^

```
AA 11 02 High Byte Low Byte
Music NO.
```
#### SM

```
Check Number of Music
In Folder
```
#### AA 12 00 BC^

```
AA 12 02 High Byte Low Byte
Music NO.
```
#### SM

### UART Tuning Tool


### The detailed development data of UART mode, please check the

### “UART mode user’s guide”

## 5.6 One_line Mode

## One_line control to play

Send the CMD as follow, send the low bit first. Send 89H as follow. The start flag is min
2ms.


### CMD format

```
CMD(HEX) Function Instruction
```
```
00 NO.0 Send the number first and
function CMD is followed,
such as setting volume to
21 steps, send
“0x02” ”0x01” ”0x0C
”
```
#### 01 NO.

#### 02 NO.

#### 03 NO.

#### 04 NO.

#### 05 NO.

#### 06 NO.

#### 07 NO.

#### 08 NO.

#### 09 NO.

```
0A Clear digital Clear digital sent
```
```
0B Select and enter
```
```
Setting function with
digital
```
```
0C Volume Setting
```
```
0D EQ Setting
```
```
0E Set cycle mode
```
```
0F Set channel
```
```
10 Select music to
```

```
inter-cut
```
```
11 Play
```
```
12 Pause
```
```
13 Stop
```
```
14 Previous music
```
```
15 Next music
```
```
16 Previous directory
```
```
17 Next directory
```
```
18 SD card selected
```
```
19 U Disk selected
```
```
1A FLASH selected
```
```
1B System sleep
```
```
1C End playing
```
### NOTE: “Select music” and “Inter-cut” are based the name of music;

### For example, the music name is “00123.mp3”,send the data

### “0x01”“0x02”“0x03”“0x0B” in sequence to complete selecting

### music.

### The detailed development data of One_Line mode, please check the

### “One_Line mode user’s guide”


