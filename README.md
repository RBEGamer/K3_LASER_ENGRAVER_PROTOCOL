
![Gopher image](/documentation/logo.png)
A reverse engineered protocol for the cheap k3 laser engraver

![Gopher image](/documentation/pictures/device.JPG)


# WHY
The original software is windows only, no commandline interface or options, in chinese.
So i tried to sniff the communication to  build commandline version that accepts bmp files for engraving.





# FEATURES
* simple commandline software with minimal configuration
* works on a RPI
* usable for automatic engraving






# THE PROTOCOL

For a detailed protocol please `commands.xlsx`
all red marked commands are not implemented in the software, because i cant figure out what they are doing.

The protocol is very simple. The most things happend on the host pc.


![Gopher image](/documentation/known_commands.PNG)


## THE IMAGE DATA BUFFER
After starting the engraving process the laser engraver needs image data.
The first 9 bytes a fixed and containing information about the lasertime and img size
The image width is devided by 8 and each 8 pixel will be combined to a value  (0-255). this is the laserontime.
These subpixel will be stored in the buffer after the config section (buffer[9..]).
So the bufferlength will be `320px_image_width /8 + 9(config lenght)` for each image line.
This buffer will be send to the laser. Until an Ack is receieved, the buffer will be calculated for the next line in the image.

* `buffer[0]` - `9` command opcode
* `buffer[1]` -  buffer.size() >> 8
* `buffer[2]` - buffer.size()
* `buffer[3]` - depth >> 8   laser on time/engraving depth
* `buffer[4]` - depth    laser on time/engraving depth 1-255
* `buffer[5]` - 1000>>8 //laser power 1000mw cahnging has no effect
* `buffer[6]` - 1000
* `buffer[7]` - current image height position >> 8 //this will go from 0 to image.height
* `buffer[8]` - current image height position


To see the complete function to sned an image to the engraver see the `int start_engraving(` function in `main.cpp`


### Buffer size calucation
```cpp
 if ((bwimg.width() % 8) <= 0) {
        ilbsize = (bwimg.width() / 8) + 9;
    } else {
        ilbsize = (bwimg.width() / 8) + 10;
    }
 ```
 
 ### Converting an Imageline to the img buffer
 ```cpp
  for (int current_height_progress = 0; current_height_progress < bwimg.height(); ++current_height_progress) {
 ```
 
 
 
 ```cpp
 int num1 = 0;
        for (int index1 = 0; index1 < img_line_buffer.size() - 9; ++index1) {
            BYTE num2 = 0;
            for (int index2 = 0; index2 < 8; ++index2) {
                if ((index1 * 8 + index2) < bwimg.width() && bwimg.get_pixel((index1 * 8) + index2, current_height_progress).red == 0) {
                    num2 +=32;        
                }
            }
            img_line_buffer[num1 + 9] = (BYTE)num2;
            ++num1;
        }
        
        //SEND img_line_buffer over serial to the engraver
        _ser.Write(img_line_buffer,img_line_buffer.size());//SEND BUFFER TO ENGRAVER
        wait_for_ack(_ser); //WAIT FOR AN ACK
```



## THE ACK
After each command the K3 laser engraver sends a one byte ack. The byte is `9` every time.
After you receieve the `9` you can send the next command. 
I found out that at commands where the head has to travel long distances it makes more sense to wait 100ms until the next one is sent to avoid errors. This of the C++ software is this already implemented 







# RUN THE ENGRAVING SAMPLE
* `git clone https://github.com/RBEGamer/K3_LASER_ENGRAVER_PROTOCOL.git`
* `cd ./K3_LASER_ENGRAVER_PROTOCOL/src`
* `mkdir build`
* `cd ./build`
* `cmake ../`
* `make`
* `sudo chmod +x ./k3_laser_api`
* `connect the laser engraver to usb port and check the device manager or /dev for the serial device`

## ALL PARAMETERS
 ```
./k3_laser_api --port /dev/ttyUSB0 --if ./default.bmp --fan true --discrete false --bwt 100 --depth 50 --home false --offsetx 0 --offsety 0 --passes 1
```

### PARAMETERS
* `port` - set the serial port on linux for example `/dev/ttyUSB0`
* `if` - input file path, bitmap file for engraving. max size 1600x1520
* `fan` - enabled the fan during engraving
* `discrete` - enabled the discrete mode. if on the laser dotn turn off between 2 pixels with laser on
* `bwt` - if image will converted into a black white image. BWT is the number how light in rgb value the pixel can be to turn white.
* `depth` - time the laser will be on for each pixel.
* `home` do a homing before starting
*`offsetx` - offsets the head before starting
* `offsety` -  offsets the head before starting
* `passes` - how many passes the image will be engraved  (default is 1)
## SIMPLE CALL with default settings
* `./k3_laser_api --port /dev/ttyUSB0 --if ./default.bmp`





# HOW REVERSE

## First soldered 3 wires to the serial connecteion of the stm32 to sniff the traffic
![Gopher image](/documentation/pictures/pcb.JPG)

## Using a DSO/Logic analyser to decode the commands and traffic between the software and STM32
![Gopher image](/documentation/pictures/scope.png)
