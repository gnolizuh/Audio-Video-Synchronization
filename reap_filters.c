int reap_filters(void)
{
    AVFrame *filtered_frame = NULL;
    int i;
    int64_t frame_pts;

    /* Reap all buffers present in the buffer sinks */
    for (i = 0; i < nb_output_streams; i++) {
        OutputStream *ost = output_streams[i];
        OutputFile    *of = output_files[ost->file_index];
        AVFilterContext *filter;
        AVCodecContext *enc = ost->enc_ctx;
        int ret = 0;

        if (!ost->filter)
            continue;
        filter = ost->filter->filter;

        if (!ost->filtered_frame && !(ost->filtered_frame = av_frame_alloc())) {
            return AVERROR(ENOMEM);
        }
        filtered_frame = ost->filtered_frame;

        while (1) {
            ret = av_buffersink_get_frame_flags(filter, filtered_frame,
                                               AV_BUFFERSINK_FLAG_NO_REQUEST);
            if (ret < 0) {
                if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                    av_log(NULL, AV_LOG_WARNING,
                           "Error in av_buffersink_get_frame_flags(): %s\n", av_err2str(ret));
                }
                break;
            }
            if (ost->finished) {
                av_frame_unref(filtered_frame);
                continue;
            }
            frame_pts = AV_NOPTS_VALUE;
            if (filtered_frame->pts != AV_NOPTS_VALUE) {
                int64_t start_time = (of->start_time == AV_NOPTS_VALUE) ? 0 : of->start_time;
				// ****<<  相对时间戳转成编码器时间戳
                filtered_frame->pts = frame_pts =
                    av_rescale_q(filtered_frame->pts, filter->inputs[0]->time_base, enc->time_base) -
                    av_rescale_q(start_time, AV_TIME_BASE_Q, enc->time_base);
            }
            //if (ost->source_index >= 0)
            //    *filtered_frame= *input_streams[ost->source_index]->decoded_frame; //for me_threshold

            switch (filter->inputs[0]->type) {
            case AVMEDIA_TYPE_VIDEO:
                filtered_frame->pts = frame_pts;
                if (!ost->frame_aspect_ratio.num)
                    enc->sample_aspect_ratio = filtered_frame->sample_aspect_ratio;

                if (debug_ts) {
                    av_log(NULL, AV_LOG_INFO, "filter -> pts:%s pts_time:%s time_base:%d/%d\n",
                            av_ts2str(filtered_frame->pts), av_ts2timestr(filtered_frame->pts, &enc->time_base),
                            enc->time_base.num, enc->time_base.den);
                }

                do_video_out(of->ctx, ost, filtered_frame);
                break;
            case AVMEDIA_TYPE_AUDIO:
                filtered_frame->pts = frame_pts;
                if (!(enc->codec->capabilities & CODEC_CAP_PARAM_CHANGE) &&
                    enc->channels != av_frame_get_channels(filtered_frame)) {
                    av_log(NULL, AV_LOG_ERROR,
                           "Audio filter graph output is not normalized and encoder does not support parameter changes\n");
                    break;
                }
                do_audio_out(of->ctx, ost, filtered_frame);
                break;
            default:
                // TODO support subtitle filters
                av_assert0(0);
            }

            av_frame_unref(filtered_frame);
        }
    }

    return 0;
}