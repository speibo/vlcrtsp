#include "ipc_core.h"
#include "videostream.h"
#include <map>
#include <thread>
#include <unistd.h>
#include <exception>
#include <assert.h>
#include <mutex>

typedef std::map<std::string, VVideoStream> VideoStreamMap;
VideoStreamMap* g_pMapCameras;
std::mutex g_lock;
IPCCoreWork* IPCCoreWork::s_pIPCCorework = NULL;


IPCCoreWork::IPCCoreWork()
{

}

IPCCoreWork::~IPCCoreWork()
{

}



/************************************************************************
**函数：threadIPCOnline
**功能：监测IPC状态
**参数：
        [in]  ip            - IPC的IP
        [in]  username      - 有户名
        [in]  password      - 密码
        [out] channel       - 码流
        [in]  ipc_info      - 获取的IPC会保存在此ipc_info_t结构体中
**返回：
        0成功，非0失败
**备注：
        
************************************************************************/
void IPCCoreWork::threadIPCOnline()
{
    while(1)
    {  
		VideoStreamMap::iterator itr = g_pMapCameras->begin();
        for( ; ; )
		{
			if(g_lock.try_lock() == false)  //可以保证在对map就行删除、添加操作时。退出遍历
			{
				gdbprintf(":---> skip ipcameras check\n");
				break;
			}
			if(itr != g_pMapCameras->end()) 
			{
			    //std::cout << itr->first << "\n";
				VVideoStream* video_stream = &(*g_pMapCameras)[itr->first];
				switch(video_stream->m_pVideoStream.state)
				{
					case VIDEO_PLAY:
						video_stream->Open(); break;
					case VIDEO_CLOSE:
						video_stream->Close(); break;
					case VIDEO_EXCEPTION:
						std::cout << itr->first << ", will reopen." << "\n";
						video_stream->ReOpen(); break;
					case VIDEO_MEDIA_ERROR:
					case VIDEO_STOP:
					case VIDEO_SLEEP:
					case VIDEO_AVTICE:
					default:
						break;
				}
			}
			else {
				g_lock.unlock();
				break;
			}
			++itr;
			g_lock.unlock();
			usleep(50000);
        }
		sleep(5);
    }
}

/*
 * 加载摄像机信息，并调用onvif搜索设备rtsp协议
 *
 */

void IPCCoreWork::loadAllIPCInfo()
{
	std::string test_ip[2] = {"10.0.63.200", "10.0.97.102"};
	for(int i= 0; i < 2; i++) {
		VVideoStream video_stream;
		video_stream.GetStreamUri(test_ip[i].c_str(), "admin", "wavecamera1", 0);
		video_stream.SetTask((unsigned char*)"000010001");
		(*g_pMapCameras)[test_ip[i]] = video_stream; 
	}
}


void IPCCoreWork::Init()
{
	g_pMapCameras = new VideoStreamMap;
    this->loadAllIPCInfo();
    std::thread threadOnline(&IPCCoreWork::threadIPCOnline, this);
    threadOnline.detach();
}

cv::Mat IPCCoreWork::GetFrame(const std::string ipc_id, int task_id)
{
	cv::Mat pframe;
	//std::lock_guard<std::mutex> lock(g_lock); //这里不加锁，如果加锁只能顺序遍历，不加锁可以随机遍历
  	try 
    {
    	if((&g_pMapCameras->at(ipc_id))->State() ==  VIDEO_AVTICE) {
			pframe = (&g_pMapCameras->at(ipc_id))->GetFrameData(task_id);
    	}
    }
  	catch(...) 
	{
		
  	}
	return pframe;
}

int IPCCoreWork::AddIPCamera(const char *ip, const char* username, const char* password, int channel)
{
	VVideoStream video_stream;
	if(video_stream.GetStreamUri(ip,  username, password, channel) == VIDEO_MEDIA_ERROR) 
	{
		std::cout << ip << ": GetStreamUri failed\n";
		return -1;
	}
	video_stream.m_pVideoStream.state = VIDEO_SLEEP;
	std::lock_guard<std::mutex> lock(g_lock); //门锁,变量销毁时自动解锁, 通知IPCOnline线程退出遍历
	(*g_pMapCameras)[ip] = video_stream;
	return 0;
}




 