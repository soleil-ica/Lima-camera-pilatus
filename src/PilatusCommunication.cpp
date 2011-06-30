/*
 This file is part of LImA, a Library for Image Acquisition

 Copyright (C) : 2009-2011
 European Synchrotron Radiation Facility
 BP 220, Grenoble 38043
 FRANCE

 This is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, see <http://www.gnu.org/licenses/>.
*/


#include <yat/network/Address.h>
#include <pthread.h>

#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <iostream>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <poll.h>

#include "Exceptions.h"

#include "PilatusCommunication.h"

using namespace lima;
using namespace lima::PilatusCpp;



#define WAIT_UNTIL(testState,errmsg) while(m_state != testState)    \
{                                                                      \
  if(!m_cond.wait(TIME_OUT))                                          \
    THROW_HW_ERROR(lima::Error) << errmsg;                              \
}

//-----------------------------------------------------
//
//-----------------------------------------------------
inline void _split(const std::string inString, const std::string &separator,std::vector<std::string> &returnVector)
{
    std::string::size_type start = 0;
    std::string::size_type end = 0;

    while ((end=inString.find (separator, start)) != std::string::npos)
    {
        returnVector.push_back (inString.substr (start, end-start));
        start = end+separator.size();
    }

    returnVector.push_back (inString.substr (start));
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Communication::Communication(const char *host,int port)
                :   m_socket(-1),
                    m_stop(false),
                    m_thread_id(0),
                    m_state(DISCONNECTED)
{
    DEB_CONSTRUCTOR();
    m_server_ip         = host;
    m_server_port       = port;
    _initVariable();

    if(pipe(m_pipes))
        THROW_HW_ERROR(Error) << "Can't open pipe";

    pthread_create(&m_thread_id,NULL,_runFunc,this);

    connect(host,port);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Communication::~Communication()
{
    AutoMutex aLock(m_cond.mutex());
    m_stop = true;
    if(m_socket >= -1)
        close(m_socket);
    else
        write(m_pipes[1],"|",1);
    aLock.unlock();

    void *returnPt;
    if(m_thread_id > 0)
        pthread_join(m_thread_id,&returnPt);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
std::string Communication::serverIP() const
{
    return m_server_ip;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Communication::serverPort() const
{
    return m_server_port;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::_initVariable()
{
    m_imgpath                           = "/ramdisk/images/";
    m_file_name                         = "image_%.5d.cbf";
    m_file_pattern                      = m_file_name;
    m_threshold                         = -1;
    m_gain                              = DEFAULT_GAIN;
    m_exposure                          = -1.;
    m_nimages                           = 1;
    m_exposure_period                   = -1.;
    m_hardware_trigger_delay            = -1.;
    m_exposure_per_frame                = 1;

    GAIN_SERVER_RESPONSE["low"]         = LOW;
    GAIN_SERVER_RESPONSE["mid"]         = MID;
    GAIN_SERVER_RESPONSE["high"]        = HIGH;
    GAIN_SERVER_RESPONSE["ultra high"]  = UHIGH;

    GAIN_VALUE2SERVER[LOW]              = "lowG";
    GAIN_VALUE2SERVER[MID]              = "midG";
    GAIN_VALUE2SERVER[HIGH]             = "highG";
    GAIN_VALUE2SERVER[UHIGH]            = "uhighG";
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::connect(const char *host,int port)
{
    DEB_MEMBER_FUNCT();

    _initVariable();

    if(host && port)
    {
        AutoMutex aLock(m_cond.mutex());
        if(m_socket >= 0)
            close(m_socket);
        else
        {
            m_socket = socket(AF_INET, SOCK_STREAM,0);
            if(m_socket >= 0)
            {
                int flag = 1;
                setsockopt(m_socket,IPPROTO_TCP,TCP_NODELAY, (void*)&flag,sizeof(flag));
                struct sockaddr_in add;
                add.sin_family = AF_INET;
                add.sin_port = htons((unsigned short)port);
                add.sin_addr.s_addr = inet_addr(yat::Address(host,0).get_ip_address().c_str());
                if(::connect(m_socket,reinterpret_cast<sockaddr*>(&add),sizeof(add)))
                {
                    close(m_socket);
                    m_socket = -1;
                    THROW_HW_ERROR(Error) << "Can't open connection";
                }
                write(m_pipes[1],"|",1);
                m_state = Communication::OK;
                _resync();
            }
            else
                THROW_HW_ERROR(Error) << "Can't create socket";
        }
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::_resync()
{
    send("SetThreshold");
    send("exptime");
    send("expperiod");
    std::stringstream cmd;
    cmd<<"imgpath "<<m_imgpath;
    send(cmd.str());
    send("delay");
    send("nexpframe");
    send("setackint 0");
    send("dbglvl 1");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::_reinit()
{
    _resync();
    send("nimages");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::send(const std::string& message)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(message);

    std::string msg = message;
    msg+= '\030';
    write(m_socket,msg.c_str(),msg.size());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void* Communication::_runFunc(void *commPt)
{
    ((Communication*)commPt)->_run();
    return NULL;
}

//-----------------------------------------------------
/*AU DEMARRAGE
15 OK  Settings: mid gain; threshold: 6000 eV; vcmp: 0.679 V
 Trim file:
  /home/det/p2_det/config/calibration/p6m0106_E12000_T6000_vrf_m0p20.bin15 OK Exposure time set to: 0.1000000 sec.15 OK Exposure period set to: 0.103000 sec10 ERR mkdir() failed15 OK Delay time set to: 0.000000 sec.15 OK Exposures per frame set to: 115 OK
*/
/*AU SNAP
15 OK Exposure time set to: 0.1000000 sec.eV; vcmp: 0.679 V
 Trim file:
  /home/det/p2_det/config/calibration/p6m0106_E12000_T6000_vrf_m0p20.bin15 OK Exposure time set to: 0.1000000 sec.15 OK Exposure period set to: 0.103000 sec10 ERR mkdir() failed15 OK Delay time set to: 0.000000 sec.15 OK Exposures per frame set to: 115 OK
*/
//-----------------------------------------------------
void Communication::_run()
{
    DEB_MEMBER_FUNCT();
    struct pollfd fds[2];
    fds[0].fd = m_pipes[0];
    fds[0].events = POLLIN;

    fds[1].events = POLLIN;
    AutoMutex aLock(m_cond.mutex());
    while(!m_stop)
    {
        int nb_poll_fd = 1;
        if(m_socket >= 0)
        {
            fds[1].fd = m_socket;
            ++nb_poll_fd;
        }
        aLock.unlock();
        poll(fds,nb_poll_fd,-1);
        aLock.lock();
        if(fds[0].revents)
        {
            char buffer[1024];
            read(m_pipes[0],buffer,sizeof(buffer));
        }
        if(nb_poll_fd > 1 && fds[1].revents)
        {
            char messages[16384];
            int aMessageSize = recv(m_socket,messages,sizeof(messages),0);
            std::string strMessages(messages,aMessageSize );
            if(!aMessageSize)
            {
                DEB_TRACE() <<"--> no message received";
                close(m_socket);
                m_socket = -1;
                m_state = DISCONNECTED;
            }
            else
            {
                DEB_TRACE() << "--> messages = "<<strMessages;
                std::vector<std::string> msg_vector;
                _split(strMessages,"\x18",msg_vector); // '\x18' == '\030'
                for(std::vector<std::string>::iterator msg_iterator = msg_vector.begin(); msg_iterator != msg_vector.end();++msg_iterator)
                {
                    std::string &msg = *msg_iterator;

                    if(msg.substr(0,2) == "15") // generic response
                    {
                        if(msg.substr(3,2) == "OK") // will check what the message is about
                        {
                            std::string real_message = msg.substr(6);

                            if(real_message.find("Settings:")!=std::string::npos) // Threshold and gain is already set,read them
                            {
                                std::vector<std::string> threshold_vector;
                                _split(msg.substr(17),";",threshold_vector);
                                std::string &gain_string = threshold_vector[0];
                                std::string &threshold_string = threshold_vector[1];
                                std::vector<std::string> thr_val;
                                _split(threshold_string," ",thr_val);
                                std::string &threshold_value = thr_val[2];
                                m_threshold = atoi(threshold_value.c_str());

                                std::vector<std::string> gain_split;
                                _split(gain_string," ",gain_split);
                                int gain_size = gain_split.size();
                                std::string &gain_value = gain_split[0];
                                std::map<std::string,Gain>::iterator gFind = GAIN_SERVER_RESPONSE.find(gain_value);
                                m_gain = gFind->second;
                            }
                            else if(real_message.find("/tmp/setthreshold")!=std::string::npos)
                            {
                                m_state = Communication::OK;
                                _reinit(); // resync with server
                            }
                            else if(real_message.find("Exposure")!=std::string::npos)
                            {
                                int columnPos = real_message.find(":");
                                int lastSpace = real_message.rfind(" ");
                                if(real_message.substr(9,4) == "time")
                                {
                                    m_exposure = atof(real_message.substr(columnPos + 1,lastSpace).c_str());
                                }
                                else if(real_message.substr(9,6) == "period")
                                {
                                    m_exposure_period = atof(real_message.substr(columnPos + 1,lastSpace).c_str());
                                }
                                else // Exposures per frame
                                {
                                    m_exposure_per_frame = atoi(real_message.substr(columnPos + 1).c_str());\
                                }

                                m_state = Communication::OK;
                            }
                            else if(real_message.find("Delay")!=std::string::npos)
                            {
                                int columnPos = real_message.find(":");
                                int lastSpace = real_message.rfind(" ");
                                m_hardware_trigger_delay = atof(real_message.substr(columnPos + 1,lastSpace).c_str());
                                m_state = Communication::OK;
                            }
                            else if(real_message.find("N images")!=std::string::npos)
                            {
                                int columnPos = real_message.find(":");
                                m_nimages = atoi(real_message.substr(columnPos+1).c_str());
                                m_state = Communication::OK;
                            }
                            else if( m_state == Communication::SETTING_THRESHOLD)
                                m_state = Communication::OK;

                        }
                        else  // ERROR MESSAGE
                        {
                            if(m_state == Communication::SETTING_THRESHOLD)
                            {
                                m_state = Communication::OK;
                                DEB_TRACE() << "==> Threshold setting failed";
                            }
                            else if(m_state == Communication::SETTING_EXPOSURE)
                            {
                                m_state = Communication::OK;
                                DEB_TRACE() << "==> Exposure setting failed";
                            }
                            else
                                DEB_TRACE() << msg.substr(2);
                        }
                        m_cond.broadcast();
                    }
                    else if(msg.substr(0,2) == "13") //Acquisition Killed
                    {
                        DEB_TRACE() << "==> Acquisition Killed";
                        m_state = Communication::OK;
                    }
                    else
                    {
                        if(msg[0] == '7')
                        {
                            if(msg.substr(2,2) == "OK")
                            {
                                m_state = Communication::OK;
                                std::string real_message = msg.substr(5);
                                /*
                                if(real_message.find(m_imgpath)!=std::string::npos)
                                {
                                m_state = Communication::STANDBY;
                                }
                                */
                            }
                            else
                            {
                                if(m_state == Communication::KILL_ACQUISITION)
                                    m_state = Communication::OK;
                                else
                                {
                                    m_state = Communication::ERROR;
                                    msg = msg.substr(2);
                                    m_error_message = msg.substr(msg.find(" "));
                                }
                            }
                        }
                        else if(msg[0] == '1')
                        {
                            if(msg.substr(2,3) == "ERR")
                            {
                                if(m_state == Communication::KILL_ACQUISITION)
                                {
                                    m_state = Communication::OK;
                                }
                                else
                                {
                                    m_error_message = msg.substr(6);
                                    m_state = Communication::ERROR;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setImgpath(const std::string& path)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not set imgpath, server not idle");
    m_imgpath = path;    
    std::stringstream cmd;
    cmd<<"imgpath "<<m_imgpath;
    send(cmd.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const std::string& Communication::imgpath(void)
{
    AutoMutex aLock(m_cond.mutex());
    return m_imgpath;
}

//-----------------------------------------------------
//
//-----------------------------------------------------   
void Communication::setFileName(const std::string& name)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    m_file_name = name;
    m_file_pattern = m_file_name;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const std::string& Communication::fileName(void)
{
    AutoMutex aLock(m_cond.mutex());
    return m_file_name;
}
    
    
//-----------------------------------------------------
//
//-----------------------------------------------------
Communication::Status Communication::status() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_state;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const std::string& Communication::errorMessage() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_error_message;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::softReset()
{
    AutoMutex aLock(m_cond.mutex());
    m_error_message.clear();
    m_state = Communication::OK;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::hardReset()
{
    AutoMutex aLock(m_cond.mutex());
    send("resetcam");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Communication::threshold() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_threshold;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Communication::Gain Communication::gain() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_gain;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setThresholdGain(int value,Communication::Gain gain)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not set threshold, server is not idle");
    if(gain == DEFAULT_GAIN)
    {
        char buffer[128];
        snprintf(buffer,sizeof(buffer),"setthreshold %d",value);
        send(buffer);
    }
    else
    {
        std::map<Gain,std::string>::iterator i = GAIN_VALUE2SERVER.find(gain);
        std::string &gainStr = i->second;

        char buffer[128];
        snprintf(buffer,sizeof(buffer),"setthreshold %s %d",gainStr.c_str(),value);

        send(buffer);
        m_state = Communication::SETTING_THRESHOLD;
    }

    if (m_gap_fill)
        send("gapfill -1");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Communication::exposure() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_exposure;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setExposure(double val)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    // yet an other border-effect with the SPEC CCD interface
    // to reach the GATE mode SPEC programs extgate + expotime = 0
    if(m_trigger_mode == Communication::EXTERNAL_GATE and val <= 0)
        return;

    WAIT_UNTIL(Communication::OK,"Could not set exposure, server is not idle");
    m_state = Communication::SETTING_EXPOSURE;
    std::stringstream msg;
    msg << "exptime " << val;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Communication::exposurePeriod() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_exposure_period;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setExposurePeriod(double val)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not set exposure period, server not idle");
    m_state = Communication::SETTING_EXPOSURE_PERIOD;
    std::stringstream msg;
    msg << "expperiod " << val;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Communication::nbImagesInIequence() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_nimages;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setNbImagesInSequence(int nb)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not set number image in sequence, server not idle");
    m_state = Communication::SETTING_NB_IMAGE_IN_SEQUENCE;
    std::stringstream msg;
    msg << "nimages " << nb;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Communication::hardwareTriggerDelay() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_hardware_trigger_delay;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setHardwareTriggerDelay(double value)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not set hardware trigger delay, server not idle");
    m_state = Communication::SETTING_HARDWARE_TRIGGER_DELAY;
    std::stringstream msg;
    msg << "delay " << value;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Communication::nbExposurePerFrame() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_exposure_per_frame;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setNbExposurePerFrame(int val)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not set exposure per frame, server not idle");
    m_state = Communication::SETTING_EXPOSURE_PER_FRAME;
    std::stringstream msg;
    msg << "nexpframe " << val;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Communication::TriggerMode Communication::triggerMode() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_trigger_mode;
}

//-----------------------------------------------------
//@brief set the trigger mode
//
//Trigger can be:
// - Internal (Software) == INTERNAL
// - External start == EXTERNAL_START
// - External multi start == EXTERNAL_MULTI_START
// - External gate == EXTERNAL_GATE
//-----------------------------------------------------
void Communication::setTriggerMode(Communication::TriggerMode triggerMode)
{
    AutoMutex aLock(m_cond.mutex());
    m_trigger_mode = triggerMode;
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::startAcquisition(int image_number)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());

    if(m_state == Communication::RUNNING)
        THROW_HW_ERROR(Error) << "Could not start acquisition, you have to wait the end of the previous one";

    std::stringstream msg;
    if( m_trigger_mode != Communication::EXTERNAL_GATE)
    {
        while(m_exposure_period <= (m_exposure + 0.002999))
        {
            msg.clear();
            msg << "expperiod " << (m_exposure + 0.003);
            m_cond.wait(TIME_OUT);
        }
    }

    char filename[256];
    snprintf(filename,sizeof(filename),m_file_pattern.c_str(),image_number);

    msg.clear();
    //Start Acquisition
    if(m_trigger_mode == Communication::EXTERNAL_START)
    {
        msg << "exttrigger " << filename;
        send(msg.str());
    }
    else if(m_trigger_mode == Communication::EXTERNAL_MULTI_START)
    {
        msg << "extmtrigger " << filename;
        send(msg.str());
    }
    else if(m_trigger_mode == Communication::EXTERNAL_GATE)
    {
        msg << "extenable " << filename;
        send(msg.str());
    }
    else
    {
        msg << "exposure " << filename;
        send(msg.str());
    }

    if(m_trigger_mode != Communication::INTERNAL || m_trigger_mode != Communication::INTERNAL_TRIG_MULTI)
        m_cond.wait(TIME_OUT);

    m_state = Communication::RUNNING;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::stopAcquisition()
{
    AutoMutex aLock(m_cond.mutex());
    if(m_state == Communication::RUNNING)
    {
        m_state = Communication::KILL_ACQUISITION;
        send("k");
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::setGapfill(bool val)
{
    AutoMutex aLock(m_cond.mutex());
    m_gap_fill = val;
    std::stringstream msg;
    msg << "gapfill " << m_gap_fill ? -1 : 0;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool Communication::gapfill() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_gap_fill;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Communication::sendAnyCommand(const std::string& message)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    WAIT_UNTIL(Communication::OK,"Could not send the Command, server is not idle");

    send(message);
}

//-----------------------------------------------------
