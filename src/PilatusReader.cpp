#include <yat/threading/Mutex.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <math.h>
#include "PilatusReader.h"
#include "PilatusInterface.h"


//---------------------------
//- Ctor
//---------------------------
Reader::Reader(Camera& cam, HwDetInfoCtrlObj& detinfo, HwBufferCtrlObj& buffer_ctrl)
: yat::Task(Config(false, //- disable timeout msg
                   kTASK_PERIODIC_MS, //- every second (i.e. 1000 msecs)
                   false, //- enable periodic msgs
                   kTASK_PERIODIC_TIMEOUT_MS, //- every second (i.e. 1000 msecs)
                   false, //- don't lock the internal mutex while handling a msg (recommended setting)
                   kDEFAULT_LO_WATER_MARK, //- msgQ low watermark value
                   kDEFAULT_HI_WATER_MARK, //- msgQ high watermark value
                   false, //- do not throw exception on post msg timeout (msqQ saturated)
                   0)), //- user data (same for all msgs) 
m_cam(cam),
m_det_info(detinfo),
m_buffer(buffer_ctrl)
{
    DEB_CONSTRUCTOR();
    try
    {
        m_det_info.getMaxImageSize(m_image_size);
        m_image_number = -1;
        m_timeout_value = kDEFAULT_READER_TIMEOUT_MSEC / 1000.;
        m_is_reader_watcher = m_cam.isReaderWatcher();
    }
    catch(yat::Exception& ex)
    {
        // Error handling
        DEB_ERROR() << ex.errors[0].desc;
        throw LIMA_HW_EXC(Error, ex.errors[0].desc);
    }
}

//---------------------------
//- Dtor
//---------------------------
Reader::~Reader()
{
    DEB_DESTRUCTOR();
    try
    {
        //NOP !
    }
    catch(yat::Exception& ex)
    {
        // Error handling
        DEB_ERROR() << ex.errors[0].desc;
        throw LIMA_HW_EXC(Error, ex.errors[0].desc);
    }
}

//---------------------------
//- Reader::start()
//---------------------------
void Reader::start()
{
    DEB_MEMBER_FUNCT();
    try
    {
        double eTime;
        eTime = m_cam.exposure();
        yat::MutexLock scoped_lock(m_lock);
        DEB_TRACE() << "eTime = " << eTime;
        DEB_TRACE() << "set timeout value = " << (eTime + m_timeout_value)*1000. << " ms";
        m_timeout.set_value((eTime + m_timeout_value)*1000.); //*1000. because m_timeout is in ms
        post(new yat::Message(PILATUS_START_MSG), kPOST_MSG_TMO);
    }
    catch(Exception &e)
    {
        // Error handling
        DEB_ERROR() << e.getErrMsg();
        throw LIMA_HW_EXC(Error, e.getErrMsg());
    }
    catch(yat::Exception &ex)
    {
        // Error handling
        DEB_ERROR() << ex.errors[0].desc;
        throw LIMA_HW_EXC(Error, ex.errors[0].desc);
    }
}

//---------------------------
//- Reader::stop()
//---------------------------
void Reader::stop()
{
    DEB_MEMBER_FUNCT();
    try
    {
        {
            yat::MutexLock scoped_lock(m_lock);
            m_stop_request = true;
        }

        post(new yat::Message(PILATUS_STOP_MSG), kPOST_MSG_TMO);
    }
    catch(yat::Exception& ex)
    {
        // Error handling
        DEB_ERROR() << ex.errors[0].desc;
        throw LIMA_HW_EXC(Error, ex.errors[0].desc);
    }
}

//---------------------------
//- Reader::reset()
//---------------------------
void Reader::reset()
{
    DEB_MEMBER_FUNCT();
    try
    {
        post(new yat::Message(PILATUS_RESET_MSG), kPOST_MSG_TMO);
    }
    catch(yat::Exception& ex)
    {
        // Error handling
        DEB_ERROR() << ex.errors[0].desc;
        throw LIMA_HW_EXC(Error, ex.errors[0].desc);
    }
}

//---------------------------
//- Reader::getLastAcquiredFrame()
//---------------------------
int Reader::getLastAcquiredFrame(void)
{
    DEB_MEMBER_FUNCT();
    yat::MutexLock scoped_lock(m_lock);
    return m_image_number;
}

//---------------------------
//- Reader::isTimeoutSignaled()
//---------------------------
bool Reader::isTimeoutSignaled()
{
    DEB_MEMBER_FUNCT();
    yat::MutexLock scoped_lock(m_lock);
    return m_timeout.expired();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Reader::setTimeout(double timeout_val)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(timeout_val);
    yat::MutexLock scoped_lock(m_lock);
    m_timeout_value = timeout_val;
}

//---------------------------
//- Reader::isRunning()
//---------------------------
bool Reader::isRunning(void)
{
    DEB_MEMBER_FUNCT();
    yat::MutexLock scoped_lock(m_lock);
    return periodic_msg_enabled();
}

//---------------------------
//- Reader::isStopRequest()
//---------------------------
bool Reader::isStopRequest(void)
{
    DEB_MEMBER_FUNCT();
    yat::MutexLock scoped_lock(m_lock);
    return m_stop_request;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Reader::handle_message(yat::Message& msg) throw(yat::Exception)
{
    DEB_MEMBER_FUNCT();
    try
    {
        switch(msg.type())
        {
                //-----------------------------------------------------
            case yat::TASK_INIT:
            {
                DEB_TRACE() << "Reader::->TASK_INIT";
                //- set unit in seconds
                m_timeout.set_unit(yat::Timeout::TMO_UNIT_MSEC);
                //- set default timeout value
                m_timeout.set_value(kDEFAULT_READER_TIMEOUT_MSEC);
            }
                break;
                //-----------------------------------------------------
            case yat::TASK_EXIT:
            {
                DEB_TRACE() << "Reader::->TASK_EXIT";
            }
                break;
                //-----------------------------------------------------
            case yat::TASK_TIMEOUT:
            {
                DEB_TRACE() << "Reader::->TASK_TIMEOUT";
            }
                break;
                //-----------------------------------------------------
            case yat::TASK_PERIODIC:
            {
                DEB_TRACE() << " ";
                DEB_TRACE() << "-----------------------";
                DEB_TRACE() << "Reader::->TASK_PERIODIC";

                
                if(m_is_reader_watcher) // need to read *.tif files in order to display the frames
                {
                    DEB_TRACE() << "Check if stop is requested";
                    //- check if stop is requested
                    if(isStopRequest())
                        break;

                    DEB_TRACE() << "Check if timeout expired";
                    //- check if timeout expired                    
                    if (m_timeout.expired())
                    {
                        DEB_TRACE() << "-- Failed to load image : timeout expired !";
                        //- disable periodic msg
                        enable_periodic_msg(false);
                        return;
                    }

                    //- get full image name as full/path/pattern_%5d.tif
                    std::string full_pattern = m_cam.imgpath() + "/" + m_cam.fileName();
                    std::string full_file_name(255, ' ');
                    sprintf(const_cast<char*>(full_file_name.data()), full_pattern.c_str(), m_image_number);
                    std::remove(full_file_name.begin(), full_file_name.end(), ' '); //trim whitespace

                    DEB_TRACE() << "Force refreshing of nfs file system using ls command ";
                    // Force nfs file system to refresh !!
                    std::stringstream lsCommand;
                    lsCommand << "ls " << m_cam.imgpath() << " 2>&1 > /dev/null";
                    system(lsCommand.str().c_str());

                    DEB_TRACE() << "Check if file : [" << full_file_name << "] exist ?";
                    //- check if file exist
                    std::ifstream ifFile(full_file_name.c_str(), ios_base::in);
                    if (ifFile)
                    {
                        //foreach file generated by CamServer & found by Reader, a new frame is added
                        DEB_TRACE() << "-- Found File [" << full_file_name << "]";
                        DEB_TRACE() << "-- Add New Frame ...";
                        addNewFrame(full_file_name);
                    }
                }
                else // need to simulate a nb_frame "null" frames at the end of camserver exposure & once for all
                {
                    m_timeout.disable();
                    DEB_TRACE() << "Check response of CamServer at the end of exposure";
                    if(m_cam.nbAcquiredImages() != 0 && !isStopRequest())
                    {
                        DEB_TRACE() << "-- Exposure succeded received from CamServer !"; //all images (nbImagesInSequence()) are acquired !
                        DEB_TRACE() << "-- Process all SIMULATED frames :";
                        for(int i = 0; i < m_cam.nbImagesInSequence(); i++)
                        {
                            //- check if stop is requested
                            if(isStopRequest())
                                break;
                            DEB_TRACE() << "-- Add New Frame ...";
                            addNewFrame();
                        }
                    }
                }
            }
                break;
                //-----------------------------------------------------
            case PILATUS_START_MSG:
            {
                DEB_TRACE() << "Reader::->PILATUS_START_MSG";
                yat::MutexLock scoped_lock(m_lock);
                {
                    m_stop_request = false;
                    //initialisze image_number when first image arrived					
                    m_image_number = 0;
                }

                //- re-arm timeout
                m_timeout.restart();
                enable_periodic_msg(true);
            }
                break;
                //-----------------------------------------------------
            case PILATUS_STOP_MSG:
            {
                DEB_TRACE() << "Reader::->PILATUS_STOP_MSG";
                if(m_is_reader_watcher)
                {
                    // Remove *.tif files in the directory               
                    DEB_TRACE() << "Remove '*.tif' files in the directory defined by 'imagePath ...";
                    std::stringstream rmCommand;
                    rmCommand  	<< "rm -f "<< m_cam.imgpath()<< "/*.tif";
                    //<< " >& /dev/null" ; 	// & avoid print out
                    system(rmCommand.str().c_str());
                    DEB_TRACE() << rmCommand.str() << " done.";
                }
                enable_periodic_msg(false);
                m_timeout.disable();
            }
                break;
                //-----------------------------------------------------
            case PILATUS_RESET_MSG:
            {
                DEB_TRACE() << "Reader::->PILATUS_RESET_MSG";
                enable_periodic_msg(false);
                m_timeout.disable();
            }
                break;
                //-----------------------------------------------------
        }
    }
    catch(yat::Exception& ex)
    {
        DEB_ERROR() << "Error : " << ex.errors[0].desc;
        throw;
    }
}

//---------------------------
//- Reader::addNewFrame()
//---------------------------
void Reader::addNewFrame(const std::string & file_name)
{
    DEB_MEMBER_FUNCT();
    try
    {
        StdBufferCbMgr& buffer_mgr = ((reinterpret_cast<BufferCtrlObj&>(m_buffer)).getBufferCbMgr());
        bool continueAcq = false;
        int buffer_nb, concat_frame_nb;
        DEB_TRACE() << "-- #image n°: " << m_image_number << " acquired !";
        buffer_mgr.setStartTimestamp(Timestamp::now());
        buffer_mgr.acqFrameNb2BufferNb(m_image_number, buffer_nb, concat_frame_nb);

        //simulate an image !
        DEB_TRACE() << "-- prepare image buffer";
        void *ptr = buffer_mgr.getBufferPtr(buffer_nb, concat_frame_nb);

        if(m_is_reader_watcher)
        {
            //read image file using diffractionImage library
            DEB_TRACE() << "-- read Tiff image created by the CamServer";
            readTiff(file_name, (int32_t*)ptr);
        }
        else
        {
            yat::ThreadingUtilities::sleep(0, 5000000); //5 ms
        }

        DEB_TRACE() << "-- newFrameReady";
        HwFrameInfoType frame_info;
        frame_info.acq_frame_nb = m_image_number;

        if(!isStopRequest())
            continueAcq = buffer_mgr.newFrameReady(frame_info);
        else
            continueAcq = false;


        // if nb acquired image < requested frames
        if(continueAcq && (!m_cam.nbImagesInSequence() || m_image_number < (m_cam.nbImagesInSequence() - 1)))
        {
            DEB_TRACE() << "All requested frames (" << m_cam.nbImagesInSequence() << ") were not acquired";
            yat::MutexLock scoped_lock(m_lock);
            {
                DEB_TRACE() << "-- increase image_number";
                m_image_number++;
                m_timeout.restart();
            }
        }
        else
        {
            DEB_TRACE() << "All requested frames (" << m_cam.nbImagesInSequence() << ") are acquired OR Stop is requested";
            DEB_TRACE() << "-- stop watching ";
            stop();
        }
        DEB_TRACE() << "-------------------------\n";
    }
    catch(yat::Exception& ex)
    {
        // Error handling
        DEB_ERROR() << ex.errors[0].desc;
        throw LIMA_HW_EXC(Error, ex.errors[0].desc);
    }
    return;
}

//-----------------------------------------------------
void Reader::dummyHandler(const char* module, const char* fmt, va_list ap)
{
    DEB_MEMBER_FUNCT();
    // ignore errors and warnings (or handle them your own way)
    DEB_TRACE() << "module :" << module;
    DEB_TRACE() << "fmt :" << fmt ;
}

//-----------------------------------------------------
void Reader::readTiff(const std::string& file_name, void *ptr)
{
    DEB_MEMBER_FUNCT();
    uint32 width, height;
    tdata_t buf;
    uint32 row, col;
    short config = -1, nbbitspersample = -1, compression = -1, nsamples = -1, sampleformat = -1;

    TIFFSetErrorHandler(NULL);
    DEB_TRACE() << "-- Loading image : " << file_name.c_str();
    TIFF *input_image = TIFFOpen(file_name.c_str(), "r");

    if(input_image)
    {
        TIFFGetField(input_image, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(input_image, TIFFTAG_IMAGELENGTH, &height);
        TIFFGetField(input_image, TIFFTAG_COMPRESSION, &compression);
        TIFFGetField(input_image, TIFFTAG_PLANARCONFIG, &config);
        TIFFGetField(input_image, TIFFTAG_SAMPLEFORMAT, &sampleformat);
        TIFFGetField(input_image, TIFFTAG_SAMPLESPERPIXEL, &nsamples);
        TIFFGetField(input_image, TIFFTAG_BITSPERSAMPLE, &nbbitspersample);

        DEB_TRACE()  << "width              = " << width;
        DEB_TRACE()  << "height             = " << height;
        DEB_TRACE()  << "compression        = " << compression;
        DEB_TRACE()  << "sampleformat       = " << sampleformat;
        DEB_TRACE()  << "nbbitspersample    = " << nbbitspersample;

        if(m_image_size.getWidth() != width || m_image_size.getHeight() != height)
            throw LIMA_HW_EXC(Error, "Image size in file is different from the expected image size of this detector !");

        buf = _TIFFmalloc(TIFFScanlineSize(input_image));

        DEB_TRACE() << "-- copy image in buffer";
        for(row = 0; row < height; row++)
        {
            TIFFReadScanline(input_image, buf, row);

            if((nbbitspersample == 32) &&
               (sampleformat == SAMPLEFORMAT_INT) &&
               (compression == COMPRESSION_NONE) &&
               (config == PLANARCONFIG_CONTIG))
            {
                memcpy(&((int32_t*)ptr)[row * width], (int32_t*)buf, width * 4);
            }
            else
            {
                //- throw exception
                std::stringstream ssErr("");
                ssErr << "This Image format : \n";
                ssErr << "nbbitspersample = " << nbbitspersample << "\n";
                ssErr << "sampleformat = " << sampleformat << "\n";
                ssErr << "compression = " << ((compression == COMPRESSION_NONE) ? "NONE" : "COMPRESSED") << "\n";
                ssErr << "config = " << ((config == PLANARCONFIG_CONTIG) ? "PLANAR" : "SEPARATE") << "\n";
                ssErr << "is not managed ! " << endl;
                throw LIMA_HW_EXC(Error, ssErr.str());
            }
        }

        _TIFFfree(buf);
        TIFFClose(input_image);
    }
}

//-----------------------------------------------------
