
#include <yat/threading/Mutex.h>
#include <sstream>
#include <iostream>
#include <string>
#include <math.h>
#include "Debug.h"
#include "Data.h"
#include "PilatusReader.h"
#include "PilatusInterface.h"




//---------------------------
//- Ctor
//---------------------------
Reader::Reader(Communication& com, HwBufferCtrlObj& buffer_ctrl)
	  : m_com(com),
		m_buffer(buffer_ctrl),
		m_stop_already_done(true),
		my_file_image("/home/informatique/ica/noureddine/DeviceSources/data/image.txt"),
		raw_data("")
{
	DEB_CONSTRUCTOR();
	try
    {
	  m_image_number = 0;
	  enable_timeout_msg(false);
	  enable_periodic_msg(false);
	  set_periodic_msg_period(kTASK_PERIODIC_TIMEOUT_MS);
	}
	catch (Exception &e)
    {
	  // Error handling
	  DEB_ERROR() << e.getErrMsg();
	  throw LIMA_HW_EXC(Error, e.getErrMsg());
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
	}
	catch (Exception &e)
    {
        // Error handling
		DEB_ERROR() << e.getErrMsg();
        throw LIMA_HW_EXC(Error, e.getErrMsg());
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
		m_image_number = 0;
		m_stop_already_done = false;
		vData.clear();
		vLineData.clear();
		raw_data.clear();
	  	my_file_image.Load(&raw_data);		
		this->post(new yat::Message(PILATUS_START_MSG), kPOST_MSG_TMO);
	}
	catch (Exception &e)
    {
        // Error handling
		DEB_ERROR() << e.getErrMsg();
        throw LIMA_HW_EXC(Error, e.getErrMsg());
    }
}

//---------------------------
//- Reader::stop()
//---------------------------
void Reader::stop()
{
	DEB_MEMBER_FUNCT();
	{
		this->post(new yat::Message(PILATUS_STOP_MSG), kPOST_MSG_TMO);
	}
}

//---------------------------
//- Reader::reset()
//---------------------------
void Reader::reset()
{
	DEB_MEMBER_FUNCT();
	{
		this->post(new yat::Message(PILATUS_RESET_MSG), kPOST_MSG_TMO);
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Reader::handle_message( yat::Message& msg )  throw( yat::Exception )
{
  DEB_MEMBER_FUNCT();  
  try
  {
    switch ( msg.type() )
    {
      //-----------------------------------------------------	
      case yat::TASK_INIT:
      {
        DEB_TRACE() <<"Reader::->TASK_INIT";
      }
      break;
      //-----------------------------------------------------    
      case yat::TASK_EXIT:
      {
        DEB_TRACE() <<"Reader::->TASK_EXIT";
      }
      break;
      //-----------------------------------------------------    
      case yat::TASK_TIMEOUT:
      {
		DEB_TRACE() <<"Reader::->TASK_TIMEOUT";
      }
      break;
      //-----------------------------------------------------    
      case yat::TASK_PERIODIC:
      {
		DEB_TRACE() <<"Reader::->TASK_PERIODIC";
		if(m_stop_already_done)
		  return;	

		//simulate an image !		
		Size image_size(2463,2527);
		
		bool continueAcq = false;
		int m_nb_frames = -1;
		StdBufferCbMgr& buffer_mgr = ((reinterpret_cast<BufferCtrlObj&>(m_buffer)).getBufferCbMgr());
		buffer_mgr.getNbBuffers(m_nb_frames);

		if(m_image_number==0)
		{
		  pImage = new uint32_t[image_size.getWidth()*image_size.getHeight()];
		  memset((uint32_t*)pImage,0,image_size.getWidth()*image_size.getHeight()*4);
		  raw_data.Split('\n',&vData);
  
		  for(int i=0;i<2527;i++)
		  {
			vData.at(i).Split(';',&vLineData);
			for(int j=0;j<2463;j++)
			{
			  pImage[i*2463+j]= (uint32_t)atoi(vLineData.at(j).c_str());
			}
		  }
		}
		

		DEB_TRACE()  << "image#" << DEB_VAR1(m_image_number) <<" acquired !";		
		int buffer_nb, concat_frame_nb;		
		buffer_mgr.setStartTimestamp(Timestamp::now());
		buffer_mgr.acqFrameNb2BufferNb(m_image_number, buffer_nb, concat_frame_nb);

		void *ptr = buffer_mgr.getBufferPtr(buffer_nb,   concat_frame_nb);
		memcpy((uint32_t *)ptr,(uint32_t *)( pImage),image_size.getWidth()*image_size.getHeight()*4);//*4 because 32bits

		HwFrameInfoType frame_info;
		frame_info.acq_frame_nb = m_image_number;
		continueAcq = buffer_mgr.newFrameReady(frame_info);
		m_image_number++;

		// if nb acquired image < requested frames
		if (continueAcq &&(!m_nb_frames || m_image_number<m_nb_frames))
		{
			//NOP
		}
		else
		{
			delete[]  pImage;			
			enable_periodic_msg(false);		  
		  	this->post(new yat::Message(PILATUS_STOP_MSG), kPOST_MSG_TMO);
		}
		/********************************************
		int nextFrameId = m_last_image_read + 1;
		Data data = Data();
		data.frameNumber = nextFrameId;
		const Size &aSize = image_size;
		data.width = aSize.getWidth();
		data.height = aSize.getHeight();
		data.type = Data::UINT32;

		Buffer *buffer = new Buffer();
		buffer->owner = Buffer::MAPPED;
		buffer->data = (void*)pImage;
		data.setBuffer(buffer);

		FrameDim frame_dim;
		((BufferCtrlObj&)m_buffer).getFrameDim(frame_dim);
		bool continueFlag = true;
		//if (((BufferCtrlObj&)m_buffer).getBufferMgr()!=NULL)
		{
			HwFrameInfoType hw_frame_info(nextFrameId,&data,&frame_dim,Timestamp::now(),0,HwFrameInfoType::Transfer);
			continueFlag = (((BufferCtrlObj&)m_buffer).getBufferCbMgr()).newFrameReady(hw_frame_info);
		}
		m_last_image_read = nextFrameId;
		********************************************/
      }
      break;
      //-----------------------------------------------------    
      case PILATUS_START_MSG:	
      {
		DEB_TRACE() << "Reader::->PILATUS_START_MSG";
		enable_periodic_msg(true);
      }
      break;
      //-----------------------------------------------------
      case PILATUS_STOP_MSG:
      {
		DEB_TRACE() << "Reader::->PILATUS_STOP_MSG";
		if(!m_stop_already_done)
			m_stop_already_done = true;
		enable_periodic_msg(false);
      }
      break;
      //-----------------------------------------------------
      case PILATUS_RESET_MSG:
      {
		DEB_TRACE() << "Reader::->PILATUS_RESET_MSG";		
      }
      break;	
      //-----------------------------------------------------
    }
  }
  catch( yat::Exception& ex )
  {
    DEB_ERROR() <<"Error : " << ex.errors[0].desc;
    throw;
  }

}

//-----------------------------------------------------
