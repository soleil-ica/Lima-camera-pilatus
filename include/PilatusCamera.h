#ifndef PILATUSCAMERA_H
#define PILATUSCAMERA_H

#include "Debug.h"



namespace lima
{
namespace Pilatus
{
class Camera
{
    DEB_CLASS_NAMESPC(DebModCameraCom,"Camera","Pilatus");

public:
    enum Status
    {
        ERROR,
        DISCONNECTED,
        STANDBY,
        SETTING_ENERGY,
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
        INTERNAL_SINGLE,
        INTERNAL_MULTI,
        EXTERNAL_SINGLE,
        EXTERNAL_MULTI,
        EXTERNAL_GATE
    };

    Camera(const char *host = "localhost",int port = 41234);
    ~Camera();
    
    void connect(const char* host,int port);
    
    const char* serverIP() const;
    int serverPort() const;

    void setImgpath(const std::string& path);
    const std::string& imgpath() const;
    
    void setFileName(const std::string& name);
    const std::string& fileName() const;
    
    Status status() const;

    double energy() const;
    void setEnergy(double val);

    int threshold() const;
    Gain gain() const;
    void setThresholdGain(int threshold,Gain gain = DEFAULT_GAIN);

    double exposure() const;
    void setExposure(double expo);

    double exposurePeriod() const;
    void setExposurePeriod(double expo_period);

    int nbImagesInSequence() const;
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

    int nbAcquiredImages() const;
    
private:
    static const double             TIME_OUT = 10.;

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
    double                  m_energy;
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
    int						m_nb_acquired_images;
};
}
}
#endif//PILATUSCAMERA_H
