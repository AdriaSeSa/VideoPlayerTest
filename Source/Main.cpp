
#include "libavcodec/avcodec.h"  
#include "libavformat/avformat.h"  
#include "libswscale/swscale.h"  
#include "libavutil/imgutils.h"  
#include "SDL.h"  

#include <memory>

int main(int argc, char** argv)
{
	// Run code to play a video using OpenCV
	//OpenCVVideoPlayer();

	// Initalizing to NULL prevents segfaults!
	AVFormatContext* pFormatCtx = NULL;
	// Open video file
	if (avformat_open_input(&pFormatCtx, "video.mp4", NULL, NULL) != 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return -1; // Couldn't open file
	}
	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, "1=========================>test.mp4", 0);
	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return -1; // Couldn't find stream information
	}
	// Contraste El �ltimo volcado puede ver la informaci�n que llena la transmisi�n en AVFFormatContext a trav�s de la funci�n AVFORMAT_FIND_STREAM_INFO
	av_dump_format(pFormatCtx, 0, "2=========================>test.mp4", 0);

	const AVCodec* pDec = NULL;
	/* select the video stream and find the decoder*/
	int video_stream_index = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pDec, 0);
	if (video_stream_index < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return -1;
	}

	//Note that we must not use the AVCodecContext from the video stream directly! 
	//So we have to use avcodec_copy_context() to copy the context to a new location (after allocating memory for it, of course).	
	/* create decoding context */
	AVCodecContext* pDecCtx = avcodec_alloc_context3(pDec);
	if (!pDecCtx)
	{
		return AVERROR(ENOMEM);
	}

	//Fill the codec context based on the values from the supplied codec
	avcodec_parameters_to_context(pDecCtx, pFormatCtx->streams[video_stream_index]->codecpar);

	//Initialize the AVCodecContext to use the given AVCodec.
	if (avcodec_open2(pDecCtx, pDec, NULL) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
		return -1;
	}


	AVPacket* packetData = av_packet_alloc();
	//Allocate an AVFrame and set its fields to default values.
	AVFrame* pFrame = av_frame_alloc();
	AVFrame* pFrameYUV = av_frame_alloc();

	// Determine required buffer size and allocate buffer
	int byteSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
		pDecCtx->width,
		pDecCtx->height,
		1
	);
	uint8_t* buffer = (uint8_t*)av_malloc(byteSize);

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,
		buffer, AV_PIX_FMT_YUV420P,
		pDecCtx->width,
		pDecCtx->height,
		1
	);

	int screen_w = pDecCtx->width / 2;
	int screen_h = pDecCtx->height / 2;
	// initialize SWS context for software scaling
	SwsContext* pSws_ctx = sws_getContext(pDecCtx->width,
		pDecCtx->height,
		pDecCtx->pix_fmt,
		screen_w,
		screen_h,
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL
	);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	//SDL 2.0 Support for multiple windows  
	SDL_Window* screen = SDL_CreateWindow("SDL",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,
		SDL_WINDOW_OPENGL
	);

	if (!screen)
	{
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)  
	SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer,
		SDL_PIXELFORMAT_IYUV,
		SDL_TEXTUREACCESS_STREAMING,
		screen_w,
		screen_h
	);
	SDL_Rect sdlRect;
	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	int ret = -1;
	SDL_Event event;

	while (1)
	{
		/**
		*Technically a packet can contain partial frames or other bits of data,
		*but ffmpeg's parser ensures that the packets we get contain either complete or multiple frames.
		*/
		ret = av_read_frame(pFormatCtx, packetData);
		if (ret < 0)
		{
			break;
		}

		if (packetData->stream_index == video_stream_index)
		{
			//Supply raw packet data as input to a decoder.
			ret = avcodec_send_packet(pDecCtx, packetData);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
				break;
			}

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(pDecCtx, pFrame);
				//Return decoded output data from a decoder.
				// Un paquete puede tener m�ltiples marcos
				if (ret >= 0)
				{
					// Convert the image from its native format to YUV
					sws_scale(pSws_ctx,
						(const uint8_t* const*)pFrame->data,
						pFrame->linesize,
						0,
						pDecCtx->height,
						pFrameYUV->data,
						pFrameYUV->linesize
					);

				}
				else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					break;
				}
				else
				{
					av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
					return -1;
				}

				SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
					pFrameYUV->data[0], pFrameYUV->linesize[0],
					pFrameYUV->data[1], pFrameYUV->linesize[1],
					pFrameYUV->data[2], pFrameYUV->linesize[2]
				);
				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
				SDL_RenderPresent(sdlRenderer);
			}

		}
		// Wipe the packet.		
		av_packet_unref(packetData);
		SDL_PollEvent(&event);
		switch (event.type)
		{
		case SDL_QUIT:
			SDL_Quit();
			break;
		default:
			break;
		}

		return 0;
	}
}