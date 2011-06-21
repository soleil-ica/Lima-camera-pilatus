#ifndef PILATUSCOMMUNICATION_H
#define PILATUSCOMMUNICATION_H

#include "Debug.h"

namespace lima
{
namespace PilatusCpp
{
class Communication
{
    DEB_CLASS_NAMESPC(DebModCameraCom,"Communication","Pilatus");

public:
    enum Status
    {
        ERROR,
        DISCONNECTED,
        OK,
        SETTING_THRESHOLD,
        SETTING_EXPOSURE,
        SETTING_NB_IMAGE_IN_SEQUENCE,
        SETTING_EXPOSURE_PERIOD,
        SETTING_HARDWARE_TRIGGER_DELAY,
        SETTING_EXPOSURE_PER_FRAME,
        KILL_ACQUISITION,
        RUNNING
    };

    enum Gain
    {
        DEFAULT_GAIN,
        LOW,
        MID,
        HIGH,
        UHIGH
    };

    enum TriggerMode
    {
        INTERNAL,
        INTERNAL_TRIG_MULTI,
        EXTERNAL_START,
        EXTERNAL_MULTI_START,
        EXTERNAL_GATE
    };

    Communication(const char *host = NULL,int port = 0);
    ~Communication();
    
    void connect(const char* host,int port);
    
    std::string serverIP() const;
    int serverPort() const;
    
    void setImgpath(const std::string& path);
    const std::string& imgpath(void);
    
    void setFileName(const std::string& name);
    const std::string& fileName(void);
    
    Status status() const;

    int threshold() const;
    Gain gain() const;
    void setThresholdGain(int threshold,Gain gain = DEFAULT_GAIN);

    double exposure() const;
    void setExposure(double expo);

    double exposurePeriod() const;
    void setExposurePeriod(double expo_period);

    int nbImagesInIequence() const;
    void setNbImagesInSequence(int nb);

    double hardwareTriggerDelay() const;
    void setHardwareTriggerDelay(double);

    int nbExposurePerFrame() const;
    void setNbExposurePerFrame(int);

    TriggerMode triggerMode() const;
    void setTriggerMode(TriggerMode);

    void startAcquisition(int image_number = 0);
    void stopAcquisition();

    bool gapfill() const;
    void setGapfill(bool onOff);
    
    void send(const std::string& message);
    
    void sendAnyCommand(const std::string& message);    

    //s
    static const long long          DEFAULT_TMPFS_SIZE = 24LL * 1024 * 1024 * 1024;// 8Go
    static const double             TIME_OUT = 10.;
 
    
private:
    
    const        std::string& errorMessage() const;
    void         softReset();
    void         hardReset();
    void         quit();    
    
    static void* _runFunc(void*);
    void         _run();    
    void         _initVariable();
    void         _resync();
    void         _reinit();
    
    std::map<std::string,Gain>    GAIN_SERVER_RESPONSE;
    std::map<Gain,std::string>    GAIN_VALUE2SERVER;


    //socket/synchronization with pilatus variables
    std::string             m_server_ip;
    int                     m_server_port; 
    int                     m_socket;
    bool                    m_stop;
    pthread_t               m_thread_id;
    int                     m_pipes[2];
    Status                  m_state;
    mutable Cond            m_cond;

    //Cache variables
    std::string             m_error_message;
    double                  m_exposure;
    int                     m_exposure_per_frame;
    double                  m_exposure_period;
    Gain                    m_gain;
    bool                    m_gap_fill;
    double                  m_hardware_trigger_delay;
    int                     m_nimages;
    int                     m_threshold;
    TriggerMode             m_trigger_mode;
    std::string             m_imgpath;
    std::string             m_file_name;
    std::string             m_file_pattern;    
};
}
}
#endif//PILATUSCOMMUNICATION_H
