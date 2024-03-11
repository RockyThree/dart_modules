#include <map>
#include "VideoDecoder.h"
class VideoDecoderManager
{
public:
	VideoDecoderManager() = default;
	~VideoDecoderManager()
	{
		for (auto &pair : decoders)
		{
			delete pair.second;
		}
	}

	VideoDecoder *getDecoder(const std::string &inputFilePath)
	{
		auto it = decoders.find(inputFilePath);
		if (it != decoders.end())
		{
			return it->second;
		}
		else
		{
			VideoDecoder *decoder = new VideoDecoder(inputFilePath);
			if (decoder->initialize())
			{
				decoders[inputFilePath] = decoder;
				return decoder;
			}
			else
			{
				std::cerr << "VideoDecoder 初始化失败" << std::endl;
				delete decoder;
				return nullptr;
			}
		}
	}

	void releaseDecoder(const std::string &inputFilePath)
	{
		auto it = decoders.find(inputFilePath);
		if (it != decoders.end())
		{
			delete it->second;
			decoders.erase(it);
		}
	}

private:
	std::map<std::string, VideoDecoder *> decoders;
};