
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

For a detailed protocol please refer to the `commands.xlsx`.
All red marked commands are not implemented in the software, because i cant figure out what they are doing.



![Gopher image](/documentation/known_commands.PNG)


## THE IMAGE DATA BUFFER
After starting the engraving process the laser needs image data.
The first 9 bytes are fixed and containing information about the lasertime and img size
The image width is devided by 8 and each 8 pixel will be combined to a single byte  (0-255). This is the laserdiode enable time.
These subpixel will be stored in the buffer after the config section (buffer[9..]).
So the buffer size is `320px_image_width /8 + 9(config lenght)` for each image line.
Each line is send to the laser, until an Ack will be receieved. The buffer will be calculated for the next line in the image.

* `buffer[0]` - `9` command opcode
* `buffer[1]` -  buffer.size() >> 8
* `buffer[2]` - buffer.size()
* `buffer[3]` - depth >> 8   laser on time/engraving depth
* `buffer[4]` - depth    laser on time/engraving depth 1-255
* `buffer[5]` - 1000>>8 //laser power 1000mw cahnging has no effect
* `buffer[6]` - 1000
* `buffer[7]` - current image height position >> 8 //this will go from 0 to image.height
* `buffer[8]` - current image height position


To see the complete function to send an image to the engraver, see the `int start_engraving(` function in `main.cpp`.


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
After each command the K3 laser engraver sends a one ack byte `9`.
After the ack, the next line can be send.
I found out that at commands where the head has to travel long distances it makes more sense to wait 100ms until the next one is sent to avoid errors.







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
* `fan` - enable the fan
* `discrete` - enable the discrete mode. The laser dont turn off between 2 pixels with laser on state.
* `bwt` - Image will be converted into a black white image.
* `depth` - time the laser will be on for each pixel.
* `home` do a homing before starting
*`offsetx` - offsets the head before starting
* `offsety` -  offsets the head before starting
* `passes` - how many passes the image will be engraved  (default is 1)


## SIMPLE CALL with default settings
* `./k3_laser_api --port /dev/ttyUSB0 --if ./default.bmp`





# HOW REVERSE

##  3 wires (RX,TX,GND) for the serial connection of the stm32 and the USB2Serial converter to sniff the traffic
![Gopher image](/documentation/pictures/pcb.JPG)

## Using a DSO/Logic analyser to decode the commands and traffic between the host software and STM32
![Gopher image](/documentation/pictures/scope.png)
