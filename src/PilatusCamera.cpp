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

#include "PilatusCamera.h"

using namespace lima;
using namespace lima::Pilatus;

static const char  SOCKET_SEPARATOR = '\030';
static const char* SPLIT_SEPARATOR  = "\x18"; // '\x18' == '\030'

//---------------------------
//- utility function
//---------------------------
static inline const char* _get_ip_addresse(const char *name_ip)
{
  
  if(inet_addr(name_ip) != INADDR_NONE)
    return name_ip;
  else
    {
      struct hostent *host = gethostbyname(name_ip);
      if(!host)
	{
	  char buffer[256];
	  snprintf(buffer,sizeof(buffer),"Can not found ip for host %s ",name_ip);
	  throw LIMA_HW_EXC(Error,buffer);
	}
      return inet_ntoa(*((struct in_addr*)host->h_addr));
    }
}


#define RECONNECT_WAIT_UNTIL(testState,errmsg)			    \
  if(m_socket == -1)						    \
    _connect(m_server_ip.c_str(),m_server_port);			    \
								    \
  while(m_state != testState &&					    \
	m_state != Camera::ERROR &&				    \
	m_state != Camera::DISCONNECTED)			    \
{                                                                   \
  if(!m_cond.wait(TIME_OUT))                                        \
    THROW_HW_ERROR(lima::Error) << errmsg;                          \
}

//-----------------------------------------------------
//
//-----------------------------------------------------
inline void _split(const std::string inString,
		   const std::string &separator,
		   std::vector<std::string> &returnVector)
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
Camera::Camera(const char *host,int port)
                :   m_socket(-1),
                    m_stop(false),
                    m_thread_id(0),
                    m_state(DISCONNECTED),
                    m_nb_acquired_images(0)
{
    DEB_CONSTRUCTOR();
    m_server_ip         = host;
    m_server_port       = port;
    _initVariable();

    if(pipe(m_pipes))
        THROW_HW_ERROR(Error) << "Can't open pipe";

    pthread_create(&m_thread_id,NULL,_runFunc,this);

    try
      {
	connect(host,port);
      }
    catch(Exception &e)		// Not an error in that case
      {
      }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Camera::~Camera()
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    m_nb_acquired_images = 0;
    m_stop = true;
    if(m_socket >=0)
    {
      if(write(m_pipes[1],"|",1) == -1)
	DEB_ERROR() << "Something wrong happen!!!";

      close(m_socket);
      m_socket = -1;
    }
    else
    {
      if(write(m_pipes[1],"|",1) == -1)
	DEB_ERROR() << "Something wrong happen!!!";
    }
    aLock.unlock();

    void *returnPt;
    if(m_thread_id > 0)
    {
        DEB_TRACE()<<"[pthread_join - BEGIN]";
        if(pthread_join(m_thread_id,&returnPt)!=0)
        {
            DEB_TRACE()<<"UNknown Error";
        }
        DEB_TRACE()<<"[pthread_join - END]";
    }
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const char* Camera::serverIP() const
{
  return m_server_ip.c_str();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Camera::serverPort() const
{
    return m_server_port;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::_initVariable()
{
    m_imgpath                           = "/ramdisk/images/";
    m_file_name                         = "image_%.5d.cbf";
    m_file_pattern                      = m_file_name;
    m_threshold                         = -1;
    m_energy                            = -1;
    m_gain                              = DEFAULT_GAIN;
    m_exposure                          = -1.;
    m_nimages                           = 1;
    m_exposure_period                   = -1.;
    m_hardware_trigger_delay            = -1.;
    m_exposure_per_frame                = 1;
    m_nb_acquired_images 		= 0;
    m_trigger_mode 			= INTERNAL_SINGLE;

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
void Camera::connect(const char *host,int port)
{
  DEB_MEMBER_FUNCT();
  AutoMutex aLock(m_cond.mutex());
  _initVariable();
  _connect(host,port);
}

void Camera::_connect(const char *host,int port)
{
    DEB_MEMBER_FUNCT();

    if(host && port)
    {
        if(m_socket >= 0)
        {
            close(m_socket);
            m_socket = -1;
        }
        else
        {
            m_socket = socket(PF_INET, SOCK_STREAM,IPPROTO_TCP);
            if(m_socket >= 0)
            {
                int flag = 1;
                setsockopt(m_socket,IPPROTO_TCP,TCP_NODELAY, (void*)&flag,sizeof(flag));
                struct sockaddr_in add;
                add.sin_family = AF_INET;
                add.sin_port = htons((unsigned short)port);
                add.sin_addr.s_addr = inet_addr(_get_ip_addresse(host));
                if(::connect(m_socket,reinterpret_cast<sockaddr*>(&add),sizeof(add)))
                {
                    close(m_socket);
                    m_socket = -1;
                    THROW_HW_ERROR(Error) << "Can't open connection";
                }
                if(write(m_pipes[1],"|",1) == -1)
		  DEB_ERROR() << "Something wrong happen to pipe ???";

                m_state = Camera::STANDBY;
                _resync();
            }
            else
                THROW_HW_ERROR(Error) << "Can't create socket";
        }
    }
    //Workaround to avoid bug in camserver
    send("exposure warmup.edf");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::_resync()
{
    if(m_has_cmd_setenergy)
      send("setenergy");
    else
      send("setthreshold");
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
void Camera::_reinit()
{
    _resync();
    send("nimages");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::send(const std::string& message)
{
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << DEB_VAR1(message);
    std::string msg = message;
    msg+= SOCKET_SEPARATOR;
    if(write(m_socket,msg.c_str(),msg.size()) == -1)
      THROW_HW_ERROR(Error) << "Could not send message to camserver";

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void* Camera::_runFunc(void *commPt)
{
    ((Camera*)commPt)->_run();
    return NULL;
}


/******************************************************
--> SUCCESS CASE (set number of images)
ni 10
15 OK N images set to: 10

--> SUCCESS CASE (get path where images are saved)
imgpath
10 OK /home/det/p2_det/images/

--> SUCCESS CASE (get path where images must be saved)
imgpath /ramdisk/tmp/Arafat
10 OK /ramdisk/tmp/Arafat/

--> SUCCESS CASE (set exposure period)
expp 0.2
15 OK Exposure period set to: 0.2000000 sec

--> SUCCESS CASE (set exposure time)
expt 0.997
15 OK Exposure time set to: 0.9970000 sec

--> SUCCESS CASE (start acquisition)
exposure toto_0001.cbf
15 OK  Starting 0.1970000 second background: 2011-Aug-04T15:27:22.593
7 OK /ramdisk/tmp/Arafat/toto_0300.cbf

--> SUCCESS CASE (abort acquisition)
exposure toto_0001.cbf
15 OK  Starting 0.1970000 second background: 2011-Aug-04T15:33:03.178
k
13 ERR kill
7 OK /ramdisk/tmp/Arafat/toto_0019.cbf

--> SUCCESS CASE (get threshold)
setthr
15 OK  Settings: mid gain; threshold: 6300 eV; vcmp: 0.654 V
 Trim file:
  /home/det/p2_det/config/calibration/p6m0106_E12600_T6300_vrf_m0p20.bin
 
--> SUCCESS CASE (set threshold to 6000)
setthr 6000
15 OK /tmp/setthreshold.cmd

--> ERROR CASE (send a bad command)
threshold
1 ERR *** Unrecognized command: threshold

--> ERROR CASE (bad parameters)
setthr low 5000
15 ERR ERROR: unknown gain setting: low 5000

TO DO :
1 - manage state for threshold : DONE -> OK
2 - resync() after threshold -> need manage state : DONE -> OK
3 - manage the stop() command like an abort :DONE -> OK
******************************************************/
void Camera::_run()
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
            if(read(m_pipes[0],buffer,sizeof(buffer)) == -1)
	      DEB_WARNING() << "Something strange happen!";
        }
        if(nb_poll_fd > 1 && fds[1].revents)
        {
            char messages[16384];
            int aMessageSize = recv(m_socket,messages,sizeof(messages),0);                    
            if(aMessageSize<=0)
            {
                DEB_TRACE() <<"-- no message received";
                close(m_socket);
                m_socket = -1;
                m_state = Camera::DISCONNECTED;                
            }
            else
            {                
                if(m_state == Camera::ERROR)//nothing to do, keep last error until a new explicit user command (start, stop, setenergy, ...)
                  continue;
                std::string strMessages(messages,aMessageSize );                    
                DEB_TRACE() << DEB_VAR1(strMessages);
                std::vector<std::string> msg_vector;
                _split(strMessages,SPLIT_SEPARATOR,msg_vector);
                for(std::vector<std::string>::iterator msg_iterator = msg_vector.begin();
		    msg_iterator != msg_vector.end();++msg_iterator)
                {
                    std::string &msg = *msg_iterator;

                    if(msg.substr(0,2) == "15") // generic response
                    {
                        if(msg.substr(3,2) == "OK") // will check what the message is about
                        {                            
                            std::string real_message = msg.substr(6);
			    size_t position;
                            if(real_message.find("Energy") != std::string::npos)
			    {
			      size_t columnPos = real_message.find(":");
			      if(columnPos == std::string::npos)
				{
				  m_threshold = m_energy = -1;
				  m_gain = DEFAULT_GAIN;
				}
			      else
				m_energy = atoi(real_message.substr(columnPos + 1).c_str());
			    }
                            if((position = real_message.find("Settings:")) !=
			       std::string::npos) // Threshold and gain is already set,read them
                            {
			        std::string submsg = real_message.substr(position);
                                std::vector<std::string> threshold_vector;
                                _split(submsg.substr(10),";",threshold_vector);
                                std::string &gain_string = threshold_vector[0];
                                std::string &threshold_string = threshold_vector[1];
                                std::vector<std::string> thr_val;
                                _split(threshold_string," ",thr_val);
                                std::string &threshold_value = thr_val[2];
                                m_threshold = atoi(threshold_value.c_str());

                                std::vector<std::string> gain_split;
                                _split(gain_string," ",gain_split);
                                std::string &gain_value = gain_split[0];
                                std::map<std::string,Gain>::iterator gFind = GAIN_SERVER_RESPONSE.find(gain_value);
                                m_gain = gFind->second;
                                m_state = Camera::STANDBY;                               
                            }
                            else if(real_message.find("/tmp/setthreshold")!=std::string::npos)
                            {
                                if(m_state == Camera::SETTING_THRESHOLD)
                                {
                                  DEB_TRACE() << "-- Threshold process succeeded";
                                }
                                if(m_state == Camera::SETTING_ENERGY)
                                {
                                  DEB_TRACE() << "-- SetEnergy process succeeded";
                                }
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
                            }
                            else if(real_message.find("Delay")!=std::string::npos)
                            {
                                int columnPos = real_message.find(":");
                                int lastSpace = real_message.rfind(" ");
                                m_hardware_trigger_delay = atof(real_message.substr(columnPos + 1,lastSpace).c_str());
                            }
                            else if(real_message.find("N images")!=std::string::npos)
                            {
                                int columnPos = real_message.find(":");
                                m_nimages = atoi(real_message.substr(columnPos+1).c_str());
                            }
			    if(m_state != Camera::RUNNING)
			      m_state = Camera::STANDBY;
                        }
                        else  // ERROR MESSAGE
                        {
                            if(m_state == Camera::SETTING_THRESHOLD)
			      DEB_TRACE() << "-- Threshold process failed";
                            if(m_state == Camera::SETTING_ENERGY)
			      DEB_TRACE() << "-- SetEnergy process failed";
                            else if(m_state == Camera::RUNNING)
			      DEB_TRACE() << "-- Exposure process failed";
                            else
                            {
			        DEB_TRACE() << "-- ERROR";
                                DEB_TRACE() << msg.substr(2);
                            }
			    m_state = Camera::ERROR;
			}
                        m_cond.broadcast();
                    }
                    else if(msg.substr(0,2) == "13") //Acquisition Killed
                    {
                        DEB_TRACE() << "-- Acquisition Killed"; 
                        m_state = Camera::STANDBY;
                    }
                    else if(msg.substr(0,2) == "7 ")
                    {
                        if(msg.substr(2,2) == "OK")
                        {
                            DEB_TRACE() << "-- Exposure succeeded";
                            m_state = Camera::STANDBY;
                            m_nb_acquired_images = m_nimages;
                        }
                        else
                        {
                            DEB_TRACE() << "-- ERROR";                      
                            m_state = Camera::ERROR;
                            msg = msg.substr(2);
                            m_error_message = msg.substr(msg.find(" "));
                        }
                    }
                    else if(msg.substr(0,2) == "1 ")
                    {
                        if(msg.substr(2,3) == "ERR")
                        {
			  // Not an error just old version of camserver
			  if(msg.find("Unrecognized command: setenergy") != 
			     std::string::npos)
			    {
			      m_has_cmd_setenergy = false;
			      _resync();
			    }
			  else
			    {
                            DEB_TRACE() << "-- ERROR";
                            m_error_message = msg.substr(6);
                            DEB_TRACE() << m_error_message;
                            m_state = Camera::ERROR;
			    }
                        }
                    }
                    else if(msg.substr(0,2) == "10")
                    {
                        if(msg.substr(3,2) == "OK")
                        {
                            DEB_TRACE() << "-- imgpath setting succeeded";
                            ////m_imgpath = msg.substr(6);////@@@@
                            m_state = Camera::STANDBY;
                        }
                        else
                        {
                            DEB_TRACE() << "-- ERROR";
                            m_state = Camera::ERROR;
                            msg = msg.substr(2);
                            m_error_message = msg.substr(msg.find(" "));
                            DEB_TRACE() << m_error_message;
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
void Camera::setImgpath(const std::string& path)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set imgpath, server not idle");
    m_imgpath = path;    
    std::stringstream cmd;
    cmd<<"imgpath "<<m_imgpath;
    send(cmd.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const std::string& Camera::imgpath() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_imgpath;
}

//-----------------------------------------------------
//
//-----------------------------------------------------   
void Camera::setFileName(const std::string& name)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    m_file_name = name;
    m_file_pattern = m_file_name;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const std::string& Camera::fileName() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_file_name;
}
    
    
//-----------------------------------------------------
//
//-----------------------------------------------------
Camera::Status Camera::status() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_state;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
const std::string& Camera::errorMessage() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_error_message;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::softReset()
{
    AutoMutex aLock(m_cond.mutex());
    m_error_message.clear();
    m_state = Camera::STANDBY;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::hardReset()
{
    AutoMutex aLock(m_cond.mutex());
    send("resetcam");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Camera::energy() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_energy;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setEnergy(double val)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set energy, server is not idle");
    if(m_has_cmd_setenergy)
      {
	m_state = Camera::SETTING_ENERGY;
	std::stringstream msg;
	msg << "setenergy " << val;
	send(msg.str());
      }
    else
      THROW_HW_ERROR(Error) << "This version of camserver don't have this feature";

}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Camera::threshold() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_threshold;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Camera::Gain Camera::gain() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_gain;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setThresholdGain(int value,Camera::Gain gain)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set threshold, server is not idle");
    m_state = Camera::SETTING_THRESHOLD;    
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
    }


    if (m_gap_fill)
        send("gapfill -1");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Camera::exposure() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_exposure;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setExposure(double val)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    // yet an other border-effect with the SPEC CCD interface
    // to reach the GATE mode SPEC programs extgate + expotime = 0
    if(m_trigger_mode == Camera::EXTERNAL_GATE and val <= 0)
        return;

    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set exposure, server is not idle");
    m_state = Camera::SETTING_EXPOSURE;
    std::stringstream msg;
    msg << "exptime " << val;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Camera::exposurePeriod() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_exposure_period;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setExposurePeriod(double val)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set exposure period, server not idle");
    m_state = Camera::SETTING_EXPOSURE_PERIOD;
    std::stringstream msg;
    msg << "expperiod " << val;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Camera::nbImagesInSequence() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_nimages;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setNbImagesInSequence(int nb)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set number image in sequence, server not idle");
    m_state = Camera::SETTING_NB_IMAGE_IN_SEQUENCE;
    std::stringstream msg;
    msg << "nimages " << nb;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
double Camera::hardwareTriggerDelay() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_hardware_trigger_delay;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setHardwareTriggerDelay(double value)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set hardware trigger delay, server not idle");
    m_state = Camera::SETTING_HARDWARE_TRIGGER_DELAY;
    std::stringstream msg;
    msg << "delay " << value;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Camera::nbExposurePerFrame() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_exposure_per_frame;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setNbExposurePerFrame(int val)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set exposure per frame, server not idle");
    m_state = Camera::SETTING_EXPOSURE_PER_FRAME;
    std::stringstream msg;
    msg << "nexpframe " << val;
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Camera::TriggerMode Camera::triggerMode() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_trigger_mode;
}

//-----------------------------------------------------
//@brief set the trigger mode
//
//Trigger can be:
// - Internal (Software) == INTERNAL_SINGLE
// - External start == EXTERNAL_SINGLE
// - External multi start == EXTERNAL_MULTI
// - External gate == EXTERNAL_GATE
//-----------------------------------------------------
void Camera::setTriggerMode(Camera::TriggerMode triggerMode)
{
    AutoMutex aLock(m_cond.mutex());
    m_trigger_mode = triggerMode;
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::startAcquisition(int image_number)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    m_nb_acquired_images = 0;
    if(m_state == Camera::RUNNING)
        THROW_HW_ERROR(Error) << "Could not start acquisition, you have to wait the end of the previous one";

    if( m_trigger_mode != Camera::EXTERNAL_GATE)
    {
        while(m_exposure_period <= (m_exposure + 0.002999))
        {
	  std::stringstream msg;
	  msg << "expperiod " << (m_exposure + 0.003);
	  send(msg.str());
	  m_cond.wait(TIME_OUT);
        }
    }

    char filename[256];
    snprintf(filename,sizeof(filename),m_file_pattern.c_str(),image_number);

    //Start Acquisition
 
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not start Acquisition, server not idle");    
    m_state = Camera::RUNNING;
    std::stringstream msg;

    if(m_trigger_mode == Camera::EXTERNAL_SINGLE)
      msg << "exttrigger " << filename;
    else if(m_trigger_mode == Camera::EXTERNAL_MULTI)
      msg << "extmtrigger " << filename;
    else if(m_trigger_mode == Camera::EXTERNAL_GATE)
      msg << "extenable " << filename;
    else
      msg << "exposure " << filename;

    send(msg.str());
    if(m_trigger_mode != Camera::INTERNAL_SINGLE || 
       m_trigger_mode != Camera::INTERNAL_MULTI)
        m_cond.wait(TIME_OUT);

}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::stopAcquisition()
{
    AutoMutex aLock(m_cond.mutex());
    if(m_state == Camera::RUNNING)
    {
        m_state = Camera::KILL_ACQUISITION;
        send("k");
    }
}

void Camera::errorStopAcquisition()
{
  stopAcquisition();
  m_state = Camera::ERROR;
}
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setGapfill(bool val)
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not set gap, server not idle");
    m_gap_fill = val;
    std::stringstream msg;
    msg << "gapfill " << (m_gap_fill ? -1 : 0);
    send(msg.str());
}

//-----------------------------------------------------
//
//-----------------------------------------------------
bool Camera::gapfill() const
{
    AutoMutex aLock(m_cond.mutex());
    return m_gap_fill;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::sendAnyCommand(const std::string& message)
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    RECONNECT_WAIT_UNTIL(Camera::STANDBY,
			 "Could not send the Command, server is not idle");

    send(message);
}

std::string Camera::sendAnyCommandAndGetErrorMsg(const std::string& message)
{
  DEB_MEMBER_FUNCT();

  AutoMutex aLock(m_cond.mutex());
  RECONNECT_WAIT_UNTIL(Camera::STANDBY,
		       "Could not send the Command, server is not idle");

  m_state = Camera::ANYCMD;
  send(message);

  while(m_state != Camera::STANDBY &&
	m_state != Camera::ERROR &&
	m_state != Camera::DISCONNECTED)
    {
      if(!m_cond.wait(TIME_OUT))
	return "Timeout";
    }

  if(m_state == Camera::ERROR)
    return m_error_message;
  else if(m_state == Camera::DISCONNECTED)
    return "Disconnected";
  else
    return "";
}
//-----------------------------------------------------
//
//-----------------------------------------------------
int Camera::nbAcquiredImages() const
{
    DEB_MEMBER_FUNCT();

    AutoMutex aLock(m_cond.mutex());
    return m_nb_acquired_images;
}

//-----------------------------------------------------
