//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
#include <cbf.h>
#include <cbf_simple.h>
#include "PilatusSaving.h"
#include "PilatusCamera.h"

using namespace lima;
using namespace lima::Pilatus;

SavingCtrlObj::SavingCtrlObj(Camera& camera) : 
  HwSavingCtrlObj(HwSavingCtrlObj::COMMON_HEADER|
		  HwSavingCtrlObj::MANUAL_READ),
  m_cam(camera)
{
}

SavingCtrlObj::~SavingCtrlObj()
{
}

void SavingCtrlObj::getPossibleSaveFormat(std::list<std::string> &format_list) const
{
  format_list.push_back(HwSavingCtrlObj::CBF_FORMAT_STR);
}

void SavingCtrlObj::readFrame(HwFrameInfoType &frame_info,int frame_nr)
{
  DEB_MEMBER_FUNCT();
  FrameDim anImageDim;
  std::string errmsg;
  std::string fullPath = _getFullPath(frame_nr);
  FILE* fd = fopen(fullPath.c_str(),"r");
  if(!fd)
    THROW_HW_ERROR(Error) << "File : " << fullPath << " doesn't exist";

  cbf_handle handle;
  if(cbf_make_handle(&handle))
    {
      errmsg = "Can't create CBF handler";
      goto closefile;
    }
  
  if(cbf_read_file (handle,fd,MSG_NODIGEST))
    {
      errmsg = "Can't read file : " + fullPath;
      goto closehandle;
    }
  
  size_t height,width;
  if(cbf_get_image_size(handle,0,1,&height,&width))
    {
      errmsg = "Can't get image size";
      goto closehandle;
    }
   
  void *aDataBuffer;
  if(posix_memalign(&aDataBuffer,16,height * width * sizeof(int)))
    {
      errmsg = "Can't allocate memory";
      goto freearray;
    }
  
  if(cbf_get_image(handle,0,1,aDataBuffer,sizeof(int),1,height,width))
    {
      errmsg = "Can't get image";
      goto freearray;
    }

  anImageDim = FrameDim(width,height,Bpp32S);
  frame_info = HwFrameInfoType(frame_nr,aDataBuffer,&anImageDim,
			       Timestamp(),0,
			       HwFrameInfoType::Shared);
  goto closehandle;

 freearray:
  free(aDataBuffer);
 closehandle:
  cbf_free_handle(handle);
 closefile:
  fclose(fd);
  if(!errmsg.empty())
    THROW_HW_ERROR(Error) << errmsg;
}

void SavingCtrlObj::setCommonHeader(const HeaderMap &header)
{
  DEB_MEMBER_FUNCT();

  std::string cmd = "mxsettings ";
  for(HeaderMap::const_iterator i = header.begin();
      i != header.end();++i)
    {

      cmd += i->first;
      cmd += ' ';
      cmd += i->second;
    }
  std::string errorMessage = m_cam.sendAnyCommandAndGetErrorMsg(cmd);
  if(!errorMessage.empty())
    THROW_HW_ERROR(lima::Error) << errorMessage;
}

void SavingCtrlObj::_prepare()
{
  DEB_MEMBER_FUNCT();

  if(m_suffix != ".cbf")
    THROW_HW_ERROR(lima::Error) << "Suffix must be .cbf";

  m_cam.setImgpath(m_directory);

  char number[16];
  snprintf(number,sizeof(number),m_index_format.c_str(),m_next_number);
  std::string filename = m_prefix;
  filename += number;
  filename += m_suffix;
  m_cam.setFileName(filename);
}

