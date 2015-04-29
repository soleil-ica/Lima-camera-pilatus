#ifndef PILATUSINTERFACE_H
#define PILATUSINTERFACE_H

#include "lima/HwInterface.h"
#include "lima/HwBufferMgr.h"
#include "lima/Debug.h"
#include "processlib/Data.h"
#include "PilatusCamera.h"
#include "PilatusReader.h"

using namespace lima;
using namespace lima::Pilatus;
using namespace std;

#define LATENCY_DEFAULT_VALUE           0.003

#define PILATUS_MODEL                   "PILATUS_6M"

#define PILATUS_6M_WIDTH                2463
#define PILATUS_6M_HEIGHT               2527

#define PILATUS_3M_WIDTH                981
#define PILATUS_3M_HEIGHT               1043

namespace lima
{
namespace Pilatus
{
class Interface;
/*******************************************************************
 * \class DetInfoCtrlObj
 * \brief Control object providing Pilatus detector info interface
 *******************************************************************/

class DetInfoCtrlObj : public HwDetInfoCtrlObj
{
    DEB_CLASS_NAMESPC(DebModCamera, "DetInfoCtrlObj", "Pilatus");

public:
    DetInfoCtrlObj(Camera& cam);
    virtual ~DetInfoCtrlObj();

    virtual void getMaxImageSize(Size& max_image_size);
    virtual void getDetectorImageSize(Size& det_image_size);

    virtual void getDefImageType(ImageType& def_image_type);
    virtual void getCurrImageType(ImageType& curr_image_type);
    virtual void setCurrImageType(ImageType curr_image_type);

    virtual void getPixelSize(double& x_size, double &y_size);
    virtual void getDetectorType(std::string& det_type);
    virtual void getDetectorModel(std::string& det_model);
    virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
    {
        ;
    }
    ;
    virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
    {
        ;
    }
    ;

private:
    Camera& m_cam;
    Size m_det_size;
    std::string m_det_model;
    int m_bpp;
};
/*******************************************************************
 * \class BufferCtrlObj
 * \brief Control object providing Pilatus buffering interface
 *******************************************************************/

class BufferCtrlObj : public HwBufferCtrlObj
{
    DEB_CLASS_NAMESPC(DebModCamera, "BufferCtrlObj", "Pilatus");

public:
    BufferCtrlObj(Camera& cam, DetInfoCtrlObj& det);
    virtual ~BufferCtrlObj();

    void start();
    void stop();
    void reset();
    virtual void setFrameDim(const FrameDim& frame_dim);
    virtual void getFrameDim(FrameDim& frame_dim);

    virtual void setNbBuffers(int nb_buffers);
    virtual void getNbBuffers(int& nb_buffers);

    virtual void setNbConcatFrames(int nb_concat_frames);
    virtual void getNbConcatFrames(int& nb_concat_frames);

    virtual void getMaxNbBuffers(int& max_nb_buffers);

    virtual void *getBufferPtr(int buffer_nb, int concat_frame_nb = 0);
    virtual void *getFramePtr(int acq_frame_nb);

    virtual void getStartTimestamp(Timestamp& start_ts);
    virtual void getFrameInfo(int acq_frame_nb, HwFrameInfoType& info);

    // -- Buffer control bject
    BufferCtrlMgr& getBufferMgr()
    {
        return m_buffer_ctrl_mgr;
    }
    ;
    StdBufferCbMgr& getBufferCbMgr()
    {
        return m_buffer_cb_mgr;
    }
    ;
    int getLastAcquiredFrame();
    bool isTimeoutSignaled(void);
    void setTimeout(double TO);
    bool isRunning(void);
    virtual void registerFrameCallback(HwFrameCallback& frame_cb);
    virtual void unregisterFrameCallback(HwFrameCallback& frame_cb);
private:
    int m_nb_buffer;
    SoftBufferAllocMgr m_buffer_alloc_mgr;
    StdBufferCbMgr m_buffer_cb_mgr;
    BufferCtrlMgr m_buffer_ctrl_mgr;
    Camera& m_cam;
    DetInfoCtrlObj& m_det;
    Reader* m_reader;
};
/*******************************************************************
 * \class SyncCtrlObj
 * \brief Control object providing Pilatus synchronization interface
 *******************************************************************/

class SyncCtrlObj : public HwSyncCtrlObj
{
    DEB_CLASS_NAMESPC(DebModCamera, "SyncCtrlObj", "Pilatus");

public:

    SyncCtrlObj(Camera& cam);
    virtual ~SyncCtrlObj();

    virtual bool checkTrigMode(TrigMode trig_mode);
    virtual void setTrigMode(TrigMode trig_mode);
    virtual void getTrigMode(TrigMode& trig_mode);

    virtual void setExpTime(double exp_time);
    virtual void getExpTime(double& exp_time);

    virtual void setLatTime(double lat_time);
    virtual void getLatTime(double& lat_time);

    virtual void setNbHwFrames(int nb_frames);
    virtual void getNbHwFrames(int& nb_frames);

    virtual void getValidRanges(ValidRangesType& valid_ranges);

    void prepareAcq();

private:
    Camera& m_cam;
    int m_nb_frames;
    double m_exposure_requested;
    double m_latency;
};
/*******************************************************************
 * \class Interface
 * \brief Pilatus hardware interface
 *******************************************************************/

class Interface : public HwInterface
{
    DEB_CLASS_NAMESPC(DebModCamera, "PilatusInterface", "Pilatus");

public:
    Interface(Camera& cam);
    virtual ~Interface();

    //- From HwInterface
    virtual void getCapList(CapList&) const;
    virtual void reset(ResetLevel reset_level);
    virtual void prepareAcq();
    virtual void startAcq();
    virtual void stopAcq();
    virtual void getStatus(StatusType& status);
    virtual int getNbHwAcquiredFrames();

    //Specific pilatus funtions

    void setEnergy(double energy);
    double getEnergy(void);
    void setThresholdGain(int threshold, Camera::Gain gain);
    int getThreshold(void);
    Camera::Gain getGain(void);
    void sendAnyCommand(const std::string& str);
    Camera& getCamera()
    {
        return m_cam;
    }
    const Camera& getCamera() const
    {
        return m_cam;
    }
    void setTimeout(double TO);

private:
    Camera& m_cam;
    CapList m_cap_list;
    DetInfoCtrlObj m_det_info;
    BufferCtrlObj m_buffer;
    SyncCtrlObj m_sync;
    double m_latency;
};

} // namespace Pilatus
} // namespace lima

#endif // PILATUSINTERFACE_H
