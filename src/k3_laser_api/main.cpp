
#include <string>
#include <iostream>
#include <cstdio>

#include "./bitmap/bitmap_image.hpp"
#include "cxxopts.hpp"
#include "./serial/serialib.h"
#include <unistd.h>
#include <vector>


#include <math.h> //for sqrt pow

#define squared(_x) pow(_x,2.0) //defines x ^ 2


using namespace std;

#define SERIAL_BAUD_RATE 115200 //my k3 laser with V1 PCB uses 115200
#define WAIT_FOR_ACK_RETRIES 100 // x*WAIT_FOR_ACK_TIME
#define WAIT_FOR_ACK_TIME 100 //wait to fill the serial buffer ms
#define LOG_OUTPUT_STDOUT //LOGS SOME INFO TO STDOUT
//#define IGNORE_ACK_ERROR //if defined ack errors will be ignored

//------------ MAX MOVEMENT AREA ---------------------------------------------------------------------------------------
#define BOUNDING_BOX_MAX_X 1600 //width
#define BOUNDING_BOX_MAX_Y 1520 //in theory also 1600 //height
#define CHECK_OUT_OF_BOUNDING_BOX_MOVE //define to prevent laser head move out of the limits
#define TRAVEL_TIME_DELAY 10 //the laser need 10seconds fpr 1600pixel to move, so we have to wait 10ms for one step
#define TRAVE_TRIME_THRESHOLD 100 //add a delay if travel time is more than 100 ms

#define LASER_POWER_MW 1000 //has no (visible )effect to my model


serialib ser;
int Ret;             //returns the recieved serial chars


typedef unsigned char BYTE;

//STORAGE FOR THE ABS POSTION OF THE HEAD
//this variables are not corrent during engraving, after engravin make send home to reset these to correct values
int32_t head_abs_pos_x = 0;
int32_t head_abs_pos_y = 0;


void thread_sleep(unsigned milliseconds) {
    usleep(milliseconds * 1000);
}



//RANDOM STRING HELPER FUNCS--------------------------------------------------------------------------------------------
#include <random>
#include <functional> //for std::function
#include <algorithm>  //for std::generate_n

typedef std::vector<char> char_array;

char_array charset()
{
    return char_array(
            {'0','1','2','3','4',
             '5','6','7','8','9',
             'A','B','C','D','E','F',
             'G','H','I','J','K',
             'L','M','N','O','P',
             'Q','R','S','T','U',
             'V','W','X','Y','Z',
             'a','b','c','d','e','f',
             'g','h','i','j','k',
             'l','m','n','o','p',
             'q','r','s','t','u',
             'v','w','x','y','z'
            });
};

// given a function that generates a random character,
// return a string of the requested length
std::string random_string( size_t length, std::function<char(void)> rand_char )
{
    std::string str(length,0);
    std::generate_n( str.begin(), length, rand_char );
    return str;
}

std::string gen_radnom_str(){
    const auto ch_set = charset();
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, ch_set.size()-1);
    auto randchar = [ ch_set,&dist,&rng ](){return ch_set[ dist(rng) ];};
    auto length = 10;
    return random_string(length,randchar);
}
//END RANDOM STRING HELPER FUNCS--------------------------------------------------------------------------------------------



//HERE ARE ALL AVARIABLE COMMANDS FOR THE K3 LASER
//please the the commands excel sheet for further commands
//not all commands are working or i dont know the purpose


//--------------------- 4 BYTE COMMANDS --------------------------------------------------------------------------------
#define send_connect_sequence_command(_ser) send_4byte_cmd(_ser,10)
#define send_fan_on_command(_ser) send_4byte_cmd(_ser,4)
#define send_fan_off_command(_ser) send_4byte_cmd(_ser,5)
#define send_hui_ling_command(_ser) send_4byte_cmd(_ser,8)
#define send_reset_command(_ser) send_4byte_cmd(_ser,6)

//COMMANDS AVARIABLE DURING THE ENGRAVING PROCESS
#define send_stop_command(_ser) send_4byte_cmd(_ser,22)
#define send_end_command(_ser) send_4byte_cmd(_ser,21)

//COMMANDS AVARIABLE BEFORE STARTING THE ENGRAVING PROCESS
#define send_disable_discrete_mode_command(_ser) send_4byte_cmd(_ser,28)
#define send_enable_discrete_mode_command(_ser) send_4byte_cmd(_ser,27)


//--------------------------------- MOVEMENT FUNCTIONS -----------------------------------------------------------------
//MOVE HEAD CURSOR FUNCTIONS
#define send_move_center_manual_command(_ser) send_move_head_relative_command(_ser,BOUNDING_BOX_MAX_X/2,BOUNDING_BOX_MAX_Y/2)
#define send_move_right(_ser) send_move_head_relative_command(_ser,99,0)
#define send_move_left(_ser) send_move_head_relative_command(_ser,-99,0)
#define send_move_up(_ser) send_move_head_relative_command(_ser,0,99)
#define send_move_down(_ser) send_move_head_relative_command(_ser,0,-99)


//------------------------------ 5 BYTE FUNCKTIONS ---------------------------------------------------------------------
#define send_zuo(_ser, _val) send_5byte_cmd(_ser,17,_val) //MACHEN ZUO ??
#define send_xia(_ser, _val) send_5byte_cmd(_ser,16,_val) //UNTER UNDER XIA ???
#define send_shang(_ser, _val) send_5byte_cmd(_ser,15,_val) //licht an light on shang
#define send_go_x(_ser, _val) send_5byte_cmd(_ser,11,_val) //go_x
#define send_go_y(_ser, _val) send_5byte_cmd(_ser,12,_val) //go_y
#define send_blink_laser(_ser, _val) send_5byte_cmd(_ser,7,_val) //enabled laser for 20ms



//------------------------------- 1 BYTE FUNCTIONS ---------------------------------------------------------------------
//COMMANDS AVARIABLE DURING THE ENGRAVING PROCESS
#define send_continue_command(_ser) send_1byte_cmd(_ser,25)
#define send_suspend_command(_ser) send_1byte_cmd(_ser,24) //PAUSE

//------------------------------- ENGRAVING FUNCTIONS ------------------------------------------------------------------








//this functions block until the engraver send the ACK byte (9)
//it can be disabled via IGNORE_ACK_ERROR define
int wait_for_ack(serialib &_ser) {
    int trys = 0;
    int ret = 0;
    int ack_ok = 0;
    char rec_buffer[128];

    //| trys > WAIT_FOR_ACK_RETRIES
    //trys < WAIT_FOR_ACK_RETRIES
    while ( trys < WAIT_FOR_ACK_RETRIES) {
        thread_sleep(WAIT_FOR_ACK_TIME);
        trys++;
        ret = _ser.Read(rec_buffer, 128, 100);
        if (rec_buffer[0] == 9) {
#ifdef LOG_OUTPUT_STDOUT
            std::cout << "ACK_OK" << "after " <<trys <<" trys"<< std::endl;
#endif
            ack_ok = 1;
            break;
        }

#ifdef LOG_OUTPUT_STDOUT
        std::cout << "ACK_OK" << std::endl;
#endif
    }
//If defined ignore if an ack is missing
#ifdef IGNORE_ACK_ERROR
     ack_ok = true;
#endif

#ifdef LOG_OUTPUT_STDOUT
    if (!ack_ok) {
        std::cout << "ACK_TIMEOUT" << std::endl;
    }
#endif
    return ack_ok;
}


//sends the simple commands with 4 byte command signature see commands.xlsx for that
int send_4byte_cmd(serialib &_ser, BYTE _cmd) {
    BYTE send_buffer[4] = {_cmd, 0, 4, 0};
    Ret = ser.Write(send_buffer, 4);
    return wait_for_ack(_ser);
}


int send_5byte_cmd(serialib &_ser, BYTE _cmd, int _val) {
    BYTE send_buffer[5] = {_cmd, 0, 5, (BYTE) (_val >> 8), (BYTE) _val};
    Ret = ser.Write(send_buffer, 5);
    return wait_for_ack(_ser);
}


int send_1byte_cmd(serialib &_ser, BYTE _cmd) {
    BYTE send_buffer[1] = {_cmd};
    Ret = ser.Write(send_buffer, 1);
    return wait_for_ack(_ser);
}


//the home command ist different because it move the laserhead to zero and resets the absolut postion variabled
int send_home_command(serialib &_ser) {
    int r = send_4byte_cmd(_ser, 23);
    //if home finisehd reset also the head position to zero
    if (r) {
        head_abs_pos_x = 0;
        head_abs_pos_y = 0;
    }
    thread_sleep(TRAVEL_TIME_DELAY * sqrt(BOUNDING_BOX_MAX_X * BOUNDING_BOX_MAX_Y));
    return r;
}


int send_home_and_center_command(serialib &_ser) {
    int ret = send_4byte_cmd(_ser, 26);
    //if home finisehd reset also the head position to zero
    if (ret) {
        head_abs_pos_x = 0;
        head_abs_pos_y = 0;
    }
    thread_sleep(TRAVEL_TIME_DELAY * sqrt(BOUNDING_BOX_MAX_X * BOUNDING_BOX_MAX_Y));
    return ret;
}


int send_move_head_relative_command(serialib &_ser, int _x, int _y) {

#ifdef CHECK_OUT_OF_BOUNDING_BOX_MOVE
    if ((head_abs_pos_x + _x) > BOUNDING_BOX_MAX_X) {
        _x = _x + (head_abs_pos_x + _x) - BOUNDING_BOX_MAX_X;
    }

    if ((head_abs_pos_y + _y) > BOUNDING_BOX_MAX_Y) {
        _y = _y + (head_abs_pos_y + _y) - BOUNDING_BOX_MAX_Y;
    }

    if ((head_abs_pos_x + _x) < 0) {
        _x = -head_abs_pos_x;
    }

    if ((head_abs_pos_x + _y) < 0) {
        _y = -head_abs_pos_y;
    }
#endif


    BYTE send_buffer[7] = {1, 0, 7, (BYTE) (_x >> 8), (BYTE) (_x), (BYTE) (_y >> 8), (BYTE) (_y)};
    //CALC NEW ABS POSTION
    head_abs_pos_x += _x;
    head_abs_pos_y += _y;

    //SEND AND WAIT FOR ACK
    Ret = ser.Write(send_buffer, 7);
    int r = wait_for_ack(ser);

    uint32_t tt = sqrt(squared(_x) + squared(_y)) * TRAVEL_TIME_DELAY; //calc travel time
    if (tt > TRAVE_TRIME_THRESHOLD) {
        thread_sleep(tt);
    }
}



int send_laser_start_engrave_command_and_move_to_pos(serialib &_ser, int _x, int _y, bitmap_image& _img) {
//TODO CHECK FOR IMG OUT OF BOUNCE
#ifdef CHECK_OUT_OF_BOUNDING_BOX_MOVE
    if ((head_abs_pos_x + _x) > BOUNDING_BOX_MAX_X) {
        _x = _x + (head_abs_pos_x + _x) - BOUNDING_BOX_MAX_X;
    }

    if ((head_abs_pos_y + _y) > BOUNDING_BOX_MAX_Y) {
        _y = _y + (head_abs_pos_y + _y) - BOUNDING_BOX_MAX_Y;
    }

    if ((head_abs_pos_x + _x) < 0) {
        _x = -head_abs_pos_x;
    }

    if ((head_abs_pos_x + _y) < 0) {
        _y = -head_abs_pos_y;
    }
#endif


    BYTE send_buffer[7] = {20, 0, 7, (BYTE) (_x >> 8), (BYTE) (_x), (BYTE) (_y >> 8), (BYTE) (_y)};
    //CALC NEW ABS POSTION
    head_abs_pos_x += _x;
    head_abs_pos_y += _y;

    //SEND AND WAIT FOR ACK
    Ret = ser.Write(send_buffer, 7);
    int r = wait_for_ack(ser);

    uint32_t tt = sqrt(squared(_x) + squared(_y)) * TRAVEL_TIME_DELAY; //calc travel time
    if (tt > TRAVE_TRIME_THRESHOLD) {
        thread_sleep(tt);
    }
}


//TODO TEST
int send_move_head_absolute_command(serialib &_ser, int _x, int _y) {
    return send_move_head_relative_command(_ser, _x - head_abs_pos_x, _y - head_abs_pos_x);
}




int rezise_img(bitmap_image& _img, int _new_w, int _new_h){
    return 0;
}


//scale -> 0-3200 (2*boundingbox_w)
//scale the image for the laser area the min size is 200x200px and max is 1600
#define IMG_SCALE_MIN_SIZE_WIDTH 200
int scale_image(bitmap_image& _img, int _scale_factor =3200){
    if ((_img.width() - BOUNDING_BOX_MAX_X) > (_img.height() - BOUNDING_BOX_MAX_Y))
    {
        int newW = _img.width() + (_scale_factor - BOUNDING_BOX_MAX_X);
        if (newW < IMG_SCALE_MIN_SIZE_WIDTH){
            newW = IMG_SCALE_MIN_SIZE_WIDTH;
        }
        if (newW > BOUNDING_BOX_MAX_X){
            newW = BOUNDING_BOX_MAX_X;
        }
        double scale_factor = (double) _img.width() * 1.0 / (double) newW;
        int newH = (int) ((double) _img.height() / scale_factor);
        return  rezise_img(_img,newW, newH);
    }
    else
    {
        int newH = _img.height() + (_scale_factor - BOUNDING_BOX_MAX_X);
        if (newH < IMG_SCALE_MIN_SIZE_WIDTH){
            newH = IMG_SCALE_MIN_SIZE_WIDTH;
        }
        if (newH > BOUNDING_BOX_MAX_Y){
            newH = BOUNDING_BOX_MAX_Y;
        }
        double scale_factor = (double) _img.height() * 1.0 / (double) newH;
        int newW = (int) ((double)_img.width() / scale_factor);
        return  rezise_img(_img,newW, newH);
    }
}


int start_engraving(serialib &_ser, std::string _bitmap_file, char _black_white_treshhold = 128, bool _gen_only_images = false, int _engraving_depth_intensity = 30,bool _discrete = false, bool _enable_fan = true, int _engrave_start_offset_x = 0, int _engrave_start_offset_y = 0, int _passes =  1) {

    std::string tmp_filename = gen_radnom_str();//storing the processed image for deb
    bitmap_image image(_bitmap_file);

    if (!image) {
        printf("Error - Failed to open '%s'\n", _bitmap_file.c_str());
        return -1;
    }
    //MAKE A SIMPLE GREYSCALE IMAGE
    image.convert_to_grayscale();
    image.save_image("./tmp/0_" + tmp_filename);


    bitmap_image bwimg(image.width(),image.height());

    //MAKE A COMPLE BLACK WHITE IMAGE
    for (unsigned int x = 0; x < bwimg.width(); x++) {
        for (unsigned int y = 0; y < bwimg.height(); y++) {

            //    std::cout << ((int)image.get_pixel(x, y).red) << std::endl;
            if (((int)image.get_pixel(x, y).red) > (unsigned char)_black_white_treshhold) {
                bwimg.set_pixel(x, y, 255, 255, 255);
            }else{
                bwimg.set_pixel(x, y, 0, 0, 0);
            }
        }
    }
    bwimg.save_image("./tmp/1_" + tmp_filename);

    //DONT START ENGRAVING IF IN PREVIEW MODE
    if (_gen_only_images) {
        return 0;
    }


    //START ENGRAVING


    if(_engraving_depth_intensity > 255){
        _engraving_depth_intensity = 255;
    }
    if(_engraving_depth_intensity <= 0){
        _engraving_depth_intensity = 1;
    }
    //send_enable_discrete_mode_command(_ser);
    //the laser will turn on/off for each pixel -° is more accurate
    if(_discrete){
        send_enable_discrete_mode_command(_ser);
    }else{
        send_disable_discrete_mode_command(_ser);//DISBALE DISCRETE MODE
    }

    send_reset_command(_ser);//RESET



    //ENABLE FAN
    if(_enable_fan){
        send_fan_on_command(_ser);
    }
    thread_sleep(500);
    
    //SEND START POSITION in ui app the top_left corner of the image to engrave in the laser area
    send_laser_start_engrave_command_and_move_to_pos(_ser,_engrave_start_offset_x,_engrave_start_offset_y,image);
    thread_sleep(500);



    //CALCULATE BYTE ARRAY TO SENT TO THE PRINTER -> the array contains one line
    unsigned int ilbsize = 0;
    if ((bwimg.width() % 8) <= 0) {
        ilbsize = (bwimg.width() / 8) + 9;
    } else {
        ilbsize = (bwimg.width() / 8) + 10;
    }
    BYTE img_line_buffer[ilbsize];

    BYTE lookup_array[8] = {(BYTE)128, (BYTE)64, (BYTE)32, (BYTE)16, (BYTE)8, (BYTE)2, (BYTE)1};


    for (int current_height_progress = 0; current_height_progress < bwimg.height(); ++current_height_progress) {


        //see README.md
        int num1 = 0;
        for (int index1 = 0; index1 < ilbsize - 9; ++index1) {
            BYTE num2 = 0;
            for (int index2 = 0; index2 < 8; ++index2) {
                if ((index1 * 8 + index2) < bwimg.width() && bwimg.get_pixel((index1 * 8) + index2, current_height_progress).red == 0) {
                    num2 |= lookup_array[index2];
                }
            }
            img_line_buffer[num1 + 9] = (BYTE)num2;
            ++num1;
        }

        //SET CONFIGRATION SEEE README.md
        img_line_buffer[0] = (BYTE)9;
        img_line_buffer[1] = (BYTE) (ilbsize >> 8);
        img_line_buffer[2] = (BYTE) (ilbsize);
        img_line_buffer[3] = (BYTE) (_engraving_depth_intensity >> 8);
        img_line_buffer[4] = (BYTE) (_engraving_depth_intensity);
        const int laser_intesity_mw = LASER_POWER_MW;//TODO
        img_line_buffer[5] = (BYTE) (laser_intesity_mw >> 8);
        img_line_buffer[6] = (BYTE) (laser_intesity_mw);
        img_line_buffer[7] = (BYTE) (current_height_progress >> 8);
        img_line_buffer[8] = (BYTE) (current_height_progress);

        //CHECK IF IMAGE LINE IS COMPLETE WHITE THEN SKIP IT
        bool is_line_white = false;
        for (int index = 9; index < ilbsize; ++index) {
            if (img_line_buffer[index] != (BYTE) 0) {
                is_line_white = true;
                break;
            }
        }
        //IF SOMETHIN TO LASER IS IN THIS LINE
        if(is_line_white ){
            for(int pass = 0; pass < _passes;pass++)
            _ser.Write(img_line_buffer,ilbsize);//SEND BUFFER TO ENGRAVER
            wait_for_ack(_ser);
            std::cout << "progress_" << current_height_progress << " for pass "<< pass << " of "<<_passes <<std::endl;
            thread_sleep(100);
        }
        }
    }
    return 1;
}


























int main(int argc, char *argv[]) {


    std::string port = "/dev/ttyUSB0";
    int laser_depth = 10;
    std::string file_to_laser = "./default.bmp";
    bool fan = false;
    int bwt = 128;
    int offset_x = 0;
    int offset_y = 0;
    bool discrete = false;
    int passes = 1;

    try
    {
        cxxopts::Options options(argv[0], " - example command line options");
        options
                .positional_help("[optional args]")
                .show_positional_help();

        bool apple = false;

        options
                .allow_unrecognised_options()
                .add_options()
                        ("if", "input-file to laser eg. ./laser.bmp", cxxopts::value<std::string>(), "./img.bmp")
                        ("depth", "on time for the laser per pixel", cxxopts::value<int>(), "1-199")
                        ("bwt", "the black white vonversation threshold", cxxopts::value<int>(), "1-255")
                        ("port", "The serial device string", cxxopts::value<std::string>(), "/dev/ttyUSB0")
                        ("fan", "enables the fan while engraving", cxxopts::value<bool>(), "true,false")
                        ("discrete", "enables the discrete mode", cxxopts::value<bool>(), "true,false")
                        ("offsetx", "offsets the image X", cxxopts::value<int>(), "0-1600-imgage width")
                        ("offsety", "offsets the image Y", cxxopts::value<int>(), "0-1520-imgage width")
                        ("passes", "how many passes, increase the depth", cxxopts::value<int>(), "1")
                        ("help", "Print help")
                ;



        options.parse_positional({"input", "output", "positional"});

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help({"", "Group"}) << std::endl;
            exit(0);
        }



        if (result.count("if"))
        {
            file_to_laser = result["if"].as<std::string>();
            std::cout << "file_to_laser= " << file_to_laser << std::endl;
        }

        if (result.count("port"))
        {
            port = result["port"].as<std::string>();
            std::cout << "set serial_port = " << port << std::endl;
        }


        if (result.count("depth"))
        {
            laser_depth =  result["depth"].as<int>();
            std::cout << "set laser_engrave_depth = " << laser_depth<< std::endl;
        }
        if (result.count("bwt"))
        {
            bwt =  result["bwt"].as<int>();
            std::cout << "set black_white_treshhold = " << bwt<< std::endl;
        }

        if (result.count("fan"))
        {
            fan =  result["fan"].as<bool>();
            std::cout << "set fan state = " <<fan << std::endl;
        }

        if (result.count("discrete"))
        {
            discrete =  result["discrete"].as<bool>();
            std::cout << "set discrete mode state = " << discrete << std::endl;
        }

        if (result.count("offsetx"))
        {
            offset_x =  result["offsetx"].as<int>();
            std::cout << "set offsetx = " <<offset_x<< std::endl;
        }

        if (result.count("offsety"))
        {
            offset_y =  result["offsety"].as<int>();
            std::cout << "set offsety = " <<offset_y<< std::endl;
        }

if (result.count("passes"))
        {
            passes =  result["passes"].as<int>();
            if(passes<= 0){
            passes = 1;
            }
            std::cout << "set paases = " <<passes<< std::endl;
        }
        std::cout << "Arguments remain = " << argc << std::endl;
    } catch (const cxxopts::OptionException& e)
    {
        std::cout << "error parsing options: " << e.what() << std::endl;
        exit(1);
    }




    //CONNECT TO SERIAL PORT
    Ret = ser.Open(port.c_str(), SERIAL_BAUD_RATE);
    if (Ret != 1) {
        std::cout << "CONNECTION_ERROR_CANT_CONNECT" << std::endl;
        return -1;
    }
#ifdef  LOG_OUTPUT_STDOUT
    std::cout << "SERIAL_CONNECTED" << std::endl;
#endif

    
    Ret = send_connect_sequence_command(ser);
    if (Ret <= 0) {
        std::cout << "SERIAL_DEVICE_NO_CONNECTION_ANSWER_IS_IT_THE_RIGHT_DEVICE" << std::endl;
        return -2;
    }





    //thread_sleep(1000);
    send_home_command(ser);
  //  thread_sleep(100);





    Ret =  start_engraving(ser, file_to_laser,bwt,false,laser_depth,discrete,fan,offset_x,offset_y,passes);

    ser.Close();
    return Ret;
}
