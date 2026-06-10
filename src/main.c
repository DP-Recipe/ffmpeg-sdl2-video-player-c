    #include <stdio.h>
    #include<stdint.h>
    #include <libavformat/avformat.h>
    #include<libavcodec/avcodec.h>
    #include<libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libswresample/swresample.h>
    #include <libavutil/samplefmt.h>
    #include<SDL2/SDL.h>
    #include <math.h>

    int main(int argc, char *argv[])
    {
        SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
        SDL_Window* window = SDL_CreateWindow("My Video Player",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,800,600,0 );
        SDL_Renderer* renderer = SDL_CreateRenderer(window,-1,0);

        printf("START\n");

        AVFormatContext *format = NULL;

        int result = avformat_open_input(&format,"test2.mp4",NULL,NULL );


        if(result != 0)
        {
            printf("Could not open video\n");
            getchar();
            return 1;
        }


        int videostream = -1;
        int audiostream = -1;

        if(avformat_find_stream_info(format,NULL)<0){
            printf("Could not find any streams\n");
            return 1;
        }

        for(int i=0;i<format->nb_streams;i++){
            if(format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
                videostream = i;
            }
            if(format->streams[i]->codecpar->codec_type== AVMEDIA_TYPE_AUDIO){
                audiostream = i;
            }
        }
        AVRational timebase = format->streams[videostream]->time_base;
        AVRational fps = format->streams[videostream]->avg_frame_rate;

        double r_fps = (double)fps.num/fps.den;
        printf("The fps is %.2f\n",r_fps);
        int frame_delay = (int)1000/r_fps;
        double timesec = (double)format->duration/1000000;
        printf("Duration is: %.2f\n",timesec);

        AVCodecParameters *codecpar = format->streams[videostream]->codecpar;
        AVCodecParameters* audio_codecpar = format->streams[audiostream]->codecpar;

        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        const AVCodec* audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
        if(codec == NULL){
            printf("Decoder not found\n");
            return 1;
        }
        if(audio_codec == NULL){
            printf("Decoder not found\n");
            return 1;
        }

        AVCodecContext *codeccontext = avcodec_alloc_context3(codec);
        AVCodecContext* audio_context = avcodec_alloc_context3(audio_codec);

        if (codeccontext == NULL){
            printf("The allocation is not done properly\n");
            return 1;
        }
        printf("The video allocation is done successfully.\n");
        if(avcodec_parameters_to_context(codeccontext,codecpar)<0){
            printf("Could not copy. \n");
            return 1;
        }
        if(avcodec_open2(codeccontext,codec,NULL)<0){
            printf("Decoder not opened.\n");
            return 1;
        }
        if (audio_context == NULL){
            printf("The allocation is not done properly\n");
            return 1;
        }
        printf("The audio allocation is done successfully.\n");
        if(avcodec_parameters_to_context(audio_context,audio_codecpar)<0){
            printf("Could not copy. \n");
            return 1;
        }
        if(avcodec_open2(audio_context,audio_codec,NULL)<0){
            printf("Decoder not opened.\n");
            return 1;
        }
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVPacket *audio_packet = av_packet_alloc();
        AVFrame *audio_frame = av_frame_alloc();

        SwrContext *swr = NULL;
        uint8_t *audio_buffer = NULL;
        SDL_AudioDeviceID device = 0;
        int bytes = 0;

        if(frame == NULL || packet == NULL){
            printf("Video Allocation failed\n");
            return 1;
        }
        if(audio_frame == NULL || audio_packet == NULL){
            printf("Audio Allocation failed\n");
            return 1;
        }

        int decode = 0;
        int audio_decode = 0;

        while(av_read_frame(format,packet)>=0){
            if(packet->stream_index == videostream){
                avcodec_send_packet(codeccontext,packet);
                if(avcodec_receive_frame(codeccontext,frame) == 0){
                decode = 1;
                break;
                }
            }
            

        }
        
        if(!decode){
            printf("Video not decoded\n");
            return 1;
        }
        struct SwsContext* sws = sws_getContext(
            frame->width,
            frame->height,
            frame->format,
            frame->width,
            frame->height,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );
        uint8_t *rgbbuffer = malloc(frame->height*frame->width*3);
        AVFrame* rgbframe = av_frame_alloc();
        rgbframe->format = AV_PIX_FMT_RGB24;
        rgbframe->width = frame->width;
        rgbframe->height = frame->height;
        av_image_fill_arrays(
            rgbframe->data,
            rgbframe->linesize,
            rgbbuffer,
            AV_PIX_FMT_RGB24,
            frame->width,
            frame->height,
            1

        );
        SDL_Texture* texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STATIC,
            rgbframe->width,
            rgbframe->height
        );
        if(audio_context){
            
            swr_alloc_set_opts2(&swr,&audio_context->ch_layout, AV_SAMPLE_FMT_FLT, audio_context->sample_rate,
            &audio_context->ch_layout, audio_context->sample_fmt, audio_context->sample_rate,
            0, NULL);
            if(swr_init(swr) < 0)
            {
                printf("Resampler init failed\n");
                return 1;
            }
            bytes = 8192*audio_context->ch_layout.nb_channels*sizeof(float);
            audio_buffer = malloc(bytes);

            SDL_AudioSpec wanted;
            SDL_zero(wanted);

            wanted.freq = audio_context->sample_rate;
            wanted.channels = audio_context->ch_layout.nb_channels;
            wanted.format = AUDIO_F32;
            wanted.samples = 1024;
            wanted.callback = NULL;
            device = SDL_OpenAudioDevice(NULL,0,&wanted,NULL,0);
            if(device == 0)
            {
                printf("Audio device failed\n");
            }else{
                SDL_PauseAudioDevice(device,0);
            }

        }else{
            printf("No audio frame decoded\n");
        }

        int running = 1;
        int pause = 0;
        SDL_Event event;
        double c_time = 0;
        double speed = 1.0;
        while(running)
        {
            while(SDL_PollEvent(&event))
            {
                if(event.type == SDL_QUIT)
                {
                    running = 0;
                }
                if(event.type == SDL_KEYDOWN){
                    if(event.key.keysym.sym == SDLK_SPACE){
                        pause = !pause;
                        printf("Space is pressed\n");
                    }
                    if(event.key.keysym.sym == SDLK_RIGHT){
                        c_time = frame->pts*av_q2d(timebase);
                        double t_time = c_time + 1.0;

                        if(t_time>timesec){
                            t_time = timesec;
                        }
                        int64_t targetpts = t_time/av_q2d(timebase);
                        if(av_seek_frame(
                            format,
                            videostream,
                            targetpts,
                            AVSEEK_FLAG_BACKWARD
                        )>=0){
                            avcodec_flush_buffers(codeccontext);
                        }

                    }
                    if(event.key.keysym.sym == SDLK_LEFT){
                        c_time = frame->pts*av_q2d(timebase);
                        double t_time = c_time - 1.0;

                        if(t_time<0.0){
                            t_time = 0.0;
                        }
                        int64_t targetpts = t_time/av_q2d(timebase);
                        if(av_seek_frame(
                            format,
                            videostream,
                            targetpts,
                            AVSEEK_FLAG_BACKWARD
                        )>=0){
                            avcodec_flush_buffers(codeccontext);
                        }

                    }
                    if(event.key.keysym.sym == SDLK_1){
                        speed = 0.5;
                    }
                    if(event.key.keysym.sym == SDLK_2){
                        speed = 1.0;
                    }
                    if(event.key.keysym.sym == SDLK_3){
                        speed = 2.0;
                    }
                    if(event.key.keysym.sym == SDLK_ESCAPE){
                        running = 0;
                    }
                }
            }
            if(!pause){
                if(av_read_frame(format,packet)>=0){
                    if(packet->stream_index == videostream){
                        
                        c_time = frame->pts*av_q2d(timebase);
                        printf("Time: %.2f / %.2f\r",c_time,timesec);
                        fflush(stdout);
                        avcodec_send_packet(codeccontext,packet);   
                        
                        if(avcodec_receive_frame(codeccontext,frame) == 0){
                            sws_scale(
                            sws,
                            (const uint8_t* const*)frame->data,
                            frame->linesize,
                            0,
                            frame->height,
                            rgbframe->data,
                            rgbframe->linesize
                            );
                
                        SDL_UpdateTexture(texture,NULL,rgbframe->data[0],rgbframe->linesize[0]);
                        SDL_RenderClear(renderer);
                        SDL_RenderCopy(renderer,texture,NULL,NULL);
                        SDL_RenderPresent(renderer);
                        SDL_Delay(frame_delay/speed);
                        }

                    }
                    if(packet->stream_index == audiostream && audio_context && swr && device)
                    {
                        avcodec_send_packet(audio_context, packet);

                        if(avcodec_receive_frame(audio_context,audio_frame) == 0)
                        {
                            int out_sample = swr_convert(swr,&audio_buffer,audio_frame->nb_samples,(const uint8_t **)audio_frame->data,audio_frame->nb_samples);
                            int limit = out_sample*audio_context->ch_layout.nb_channels*sizeof(float);
                            SDL_QueueAudio(device,audio_buffer,limit);
                        }
                    }
                    av_packet_unref(packet);
                }else{
                running =0;
                }
            }
        
        } 
        free(rgbbuffer);
        free(audio_buffer); 

        av_frame_free(&frame);
        av_frame_free(&audio_frame);
        av_frame_free(&rgbframe);
        
        av_packet_free(&packet);

        avcodec_free_context(&codeccontext);
        avcodec_free_context(&audio_context);

        sws_freeContext(sws);
        swr_free(&swr);

        avformat_close_input(&format);

        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        SDL_CloseAudioDevice(device);

        SDL_Quit();
    }
